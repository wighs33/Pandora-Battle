#include "Mode/WorldBossRaid.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AI/Monster/MonsterCharacter.h"
#include "Character/CharacterBase.h"
#include "Cinematic/CinematicSequenceActor.h"
#include "Common/PdGameplayTags.h"
#include "Component/Match/MatchTravel.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Data/ContentDataSubsystem.h"
#include "Definition/Character/EnemyBaseDefinition.h"
#include "Definition/Level/LevelDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Online/Backend/BackendClientSubsystem.h"
#include "Online/Backend/BackendHttp.h"
#include "Online/Backend/BackendSettings.h"
#include "Online/GameLift/GameLiftServerSubsystem.h"
#include "TimerManager.h"
#include "UI/HUD/PdHUD.h"
#include "UI/HUD/Player/PlayerHudWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldBossRaid)

DEFINE_LOG_CATEGORY_STATIC(LogWorldBossRaid, Log, All);

namespace
{
	/** 한 번의 도전 시간. 백엔드 회차 설정(AttemptSeconds)을 받기 전이나 백엔드 없이 시험할 때 쓴다. */
	constexpr double DefaultAttemptSeconds = 5.0 * 60.0;
	/** 쌓인 딜을 보내는 간격. 다른 서버가 깎은 체력도 이 간격으로 이 월드에 반영된다. */
	constexpr double ReportIntervalSeconds = 10.0;
	/** 도전이 끝나면 HUD가 도전 종료를 보여 준 뒤 타이틀로 보낸다. */
	constexpr double AttemptEndTravelDelaySeconds = 3.0;
	/** 격파되면 드래곤이 쓰러져 사라지는 연출(AMonsterCharacter::DeathDestroyDelay 8초)을 본 뒤 타이틀로 보낸다. */
	constexpr double DefeatTravelDelaySeconds = 10.0;
	constexpr float RaidTickIntervalSeconds = 1.0f;
	/** 클라이언트는 서버보다 조금 늦게 연출을 끝내므로, 서버가 끝낸 뒤에도 연출 액터를 잠시 남겨 마지막 장면을 마저 보게 한다. */
	constexpr float EntranceActorLingerSeconds = 2.0f;
}

AWorldBossRaid::AWorldBossRaid()
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

void AWorldBossRaid::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AWorldBossRaid, BossHealth, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AWorldBossRaid, BossMaxHealth, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AWorldBossRaid, bBossDefeated, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AWorldBossRaid, Attempts, Params);
}

AWorldBossRaid* AWorldBossRaid::Find(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<AWorldBossRaid> It(World);
	return It ? *It : nullptr;
}

// 이미 들어와 있는 플레이어(단독 실행의 로컬 플레이어)부터 지켜보고, 빈 보고로 지금 회차와 전역 체력을 받아 온다.
void AWorldBossRaid::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer)
	{
		ShowBarOnLocalHuds();
	}
	if (!HasAuthority())
	{
		return;
	}

	AttemptSeconds = DefaultAttemptSeconds;
	PostLoginHandle = FGameModeEvents::OnGameModePostLoginEvent().AddUObject(this, &ThisClass::HandlePostLogin);
	LogoutHandle = FGameModeEvents::OnGameModeLogoutEvent().AddUObject(this, &ThisClass::HandleLogout);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* Player = It->Get())
		{
			WatchPlayerReady(Player);
			BeginAttempt(Player->PlayerState);
		}
	}
	GetWorldTimerManager().SetTimer(TickTimerHandle, this, &ThisClass::TickRaid, RaidTickIntervalSeconds, true);
	SendReport();
}

// HUD가 먼저 만들어졌으면 지금 체력바를 붙인다. HUD가 나중에 만들어지면 HUD가 이 액터를 찾아 붙인다.
void AWorldBossRaid::ShowBarOnLocalHuds()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Player = It->Get();
		const APdHUD* Hud = Player && Player->IsLocalController() ? Player->GetHUD<APdHUD>() : nullptr;
		if (UPlayerHudWidget* PlayerHud = Hud ? Cast<UPlayerHudWidget>(Hud->GetPlayerHudWidget()) : nullptr)
		{
			PlayerHud->ShowWorldBossBar(*this);
		}
	}
}

