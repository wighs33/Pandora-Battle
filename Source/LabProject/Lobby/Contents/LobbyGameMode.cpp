#include "Lobby/Contents/LobbyGameMode.h"

#include "Character/PdPlayer.h"
#include "Component/Lobby/LobbyConfigurationComponent.h"
#include "Component/Experience/ExperienceManagerComponent.h"
#include "Definition/Experience/ExperienceDefinition.h"
#include "Experience/PdWorldSettings.h"
#include "Component/Lobby/LobbyPlayerStateComponent.h"
#include "GameFramework/GameSession.h"
#include "Component/Lobby/LobbyPlayerSetupComponent.h"
#include "Component/Player/PlayerSpawnComponent.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Lobby/Contents/LobbyHUD.h"
#include "Lobby/Contents/LobbyPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Lobby/Coordination/LobbyTravelCoordinator.h"
#include "Mode/PdPlayerController.h"
#include "Provision/DefaultPlayerProvisioner.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyGameMode)

DEFINE_LOG_CATEGORY_STATIC(LogLobbyGameMode, Log, All);

#if WITH_EDITOR
EDataValidationResult ALobbyGameMode::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const auto ValidateClass = [&Context, &Result](const UClass* Class, const UClass* RequiredClass, const TCHAR* PropertyName)
	{
		if (!Class || !Class->IsChildOf(RequiredClass))
		{
			Context.AddError(FText::Format(NSLOCTEXT("LobbyGameMode", "InvalidFrameworkClass", "{0} must inherit from {1}."),
				FText::FromString(PropertyName), FText::FromString(RequiredClass->GetName())));
			Result = EDataValidationResult::Invalid;
		}
	};
	ValidateClass(PlayerControllerClass, ALobbyPlayerController::StaticClass(), TEXT("PlayerControllerClass"));
	ValidateClass(GameStateClass, ALobbyGameState::StaticClass(), TEXT("GameStateClass"));
	ValidateClass(PlayerStateClass, APdPlayerState::StaticClass(), TEXT("PlayerStateClass"));
	ValidateClass(HUDClass, ALobbyHUD::StaticClass(), TEXT("HUDClass"));
	ValidateClass(DefaultPawnClass, APdPlayer::StaticClass(), TEXT("DefaultPawnClass"));
	return Result == EDataValidationResult::Invalid ? Result : EDataValidationResult::Valid;
}
#endif

// 로비의 필수 처리 객체와 기본 프레임워크 클래스를 구성한다.
ALobbyGameMode::ALobbyGameMode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	LobbyConfigurationComponent = CreateDefaultSubobject<ULobbyConfigurationComponent>(TEXT("LobbyConfigurationComponent"));
	LobbyPlayerSetupComponent = CreateDefaultSubobject<ULobbyPlayerSetupComponent>(TEXT("LobbyPlayerCoordinatorComponent"));
	SpawnComponent = CreateDefaultSubobject<UPlayerSpawnComponent>(TEXT("LobbyRespawnComponent"));
	DefaultPlayerProvisioner = CreateDefaultSubobject<UDefaultPlayerProvisioner>(TEXT("DefaultPlayerProvisioner"));
	TravelCoordinator = CreateDefaultSubobject<ULobbyTravelCoordinator>(TEXT("LobbyTravelCoordinator"));

	PlayerControllerClass = ALobbyPlayerController::StaticClass();
	GameStateClass = ALobbyGameState::StaticClass();
	PlayerStateClass = APdPlayerState::StaticClass();
	HUDClass = ALobbyHUD::StaticClass();
	DefaultPawnClass = APdPlayer::StaticClass();
	bUseSeamlessTravel = true;
}

// 로비 설정을 준비한 뒤 선택한 맵과 서버의 세션 광고 정보를 맞춘다.
void ALobbyGameMode::BeginPlay()
{
	Super::BeginPlay();
	SpawnComponent->OnPlayerRespawned.AddUObject(this, &ThisClass::HandlePlayerRespawned);
	LobbyConfigurationComponent->InitializeRuntime(FSimpleDelegate::CreateWeakLambda(this, [this]()
	{
		if (!DefaultPlayerProvisioner->Initialize(
			LobbyConfigurationComponent->GetDefaultProvisionDefinition(), EDefaultProvisionMode::Lobby))
		{
			return;
		}
		LobbyConfigurationComponent->ApplyDefaultLobbyConfigIfNeeded();
		SpawnComponent->Initialize(LobbyConfigurationComponent->GetMatchRuleDefinition());
		UpdateAdvertisedSessionSettings();
		// Experience와 로비 설정 중 어느 쪽이 먼저 로딩되어도 두 준비가 끝난 뒤 플레이어를 시작한다.
		ResumeWaitingPlayers();
	}));
}