void AWorldBossRaid::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FGameModeEvents::OnGameModePostLoginEvent().Remove(PostLoginHandle);
	FGameModeEvents::OnGameModeLogoutEvent().Remove(LogoutHandle);
	GetWorldTimerManager().ClearTimer(TickTimerHandle);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APdPlayerController* Player = Cast<APdPlayerController>(It->Get()))
		{
			Player->OnClientPlayReady().RemoveAll(this);
		}
	}
	EntranceSequenceLease.Reset();
	if (UAbilitySystemComponent* AbilitySystem = Boss.IsValid() ? Boss->GetAbilitySystemComponent() : nullptr)
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetHealthAttribute()).Remove(BossHealthChangedHandle);
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxHealthAttribute()).Remove(BossMaxHealthChangedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AWorldBossRaid::RegisterBoss(AMonsterCharacter& InBoss)
{
	UAbilitySystemComponent* AbilitySystem = InBoss.GetAbilitySystemComponent();
	if (!HasAuthority() || !AbilitySystem)
	{
		return;
	}

	Boss = &InBoss;
	BossHealthChangedHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetHealthAttribute())
		.AddUObject(this, &ThisClass::HandleBossHealthChanged);
	BossMaxHealthChangedHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &ThisClass::HandleBossMaxHealthChanged);
	if (bGlobalHealthKnown)
	{
		ApplyHealthToBoss();
	}
	BossMaxHealth = AbilitySystem->GetNumericAttribute(UBasicAttributeSet::GetMaxHealthAttribute());
	BossHealth = AbilitySystem->GetNumericAttribute(UBasicAttributeSet::GetHealthAttribute());
	MARK_PROPERTY_DIRTY_FROM_NAME(AWorldBossRaid, BossMaxHealth, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AWorldBossRaid, BossHealth, this);
	// 쓰러진 뒤 다시 나온 보스는 등장 연출 없이 바로 싸운다.
	if (EntranceState == EEntranceState::AwaitingBoss)
	{
		BeginEntrance(InBoss);
	}
}

// 처음 나온 보스는 등장 연출이 끝날 때까지 숨겨 두고 시퀀스를 불러온다. 연출이 없으면 바로 싸운다.
void AWorldBossRaid::BeginEntrance(AMonsterCharacter& InBoss)
{
	const UEnemyBaseDefinition* Definition = InBoss.GetMonsterDefinition();
	EntranceSequence = Definition ? Definition->GetMonsterPresentationSettings().EntranceSequence : TSoftObjectPtr<ULevelSequence>();
	if (EntranceSequence.IsNull())
	{
		FinishEntrance();
		return;
	}
	InBoss.HoldForEntrance();
	EntranceState = EEntranceState::AwaitingPlayer;
	EntranceSequenceLease = UContentDataSubsystem::AcquireContent({EntranceSequence.ToSoftObjectPath()},
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleEntranceSequenceLoaded));
}

void AWorldBossRaid::HandleEntranceSequenceLoaded()
{
	bEntranceSequenceLoadFinished = true;
	if (!EntranceSequence.Get())
	{
		UE_LOG(LogWorldBossRaid, Warning, TEXT("Entrance sequence %s could not be loaded. The world boss starts without it."),
			*EntranceSequence.ToString());
		FinishEntrance();
		return;
	}
	TryPlayEntrance();
}

// 플레이어 화면이 준비되면 등장 연출을 시작할 수 있는지 다시 본다.
void AWorldBossRaid::WatchPlayerReady(APlayerController* Player)
{
	if (APdPlayerController* PdPlayer = Cast<APdPlayerController>(Player); PdPlayer && EntranceState != EEntranceState::Done)
	{
		PdPlayer->OnClientPlayReady().AddUObject(this, &ThisClass::HandleClientPlayReady, TWeakObjectPtr<APdPlayerController>(PdPlayer));
	}
}

// 연출 도중에 화면이 준비된 플레이어도 남은 연출을 보므로 함께 보호한다.
void AWorldBossRaid::HandleClientPlayReady(const TWeakObjectPtr<APdPlayerController> Player)
{
	if (EntranceState == EEntranceState::Playing && Player.IsValid())
	{
		SetWatchingEntrance(*Player, true);
	}
	TryPlayEntrance();
}

// 시퀀스를 다 불러왔고 화면을 볼 플레이어가 하나라도 있으면, 보스가 서 있는 자리를 원점으로 모두에게 재생한다.
void AWorldBossRaid::TryPlayEntrance()
{
	ULevelSequence* Sequence = EntranceSequence.Get();
	AMonsterCharacter* CurrentBoss = Boss.Get();
	if (EntranceState != EEntranceState::AwaitingPlayer || !bEntranceSequenceLoadFinished || !Sequence || !CurrentBoss
		|| !HasPlayReadyPlayer())
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	EntranceActor = GetWorld()->SpawnActor<ACinematicSequenceActor>(
		CurrentBoss->GetActorLocation(), CurrentBoss->GetActorRotation(), SpawnParameters);
	ULevelSequencePlayer* Player = EntranceActor ? EntranceActor->GetSequencePlayer() : nullptr;
	if (!Player)
	{
		FinishEntrance();
		return;
	}
	EntranceState = EEntranceState::Playing;
	EntranceActor->SetSequence(Sequence);
	Player->OnNativeFinished.BindUObject(this, &ThisClass::FinishEntrance);
	Player->Play();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APlayerController* Watcher = It->Get())
		{
			SetWatchingEntrance(*Watcher, true);
		}
	}
}

void AWorldBossRaid::SetWatchingEntrance(const APlayerController& Player, const bool bWatching)
{
	const ACharacterBase* Character = Cast<ACharacterBase>(Player.GetPawn());
	if (UAbilitySystemComponent* AbilitySystem = Character ? Character->GetAbilitySystemComponent() : nullptr)
	{
		AbilitySystem->SetLooseGameplayTagCount(PdGameplayTags::State_Cinematic, bWatching ? 1 : 0);
	}
}

// 연출이 끝나면(또는 연출이 없으면) 보스를 내보내고, 그때까지 들어온 플레이어의 도전을 함께 시작한다.
// 연출을 본 플레이어는 연출 카메라에서 자기 카메라로 돌아올 때 보스 쪽을 보게 한다. 높낮이는 플레이어가 두던 그대로 둔다.
void AWorldBossRaid::FinishEntrance()
{
	if (EntranceState == EEntranceState::Done)
	{
		return;
	}
	const bool bEntrancePlayed = EntranceState == EEntranceState::Playing;
	EntranceState = EEntranceState::Done;
	EntranceSequenceLease.Reset();
	AMonsterCharacter* CurrentBoss = Boss.Get();
	if (CurrentBoss)
	{
		CurrentBoss->ReleaseFromEntrance();
	}
	if (EntranceActor)
	{
		EntranceActor->SetLifeSpan(EntranceActorLingerSeconds);
		EntranceActor = nullptr;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Player = It->Get();
		if (!Player)
		{
			continue;
		}
		const APawn* Pawn = Player->GetPawn();
		if (bEntrancePlayed)
		{
			SetWatchingEntrance(*Player, false);
		}
		if (bEntrancePlayed && CurrentBoss && Pawn)
		{
			const FVector ToBoss = CurrentBoss->GetActorLocation() - Pawn->GetActorLocation();
			Player->ClientSetRotation(FRotator(Player->GetControlRotation().Pitch, ToBoss.Rotation().Yaw, 0.0));
		}
		BeginAttempt(Player->PlayerState);
	}
}

bool AWorldBossRaid::HasPlayReadyPlayer() const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APdPlayerController* Player = Cast<APdPlayerController>(It->Get());
		if (Player && Player->IsClientPlayReady())
		{
			return true;
		}
	}
	return false;
}

// 도전 중인 플레이어의 딜만 순위에 넣는다. 도전이 끝난 뒤(타이틀로 가는 사이)의 딜이나 공격자를 알 수 없는 딜은 전역 체력에만 들어간다.
void AWorldBossRaid::RecordBossDamage(const APlayerState* Attacker, const float Damage)
{
	if (!HasAuthority() || !(Damage > 0.0f))
	{
		return;
	}

	FWorldBossAttempt* Attempt = Attempts.FindByPredicate([Attacker](const FWorldBossAttempt& Candidate)
	{
		return Attacker && Candidate.Player == Attacker && !Candidate.bEnded;
	});
	const APdPlayerState* PdAttacker = Cast<APdPlayerState>(Attacker);
	const FString PlayerId = Attempt && PdAttacker ? PdAttacker->GetBackendPlayerId() : FString();
	FPendingDamage& Pending = PendingDamage.FindOrAdd(PlayerId);
	Pending.Damage += Damage;
	if (Attempt)
	{
		Attempt->Damage += Damage;
		MARK_PROPERTY_DIRTY_FROM_NAME(AWorldBossRaid, Attempts, this);
		Pending.DisplayName = UPlayerMatchComponent::ResolveDisplayName(Attacker).ToString();
	}
}