// 로비가 종료되면 조정 객체가 보유한 타이머와 비동기 요청을 정리한다.
void ALobbyGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StartCountdownTimerHandle);
	bGameStartRequested = false;
	TravelCoordinator->CancelPendingTravel();
	DefaultPlayerProvisioner->Shutdown();
	SpawnComponent->OnPlayerRespawned.RemoveAll(this);
	Super::EndPlay(EndPlayReason);
}

// 생성된 GameState의 Experience 관리자를 통해 로비 콘텐츠 준비를 시작한다.
void ALobbyGameMode::InitGameState()
{
	Super::InitGameState();
	StartExperienceLoad();
}

// 엔진의 접속 승인을 유지하면서 현재 로비 설정의 인원 제한을 적용한다.
void ALobbyGameMode::PreLogin(
	const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (ErrorMessage.IsEmpty() && LobbyConfigurationComponent->IsRuntimeReady() && GameState && GameState->PlayerArray.Num() >= LobbyConfigurationComponent->GetConfiguredMaxPlayerCount())
	{
		ErrorMessage = TEXT("Server is full.");
	}
}

// 일반 접속과 심리스 트래블 모두에서 Pawn 생성 전에 이름, 팀과 스폰 식별 정보를 준비한다.
void ALobbyGameMode::GenericPlayerInitialization(AController* Controller)
{
	Super::GenericPlayerInitialization(Controller);
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	APdPlayerState* LobbyPlayerState = PlayerController ? PlayerController->GetPlayerState<APdPlayerState>() : nullptr;
	if (!LobbyPlayerState)
	{
		return;
	}

	SpawnComponent->ClearRuntimeStateForController(Controller);
	LobbyPlayerSetupComponent->InitializeLobbyPlayerState(PlayerController, LobbyPlayerState);
	if (APdPlayerController* PdPlayerController = Cast<APdPlayerController>(PlayerController))
	{
		PdPlayerController->Client_RequestLocalCosmeticProfileSync();
	}
}

// 필수 콘텐츠 준비 후 Pawn을 시작하고 로비 장비를 지급한다. 전환 중 입장한 Pawn도 이동을 잠근다.
void ALobbyGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (!IsReadyForPlayerStart())
	{
		return;
	}

	AssignLobbyTeamColorIfNeeded(NewPlayer ? NewPlayer->GetPlayerState<APdPlayerState>() : nullptr);
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	ProvisionLobbyPlayer(NewPlayer);
	const ALobbyGameState* LobbyGameState = GetGameState<ALobbyGameState>();
	if (LobbyGameState && LobbyGameState->IsGameStartPending())
	{
		TravelCoordinator->SetLobbyPawnTravelLocked(NewPlayer, true);
	}
}