const FWorldBossAttempt* AWorldBossRaid::FindAttempt(const APlayerState* Player) const
{
	return Player
		? Attempts.FindByPredicate([Player](const FWorldBossAttempt& Attempt) { return Attempt.Player == Player; })
		: nullptr;
}

double AWorldBossRaid::GetAttemptSecondsLeft(const FWorldBossAttempt& Attempt) const
{
	return FMath::Max(0.0, Attempt.EndServerTime - GetServerTime());
}

void AWorldBossRaid::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (GameMode && GameMode->GetWorld() == GetWorld() && NewPlayer)
	{
		WatchPlayerReady(NewPlayer);
		BeginAttempt(NewPlayer->PlayerState);
	}
}

// 나간 플레이어의 딜은 바로 보낸다. 다시 들어오면 백엔드가 도전 횟수를 한 번 더 쓴 새 도전이다.
void AWorldBossRaid::HandleLogout(AGameModeBase* GameMode, AController* Exiting)
{
	if (!GameMode || GameMode->GetWorld() != GetWorld() || !Exiting)
	{
		return;
	}
	const APlayerState* Player = Exiting->PlayerState;
	if (Attempts.RemoveAll([Player](const FWorldBossAttempt& Attempt) { return Attempt.Player == Player; }) > 0)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(AWorldBossRaid, Attempts, this);
	}
	SendReport();
}

void AWorldBossRaid::HandleBossHealthChanged(const FOnAttributeChangeData& Data)
{
	BossHealth = Data.NewValue;
	MARK_PROPERTY_DIRTY_FROM_NAME(AWorldBossRaid, BossHealth, this);
	// 이 월드에서 쓰러졌으면 전역 체력도 0 이하다. 다음 보고 간격을 기다리지 않고 마지막 일격을 알린다.
	if (Data.OldValue > 0.0f && Data.NewValue <= 0.0f)
	{
		SendReport();
	}
}

void AWorldBossRaid::HandleBossMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	BossMaxHealth = Data.NewValue;
	MARK_PROPERTY_DIRTY_FROM_NAME(AWorldBossRaid, BossMaxHealth, this);
}

void AWorldBossRaid::OnRep_Attempts()
{
	NoteLocalPlayerAttempts();
}

// 이 화면의 플레이어가 도전을 시작했으면, 타이틀로 돌아갔을 때 결과를 알리도록 백엔드 클라이언트에 남긴다.
void AWorldBossRaid::NoteLocalPlayerAttempts() const
{
	UBackendClientSubsystem* Backend = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBackendClientSubsystem>() : nullptr;
	if (!Backend)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Player = It->Get();
		if (Player && Player->IsLocalController() && FindAttempt(Player->PlayerState))
		{
			Backend->NoteBossRaidPlayed();
		}
	}
}

void AWorldBossRaid::TickRaid()
{
	const double Now = GetServerTime();
	bool bAttemptEnded = false;
	for (FWorldBossAttempt& Attempt : Attempts)
	{
		if (!Attempt.bEnded && Now >= Attempt.EndServerTime)
		{
			Attempt.bEnded = true;
			bAttemptEnded = true;
			MARK_PROPERTY_DIRTY_FROM_NAME(AWorldBossRaid, Attempts, this);
		}
		else if (Attempt.bEnded && !Attempt.bSentToTitle && Now >= Attempt.EndServerTime + AttemptEndTravelDelaySeconds)
		{
			Attempt.bSentToTitle = true;
			SendPlayerToTitle(Attempt.Player);
		}
	}

	// 플레이어가 있는 동안에는 딜이 없어도 물어 다른 서버가 깎은 체력을 반영한다.
	if (bAttemptEnded || (!Attempts.IsEmpty() && Now - LastReportServerTime >= ReportIntervalSeconds))
	{
		SendReport();
	}

	if (bBossDefeated && !bDefeatTravelStarted && Now - DefeatedServerTime >= DefeatTravelDelaySeconds)
	{
		bDefeatTravelStarted = true;
		for (const FWorldBossAttempt& Attempt : Attempts)
		{
			SendPlayerToTitle(Attempt.Player);
		}
		if (UGameLiftServerSubsystem* GameLift = UGameLiftServerSubsystem::Get(this))
		{
			GameLift->RequestSessionEnd(TEXT("World boss defeated"));
		}
	}
}

// 등장 연출이 끝나기 전에 들어온 플레이어의 도전은 연출이 끝날 때(FinishEntrance) 시작한다.
void AWorldBossRaid::BeginAttempt(APlayerState* Player)
{
	if (!Player || FindAttempt(Player) || EntranceState != EEntranceState::Done)
	{
		return;
	}
	FWorldBossAttempt& Attempt = Attempts.AddDefaulted_GetRef();
	Attempt.Player = Player;
	Attempt.EndServerTime = GetServerTime() + AttemptSeconds;
	MARK_PROPERTY_DIRTY_FROM_NAME(AWorldBossRaid, Attempts, this);
	// 단독 실행·리슨 서버의 로컬 플레이어는 복제 알림을 받지 않는다.
	NoteLocalPlayerAttempts();
}

// 한 번에 한 보고만 보낸다. 실패한 보고는 같은 순번과 내용으로 다시 보내고, 그사이 쌓인 딜은 다음 보고로 넘긴다.
void AWorldBossRaid::SendReport()
{
	if (bOffline)
	{
		return;
	}
	if (bReportInFlight)
	{
		bReportQueued = true;
		return;
	}

	const UBackendSettings* Settings = GetDefault<UBackendSettings>();
	const FString Url = Settings->BuildApiUrl(TEXT("/server/raid-damage"));
	const FString ServerKey = Settings->GetServerKey();
	// GameLift 서버 서브시스템은 전용 서버에만 있다. 리슨 서버와 단독 실행은 보고하지 않는다.
	const UGameLiftServerSubsystem* GameLift = UGameLiftServerSubsystem::Get(this);
	if (Url.IsEmpty() || ServerKey.IsEmpty() || !GameLift)
	{
		UE_LOG(LogWorldBossRaid, Log, TEXT("No backend URL or server key. The world boss uses this world's health only."));
		bOffline = true;
		return;
	}
	if (ServerId.IsEmpty())
	{
		// 보스 레이드 세션은 월드가 열린 뒤 활성화되므로, 첫 보고를 만들 때 게임 세션 ID를 읽는다.
		ServerId = GameLift->IsGameLiftActive() && !GameLift->GetGameSessionId().IsEmpty()
			? GameLift->GetGameSessionId()
			: FString::Printf(TEXT("local-%s"), *FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	}

	if (!UnsentReport.IsSet())
	{
		// 백엔드는 정수 딜만 받는다. 소수점 아래는 다음 보고로 넘긴다.
		FReport Report;
		Report.Seq = ++LastSeq;
		for (TPair<FString, FPendingDamage>& Pair : PendingDamage)
		{
			const double WholeDamage = FMath::FloorToDouble(Pair.Value.Damage);
			if (WholeDamage >= 1.0)
			{
				Pair.Value.Damage -= WholeDamage;
				Report.Players.Emplace(Pair.Key, FPendingDamage{Pair.Value.DisplayName, WholeDamage});
			}
		}
		UnsentReport = MoveTemp(Report);
	}

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("raidId"), RaidId);
	Body->SetStringField(TEXT("serverId"), ServerId);
	Body->SetNumberField(TEXT("seq"), UnsentReport->Seq);
	TArray<TSharedPtr<FJsonValue>> Players;
	for (const TPair<FString, FPendingDamage>& Pair : UnsentReport->Players)
	{
		const TSharedRef<FJsonObject> Player = MakeShared<FJsonObject>();
		Player->SetStringField(TEXT("playerId"), Pair.Key);
		Player->SetStringField(TEXT("displayName"), Pair.Value.DisplayName);
		Player->SetNumberField(TEXT("damage"), Pair.Value.Damage);
		Players.Add(MakeShared<FJsonValueObject>(Player));
	}
	Body->SetArrayField(TEXT("players"), Players);

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = BackendHttp::CreateJsonRequest(
		TEXT("POST"), Url, Body, Settings->GetServerReportTimeoutSeconds());
	Request->SetHeader(BackendHttp::ServerKeyHeader, ServerKey);
	bReportInFlight = true;
	LastReportServerTime = GetServerTime();
	BackendHttp::Send(Request, BackendHttp::FOnResponse::CreateUObject(this, &ThisClass::HandleReportResponse));
}