// 설정된 Experience가 준비된 경우에만 해당 Pawn을 선택하고, 미설정 맵에서는 기본 클래스를 사용한다.
UClass* ALobbyGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (!IsReadyForPlayerStart())
	{
		return nullptr;
	}
	if (GetConfiguredExperienceId().IsValid())
	{
		const UExperienceDefinition* Experience = GetExperienceManager()->GetCurrentExperienceChecked();
		if (Experience->DefaultPawnClass) { return Experience->DefaultPawnClass; }
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

// 퇴장 플레이어의 지급 상태를 정리한다. UI의 목록 갱신은 GameState의 제거 알림이 담당한다.
void ALobbyGameMode::Logout(AController* Exiting)
{
	DefaultPlayerProvisioner->ClearRuntimeStateForController(Exiting, Exiting ? Exiting->PlayerState : nullptr);
	SpawnComponent->ClearRuntimeStateForController(Exiting);
	Super::Logout(Exiting);
}

// 호스트가 확정한 로비 설정을 적용한다.
void ALobbyGameMode::SaveConfig(FName MapKey, int32 InMaxBotCount)
{
	if (!LobbyConfigurationComponent->IsRuntimeReady()) { return; }
	FLobbyMatchMapOption Option;
	if (!LobbyConfigurationComponent->FindConfiguredMapOption(MapKey, Option)) { return; }
	if (bGameStartRequested) { CancelPendingGameStart(); }
	LobbyConfigurationComponent->SaveConfig(MapKey, InMaxBotCount);
	UpdateAdvertisedSessionSettings();
}

// 시작 조건을 확인하고 Pawn을 잠근다. 혼자이면 즉시 이동하고 여러 명이면 카운트다운한다.
void ALobbyGameMode::TryStartGame()
{
	if (!CanHostStartGame()) { return; }
	bGameStartRequested = true;
	GetWorldTimerManager().ClearTimer(StartCountdownTimerHandle);
	const float CountdownSeconds = GetStartCountdownSeconds();
	if (ALobbyGameState* State = GetGameState<ALobbyGameState>())
	{
		State->SetGameStartPending(true, State->GetServerWorldTimeSeconds() + CountdownSeconds);
	}
	TravelCoordinator->SetAllLobbyPawnsTravelLocked(true);
	if (CountdownSeconds > 0.0f)
	{
		GetWorldTimerManager().SetTimer(StartCountdownTimerHandle, this, &ThisClass::HandleStartCountdownElapsed, CountdownSeconds, false);
	}
	else
	{
		HandleStartCountdownElapsed();
	}
}

// 호스트의 시작 버튼이 현재 콘텐츠와 로비 조건을 만족하는지 조회한다.
bool ALobbyGameMode::CanHostStartGame() const
{
	return !bGameStartRequested && AreMatchStartConditionsMet();
}

// 팀 변경 시 진행 중인 시작 카운트다운을 취소하게 한다.
void ALobbyGameMode::NotifyLobbyTeamChanged()
{
	if (bGameStartRequested) { CancelPendingGameStart(); }
}

// 호스트가 지정한 플레이어의 강퇴를 처리한다.
void ALobbyGameMode::KickPlayer(APdPlayerState* TargetPlayerState)
{
	if (!HasAuthority() || !TargetPlayerState || !GameSession) { return; }
	APlayerController* Controller = TargetPlayerState->GetPlayerController();
	if (!Controller)
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (It->Get() && It->Get()->PlayerState == TargetPlayerState) { Controller = It->Get(); break; }
		}
	}
	if (!Controller) { return; }
	if (bGameStartRequested) { CancelPendingGameStart(); }
	TargetPlayerState->GetLobbyPlayerStateComponent()->SetLeavingLobby(true);
	GameSession->KickPlayer(Controller, NSLOCTEXT("Lobby", "KickedByHost", "Kicked by host"));
}

// Pawn이 준비된 플레이어에게 로비 장비와 기본 상태를 지급한다. 반복 요청의 중복 방지는 지급기가 담당한다.
void ALobbyGameMode::ProvisionLobbyPlayer(APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->GetPawn() || !IsReadyForPlayerStart())
	{
		return;
	}
	DefaultPlayerProvisioner->ProvisionPlayer(PlayerController);
}

bool ALobbyGameMode::IsReadyForPlayerStart() const
{
	return LobbyConfigurationComponent->IsRuntimeReady()
		&& DefaultPlayerProvisioner->IsInitialized()
		&& (!GetConfiguredExperienceId().IsValid() || (GetExperienceManager() && GetExperienceManager()->IsExperienceLoaded()));
}

void ALobbyGameMode::ResumeWaitingPlayers()
{
	UWorld* World = GetWorld();
	if (!World || !IsReadyForPlayerStart())
	{
		return;
	}
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* Player = Iterator->Get();
		if (IsValid(Player) && !Player->GetPawn() && PlayerCanRestart(Player))
		{
			HandleStartingNewPlayer(Player);
		}
	}
}

AActor* ALobbyGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (AActor* Start = SpawnComponent->ChooseConfiguredPlayerStart(Player, TEXT("Spawn_"))) { return Start; }
	AActor* Start = Super::ChoosePlayerStart_Implementation(Player);
	SpawnComponent->MarkPlayerStartUsed(Player, Start);
	return Start;
}

APawn* ALobbyGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* Player, const FTransform& Transform)
{
	APawn* Pawn = Super::SpawnDefaultPawnAtTransform_Implementation(Player, Transform);
	if (Pawn) { SpawnComponent->RecordInitialSpawn(Player, Pawn->GetActorTransform()); }
	return Pawn;
}

void ALobbyGameMode::HandlePlayerRespawned(APlayerController* Player, bool bCreatedPawn)
{
	// 재사용한 Pawn의 지급 상태는 보존하며 새 Pawn에만 기존 로비 지급을 실행한다.
	if (bCreatedPawn) { ProvisionLobbyPlayer(Player); }
}

FPrimaryAssetId ALobbyGameMode::GetConfiguredExperienceId() const
{
	const APdWorldSettings* Settings = GetWorld() ? Cast<APdWorldSettings>(GetWorld()->GetWorldSettings()) : nullptr;
	return Settings ? Settings->GetDefaultExperienceId() : FPrimaryAssetId();
}

UExperienceManagerComponent* ALobbyGameMode::GetExperienceManager() const
{
	const ALobbyGameState* State = GetGameState<ALobbyGameState>();
	return State ? State->GetExperienceManagerComponent() : nullptr;
}

void ALobbyGameMode::StartExperienceLoad()
{
	const FPrimaryAssetId Id = GetConfiguredExperienceId();
	if (!Id.IsValid()) { return; }
	UExperienceManagerComponent* Manager = GetExperienceManager();
	if (!Manager)
	{
		HandleExperienceLoadFailed(Id, TEXT("LobbyGameState or ExperienceManagerComponent is missing."));
		return;
	}
	Manager->CallOrRegister_OnExperienceLoaded(FOnPdExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::HandleExperienceLoaded));
	Manager->CallOrRegister_OnExperienceLoadFailed(FOnPdExperienceLoadFailed::FDelegate::CreateUObject(this, &ThisClass::HandleExperienceLoadFailed));
	if (Manager->GetLoadState() == EExperienceLoadState::Unloaded) { Manager->SetCurrentExperienceAuth(Id); }
}

void ALobbyGameMode::HandleExperienceLoaded(const UExperienceDefinition* Experience)
{
	GetGameState<ALobbyGameState>()->SetExperienceLoadFailed(false);
	ResumeWaitingPlayers();
}

void ALobbyGameMode::HandleExperienceLoadFailed(FPrimaryAssetId Id, const FString& Message)
{
	UE_LOG(LogLobbyGameMode, Error, TEXT("Lobby experience load failed: %s %s"), *Id.ToString(), *Message);
	if (ALobbyGameState* State = GetGameState<ALobbyGameState>()) { State->SetExperienceLoadFailed(true); }
}

void ALobbyGameMode::SelectLobbyMapByOffset(int32 Offset)
{
	if (!HasAuthority() || !LobbyConfigurationComponent->IsRuntimeReady() || Offset == 0) { return; }
	const int32 Count = LobbyConfigurationComponent->GetLobbyMapOptionCount();
	const FName Key = LobbyConfigurationComponent->GetSelectedLobbyMapKey();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FLobbyMatchMapOption Option;
		LobbyConfigurationComponent->GetLobbyMapOptionAtIndex(Index, Option);
		if (Option.MapKey != Key) { continue; }
		LobbyConfigurationComponent->GetLobbyMapOptionAtIndex(((Index + Offset) % Count + Count) % Count, Option);
		SaveConfig(Option.MapKey, LobbyConfigurationComponent->GetConfiguredMaxBotCount());
		return;
	}
}

bool ALobbyGameMode::AreMatchStartConditionsMet() const
{
	if (!HasAuthority() || !IsReadyForPlayerStart())
	{
		return false;
	}

	const int32 ActivePlayerCount = GetActiveLobbyPlayerCount();
	const int32 MaxPlayerCount = LobbyConfigurationComponent->GetConfiguredMaxPlayerCount();
	return ActivePlayerCount > 0 && ActivePlayerCount <= MaxPlayerCount && AreLobbyTeamsBalanced();
}

int32 ALobbyGameMode::GetActiveLobbyPlayerCount() const
{
	if (!GameState)
	{
		return 0;
	}

	int32 ActivePlayerCount = 0;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		const APdPlayerState* LobbyPlayerState = Cast<APdPlayerState>(PlayerState);
		if (LobbyPlayerState && !LobbyPlayerState->GetLobbyPlayerStateComponent()->IsLeavingLobby())
		{
			++ActivePlayerCount;
		}
	}

	return ActivePlayerCount;
}