void AWorldBossRaid::HandleReportResponse(const BackendHttp::FResponse& Response)
{
	bReportInFlight = false;
	if (!Response.bSucceeded || !Response.Json.IsValid())
	{
		if (Response.StatusCode == 404)
		{
			// 열린 회차가 없다. 이 서버가 끝날 때까지 이 월드의 체력으로만 싸운다.
			UE_LOG(LogWorldBossRaid, Warning, TEXT("No boss raid is open on the backend. The world boss uses this world's health only."));
			bOffline = true;
			UnsentReport.Reset();
			return;
		}
		if (Response.StatusCode == 400)
		{
			// 형식이 틀린 보고는 다시 보내도 같은 이유로 거절된다.
			UE_LOG(LogWorldBossRaid, Error, TEXT("Raid damage report was rejected: %s"), *Response.ErrorMessage);
			UnsentReport.Reset();
			return;
		}
		UE_LOG(LogWorldBossRaid, Warning, TEXT("Raid damage report failed (HTTP %d %s). It will be sent again."),
			Response.StatusCode, *Response.ErrorCode);
		return;
	}

	UnsentReport.Reset();
	Response.Json->TryGetStringField(TEXT("raidId"), RaidId);
	double Health = 0.0;
	double MaxHealth = 0.0;
	double ResponseAttemptSeconds = 0.0;
	bool bDefeated = false;
	Response.Json->TryGetNumberField(TEXT("hp"), Health);
	Response.Json->TryGetNumberField(TEXT("maxHp"), MaxHealth);
	Response.Json->TryGetBoolField(TEXT("defeated"), bDefeated);
	if (Response.Json->TryGetNumberField(TEXT("attemptSeconds"), ResponseAttemptSeconds) && ResponseAttemptSeconds > 0.0)
	{
		AttemptSeconds = ResponseAttemptSeconds;
	}
	ApplyGlobalHealth(Health, MaxHealth, bDefeated);

	if (bReportQueued)
	{
		bReportQueued = false;
		SendReport();
	}
}

void AWorldBossRaid::ApplyGlobalHealth(const double Health, const double MaxHealth, const bool bDefeated)
{
	if (MaxHealth <= 0.0)
	{
		return;
	}
	if (!bGlobalHealthKnown)
	{
		UE_LOG(LogWorldBossRaid, Log, TEXT("World boss raid %s: %.0f / %.0f health, %.0f s per attempt."),
			*RaidId, Health, MaxHealth, AttemptSeconds);
	}
	bGlobalHealthKnown = true;
	GlobalHealth = Health;
	GlobalMaxHealth = MaxHealth;
	ApplyHealthToBoss();
	if (bDefeated && !bBossDefeated)
	{
		UE_LOG(LogWorldBossRaid, Log, TEXT("World boss raid %s was defeated."), *RaidId);
		bBossDefeated = true;
		MARK_PROPERTY_DIRTY_FROM_NAME(AWorldBossRaid, bBossDefeated, this);
		DefeatedServerTime = GetServerTime();
	}
}

// 응답의 전역 체력에는 이 서버가 그 뒤에 넣은 딜(아직 보내지 않은 딜)이 빠져 있으므로 그만큼 빼서 적용한다.
// 체력이 0이 되면 기존 사망 흐름(사망 능력, 연출)이 그대로 돈다.
void AWorldBossRaid::ApplyHealthToBoss()
{
	AMonsterCharacter* CurrentBoss = Boss.Get();
	UAbilitySystemComponent* AbilitySystem = CurrentBoss ? CurrentBoss->GetAbilitySystemComponent() : nullptr;
	if (!AbilitySystem || CurrentBoss->IsDead())
	{
		return;
	}
	const double Health = FMath::Max(0.0, GlobalHealth - GetUnreportedDamage());
	// 최대 체력을 먼저 바꿔야 새 체력이 이전 최대 체력에 잘리지 않는다.
	AbilitySystem->SetNumericAttributeBase(UBasicAttributeSet::GetMaxHealthAttribute(), GlobalMaxHealth);
	AbilitySystem->SetNumericAttributeBase(UBasicAttributeSet::GetHealthAttribute(), Health);
}

void AWorldBossRaid::SendPlayerToTitle(APlayerState* Player) const
{
	APlayerController* Controller = Player ? Cast<APlayerController>(Player->GetOwningController()) : nullptr;
	const ULevelDefinition* Levels = ULevelDefinition::ResolveDefaultDefinition();
	if (Controller && Levels)
	{
		MatchTravel::SendPlayerToTitle(*Controller, Levels->GetTitleTravelMapName());
	}
}

double AWorldBossRaid::GetServerTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

double AWorldBossRaid::GetUnreportedDamage() const
{
	double Damage = 0.0;
	for (const TPair<FString, FPendingDamage>& Pair : PendingDamage)
	{
		Damage += Pair.Value.Damage;
	}
	return Damage;
}