bool ALobbyGameMode::AreLobbyTeamsBalanced() const
{
	if (!GameState)
	{
		return false;
	}

	int32 ActivePlayerCount = 0;
	TMap<int32, int32> PlayerCountsByTeamColor;
	bool bHasUnassignedTeam = false;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		const APdPlayerState* LobbyPlayerState = Cast<APdPlayerState>(PlayerState);
		if (!LobbyPlayerState || LobbyPlayerState->GetLobbyPlayerStateComponent()->IsLeavingLobby())
		{
			continue;
		}

		++ActivePlayerCount;
		const int32 TeamColorIndex = LobbyPlayerState->GetPlayerMatchComponent()->GetMatchTeamColorIndex();
		if (TeamColorIndex == INDEX_NONE)
		{
			bHasUnassignedTeam = true;
		}
		else
		{
			++PlayerCountsByTeamColor.FindOrAdd(TeamColorIndex);
		}
	}

	// 혼자 입장할 때에는 팀 지정 여부와 관계없이 연습 경기를 허용한다.
	if (ActivePlayerCount == 1)
	{
		return true;
	}
	if (ActivePlayerCount < 2 || PlayerCountsByTeamColor.Num() < 2 || bHasUnassignedTeam)
	{
		return false;
	}

	const int32 PlayersPerTeam = ActivePlayerCount / PlayerCountsByTeamColor.Num();
	for (const TPair<int32, int32>& TeamPlayerCount : PlayerCountsByTeamColor)
	{
		if (TeamPlayerCount.Value != PlayersPerTeam)
		{
			return false;
		}
	}
	return true;
}

void ALobbyGameMode::AssignLobbyTeamColorIfNeeded(APdPlayerState* LobbyPlayerState) const
{
	if (!LobbyPlayerState || LobbyPlayerState->GetPlayerMatchComponent()->GetMatchTeamColorIndex() != INDEX_NONE)
	{
		return;
	}

	int32 TeamColorIndex = LobbyPlayerState->GetPlayerMatchComponent()->GetMatchSpawnIndex();
	if (TeamColorIndex == INDEX_NONE)
	{
		TeamColorIndex = FindAvailableLobbyTeamColorIndex(LobbyPlayerState);
	}

	TeamColorIndex = FMath::Clamp(TeamColorIndex, 0, LobbyConfigurationComponent->GetConfiguredMaxPlayerCount() - 1);
	LobbyPlayerState->GetPlayerMatchComponent()->SetMatchTeamColorIndex(TeamColorIndex);
}

int32 ALobbyGameMode::FindAvailableLobbyTeamColorIndex(const APdPlayerState* IgnoredPlayerState) const
{
	TSet<int32> UsedTeamColorIndices;
	if (GameState)
	{
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			const APdPlayerState* LobbyPlayerState = Cast<APdPlayerState>(PlayerState);
			if (!LobbyPlayerState || LobbyPlayerState == IgnoredPlayerState
				|| LobbyPlayerState->GetLobbyPlayerStateComponent()->IsLeavingLobby())
			{
				continue;
			}

			const int32 TeamColorIndex = LobbyPlayerState->GetPlayerMatchComponent()->GetMatchTeamColorIndex();
			if (TeamColorIndex != INDEX_NONE)
			{
				UsedTeamColorIndices.Add(TeamColorIndex);
			}
		}
	}

	for (int32 TeamColorIndex = 0; TeamColorIndex < LobbyConfigurationComponent->GetConfiguredMaxPlayerCount();
		++TeamColorIndex)
	{
		if (!UsedTeamColorIndices.Contains(TeamColorIndex))
		{
			return TeamColorIndex;
		}
	}

	return 0;
}

void ALobbyGameMode::HandleStartCountdownElapsed()
{
	if (!bGameStartRequested) { return; }
	if (!AreMatchStartConditionsMet())
	{
		CancelPendingGameStart();
		return;
	}
	TravelCoordinator->StartSessionAndTravel(GetActiveLobbyPlayerCount() == 1);
}

float ALobbyGameMode::GetStartCountdownSeconds() const
{
	return GetActiveLobbyPlayerCount() == 1 ? 0.0f
		: FMath::Max(LobbyConfigurationComponent->GetMatchRuleDefinition()->LobbyStartCountdownSeconds, 0.0f);
}

// 시작 상태는 GameMode에서 끝내고, 비동기 이동과 화면·잠금 정리는 이동 객체에 맡긴다.
void ALobbyGameMode::CancelPendingGameStart()
{
	if (!HasAuthority()) { return; }
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(StartCountdownTimerHandle); }
	bGameStartRequested = false;
	if (ALobbyGameState* State = GetGameState<ALobbyGameState>()) { State->SetGameStartPending(false, 0.0); }
	TravelCoordinator->CancelPendingTravel();
}

void ALobbyGameMode::UpdateAdvertisedSessionSettings() const
{
	if (!HasAuthority()) { return; }
	UOnlineSessionsSubsystem* Sessions = GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>();
	if (!Sessions || !Sessions->HasNamedSession()) { return; }
	FLobbyMatchMapOption Option;
	if (!LobbyConfigurationComponent->GetSelectedLobbyMapOption(Option)) { return; }
	Sessions->UpdateSessionSettings(Option.MapKey.ToString(), Option.MaxPlayerCount, true);
}
