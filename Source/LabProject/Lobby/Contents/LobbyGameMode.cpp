#include "Lobby/Contents/LobbyGameMode.h"

#include "AI/Bot/PdBotController.h"
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
#include "GameFramework/HUD.h"
#include "Lobby/Contents/LobbyPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "Online/GameLift/GameLiftServerSubsystem.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "Lobby/Coordination/LobbyTravelCoordinator.h"
#include "Localization/MenuLocalizationSubsystem.h"
#include "Mode/PdPlayerController.h"
#include "Provision/DefaultPlayerProvisioner.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyGameMode)

DEFINE_LOG_CATEGORY_STATIC(LogLobbyGameMode, Log, All);

namespace
{
	TAutoConsoleVariable<int32> CVarDedicatedServerMinPlayersToStart(TEXT("pd.DedicatedServer.MinPlayersToStart"), 2,
		TEXT("Minimum lobby players before a dedicated server starts the match automatically."), ECVF_Default);

	TAutoConsoleVariable<float> CVarDedicatedServerAutoStartDelay(TEXT("pd.DedicatedServer.AutoStartDelay"), 5.0f,
		TEXT("Seconds the start conditions must hold before a dedicated server starts the lobby countdown."),
		ECVF_Default);

	constexpr float DedicatedServerAutoStartCheckIntervalSeconds = 1.0f;
}

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
	// HUD는 UI 쪽 클래스라 BP 데이터로 지정하고, 엔진 기본 HUD가 남아 있지 않은지만 확인한다.
	if (!HUDClass || HUDClass == AHUD::StaticClass())
	{
		Context.AddError(NSLOCTEXT("LobbyGameMode", "MissingLobbyHUD", "HUDClass must be set to the lobby HUD."));
		Result = EDataValidationResult::Invalid;
	}
	ValidateClass(DefaultPawnClass, APdPlayer::StaticClass(), TEXT("DefaultPawnClass"));
	return Result == EDataValidationResult::Invalid ? Result : EDataValidationResult::Valid;
}
#endif

// 로비의 필수 처리 객체와 기본 프레임워크 클래스를 구성한다.
ALobbyGameMode::ALobbyGameMode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	LobbyConfigurationComponent = CreateDefaultSubobject<ULobbyConfigurationComponent>(TEXT("LobbyConfigurationComponent"));
	LobbyPlayerSetupComponent = CreateDefaultSubobject<ULobbyPlayerSetupComponent>(TEXT("LobbyPlayerSetupComponent"));
	SpawnComponent = CreateDefaultSubobject<UPlayerSpawnComponent>(TEXT("SpawnComponent"));
	DefaultPlayerProvisioner = CreateDefaultSubobject<UDefaultPlayerProvisioner>(TEXT("DefaultPlayerProvisioner"));
	TravelCoordinator = CreateDefaultSubobject<ULobbyTravelCoordinator>(TEXT("LobbyTravelCoordinator"));

	PlayerControllerClass = ALobbyPlayerController::StaticClass();
	GameStateClass = ALobbyGameState::StaticClass();
	PlayerStateClass = APdPlayerState::StaticClass();
	DefaultPawnClass = APdPlayer::StaticClass();
	BotControllerClass = APdBotController::StaticClass();
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

	if (GetNetMode() == NM_DedicatedServer)
	{
		GetWorldTimerManager().SetTimer(DedicatedServerAutoStartTimerHandle, this,
			&ThisClass::UpdateDedicatedServerAutoStart, DedicatedServerAutoStartCheckIntervalSeconds, true);

		// 로비가 열려 접속을 받을 수 있으면 GameLift에 게임 세션을 받을 준비가 됐다고 알린다.
		if (UGameLiftServerSubsystem* GameLift = UGameLiftServerSubsystem::Get(this))
		{
			GameLift->NotifyServerReadyForSessions(GetWorld());
		}
	}
}

// 로비가 종료되면 조정 객체가 보유한 타이머와 비동기 요청을 정리한다.
void ALobbyGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StartCountdownTimerHandle);
	GetWorldTimerManager().ClearTimer(DedicatedServerAutoStartTimerHandle);
	bGameStartRequested = false;
	BotPandoraPreloadHandle.Reset();
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

// 엔진의 접속 승인을 유지하면서 현재 로비 설정의 인원 제한을 적용한다. GameLift 서버는 마지막으로 player session을 검증한다.
void ALobbyGameMode::PreLogin(
	const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (ErrorMessage.IsEmpty() && LobbyConfigurationComponent->IsRuntimeReady() && GetHumanPlayerCount() >= LobbyConfigurationComponent->GetConfiguredMaxPlayerCount())
	{
		ErrorMessage = TEXT("Server is full.");
	}
	if (UGameLiftServerSubsystem* GameLift = UGameLiftServerSubsystem::Get(this))
	{
		GameLift->ValidatePlayerJoin(Options, ErrorMessage);
	}
}

// 사람이 들어와 정원을 넘으면 봇이 자리를 비켜 준다.
void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	RemoveBotsOverCapacity();
}

// 일반 접속과 심리스 트래블 모두에서 Pawn 생성 전에 이름, 팀과 스폰 식별 정보를 준비한다.
// 봇은 방장이 넣을 때와 경기에서 돌아올 때 여기를 지난다(로비 Pawn과 지급은 없다).
void ALobbyGameMode::GenericPlayerInitialization(AController* Controller)
{
	Super::GenericPlayerInitialization(Controller);
	APdPlayerState* LobbyPlayerState = Controller ? Controller->GetPlayerState<APdPlayerState>() : nullptr;
	if (!LobbyPlayerState)
	{
		return;
	}

	SpawnComponent->ClearRuntimeStateForController(Controller);
	LobbyPlayerSetupComponent->InitializeLobbyPlayerState(Cast<APlayerController>(Controller), LobbyPlayerState);
	if (APdPlayerController* PdPlayerController = Cast<APdPlayerController>(Controller))
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
		if (Experience->DefaultPawnClass)
		{
			return Experience->DefaultPawnClass;
		}
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

// 퇴장 플레이어의 지급 상태를 정리한다. UI의 목록 갱신은 GameState의 제거 알림이 담당한다.
// 마지막 사람이 나가면 봇도 뺀다(퇴장하는 사람의 PlayerState는 아직 목록에 있다).
void ALobbyGameMode::Logout(AController* Exiting)
{
	const bool bLastHumanLeaving = Exiting && Exiting->IsPlayerController() && GetHumanPlayerCount(Exiting->PlayerState) == 0;
	DefaultPlayerProvisioner->ClearRuntimeStateForController(Exiting, Exiting ? Exiting->PlayerState : nullptr);
	SpawnComponent->ClearRuntimeStateForController(Exiting);
	Super::Logout(Exiting);
	if (bLastHumanLeaving)
	{
		RemoveAllBots();
	}
}

// 경기에서 돌아온 봇은 사람과 달리 엔진이 시작 처리를 불러 주지 않는다. 사람이 모두 떠난 채 돌아왔으면(빈 전용 서버) 봇도 뺀다.
void ALobbyGameMode::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();
	RemoveBotsWithoutHumans();
}

// 호스트가 확정한 로비 설정을 적용한다.
void ALobbyGameMode::SelectLobbyMap(FName MapKey)
{
	if (!LobbyConfigurationComponent->IsRuntimeReady())
	{
		return;
	}
	FLobbyMatchMapOption Option;
	if (!LobbyConfigurationComponent->FindConfiguredMapOption(MapKey, Option))
	{
		return;
	}
	if (bGameStartRequested)
	{
		CancelPendingGameStart();
	}
	LobbyConfigurationComponent->SelectLobbyMap(MapKey);
	RemoveBotsOverCapacity();
	UpdateAdvertisedSessionSettings();
}

// 시작 조건을 확인하고 Pawn을 잠근다. 혼자이면 즉시 이동하고 여러 명이면 카운트다운한다.
void ALobbyGameMode::TryStartGame()
{
	if (!CanHostStartGame())
	{
		return;
	}
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
	if (bGameStartRequested)
	{
		CancelPendingGameStart();
	}
}

// 호스트가 지정한 플레이어의 강퇴를 처리한다.
void ALobbyGameMode::KickPlayer(APdPlayerState* TargetPlayerState)
{
	if (!HasAuthority() || !TargetPlayerState || !GameSession)
	{
		return;
	}
	if (TargetPlayerState->IsABot())
	{
		RemoveBot(TargetPlayerState);
		return;
	}
	APlayerController* Controller = TargetPlayerState->GetPlayerController();
	if (!Controller)
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (It->Get() && It->Get()->PlayerState == TargetPlayerState)
			{
				Controller = It->Get();
				break;
			}
		}
	}
	if (!Controller)
	{
		return;
	}
	if (bGameStartRequested)
	{
		CancelPendingGameStart();
	}
	TargetPlayerState->GetLobbyPlayerStateComponent()->SetLeavingLobby(true);
	GameSession->KickPlayer(Controller, NSLOCTEXT("Lobby", "KickedByHost", "Kicked by host"));
}

// 봇은 로비 Pawn 없이 자리만 차지한다. 이름·스폰 번호·팀은 사람처럼 정한다. 플레이어 번호는 엔진이 사람에게 주는 번호에서 이어 받는다
// (엔진은 사람이 접속할 때만 번호를 매긴다. 그대로 두면 봇은 모두 0이 되어 목록 맨 앞에 서고 결과 화면에서 서로 구분되지 않는다).
bool ALobbyGameMode::AddBot()
{
	if (!CanAddBot())
	{
		return false;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APdBotController* Bot = GetWorld()->SpawnActor<APdBotController>(BotControllerClass, SpawnParameters);
	APdPlayerState* BotPlayerState = Bot ? Bot->GetPlayerState<APdPlayerState>() : nullptr;
	if (!BotPlayerState)
	{
		if (Bot)
		{
			Bot->Destroy();
		}
		return false;
	}
	BotPlayerState->SetPlayerId(GameSession->GetNextPlayerID());
	BotPlayerState->GetPlayerMatchComponent()->SetMatchDisplayName(MakeBotDisplayName());
	GenericPlayerInitialization(Bot);
	AssignLobbyTeamColorIfNeeded(BotPlayerState);
	NotifyLobbyTeamChanged();
	PreloadBotPandoras();
	return true;
}

// 경기로 갈 때 봇의 판도라를 고르려면(LobbyTravelCoordinator) 판도라 정의와 스킬이 불러와져 있어야 한다.
void ALobbyGameMode::PreloadBotPandoras()
{
	UContentDataSubsystem* ContentData = UGameInstance::GetSubsystem<UContentDataSubsystem>(GetGameInstance());
	if (!BotPandoraPreloadHandle.IsValid() && ContentData)
	{
		BotPandoraPreloadHandle = ContentData->PreloadPandoraDataAssetsAsync();
	}
}

// 봇은 사람과 함께 놀려고 넣는 것이므로, 사람이 아무도 없으면 남겨 두지 않는다(빈 전용 서버가 봇끼리 경기를 시작하지 않게).
void ALobbyGameMode::RemoveBotsWithoutHumans()
{
	if (GetHumanPlayerCount() == 0)
	{
		RemoveAllBots();
	}
}

bool ALobbyGameMode::CanAddBot() const
{
	return HasAuthority() && GameState && GameSession && BotControllerClass && LobbyConfigurationComponent->IsRuntimeReady() && !bGameStartRequested
		&& GetActiveLobbyPlayerCount() < LobbyConfigurationComponent->GetConfiguredMaxPlayerCount();
}

void ALobbyGameMode::RemoveBot(APdPlayerState* BotPlayerState)
{
	AController* BotController = BotPlayerState && BotPlayerState->IsABot() ? Cast<AController>(BotPlayerState->GetOwner()) : nullptr;
	if (!HasAuthority() || !BotController)
	{
		return;
	}
	NotifyLobbyTeamChanged();
	BotController->Destroy();
}

void ALobbyGameMode::SetBotTeamColor(APdPlayerState* BotPlayerState, const int32 TeamColorIndex)
{
	if (!HasAuthority() || !BotPlayerState || !BotPlayerState->IsABot())
	{
		return;
	}
	const int32 LastTeamColorIndex = FMath::Max(LobbyConfigurationComponent->GetConfiguredMaxPlayerCount(), 1) - 1;
	const int32 ClampedIndex = FMath::Clamp(TeamColorIndex, 0, LastTeamColorIndex);
	if (BotPlayerState->GetPlayerMatchComponent()->GetMatchTeamColorIndex() != ClampedIndex)
	{
		BotPlayerState->GetPlayerMatchComponent()->SetMatchTeamColorIndex(ClampedIndex);
		NotifyLobbyTeamChanged();
	}
}

void ALobbyGameMode::RemoveAllBots()
{
	for (APdPlayerState* BotPlayerState : GetBotPlayerStates())
	{
		RemoveBot(BotPlayerState);
	}
}

int32 ALobbyGameMode::GetHumanPlayerCount(const APlayerState* IgnoredPlayerState) const
{
	int32 HumanPlayerCount = 0;
	if (!GameState)
	{
		return HumanPlayerCount;
	}
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		const APdPlayerState* LobbyPlayerState = Cast<APdPlayerState>(PlayerState);
		if (LobbyPlayerState && LobbyPlayerState != IgnoredPlayerState && !LobbyPlayerState->IsABot()
			&& !LobbyPlayerState->GetLobbyPlayerStateComponent()->IsLeavingLobby())
		{
			++HumanPlayerCount;
		}
	}
	return HumanPlayerCount;
}

TArray<APdPlayerState*> ALobbyGameMode::GetBotPlayerStates() const
{
	TArray<APdPlayerState*> Bots;
	if (!GameState)
	{
		return Bots;
	}
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		APdPlayerState* LobbyPlayerState = Cast<APdPlayerState>(PlayerState);
		if (LobbyPlayerState && LobbyPlayerState->IsABot() && !LobbyPlayerState->GetLobbyPlayerStateComponent()->IsLeavingLobby())
		{
			Bots.Add(LobbyPlayerState);
		}
	}
	Bots.Sort([](const APdPlayerState& A, const APdPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
	return Bots;
}

void ALobbyGameMode::RemoveBotsOverCapacity()
{
	TArray<APdPlayerState*> Bots = GetBotPlayerStates();
	const int32 MaxPlayerCount = LobbyConfigurationComponent->GetConfiguredMaxPlayerCount();
	for (int32 ExcessCount = GetActiveLobbyPlayerCount() - MaxPlayerCount; ExcessCount > 0 && !Bots.IsEmpty(); --ExcessCount)
	{
		RemoveBot(Bots.Pop());
	}
}

// 지금 봇들이 쓰지 않는 가장 작은 번호로 'Bot 1' 같은 이름을 짓는다.
FText ALobbyGameMode::MakeBotDisplayName() const
{
	const FText NameFormat = UMenuLocalizationSubsystem::GetTextOrFallback(TEXT("Lobby.BotName"), INVTEXT("Bot {0}"));
	TSet<FString> UsedNames;
	for (const APdPlayerState* Bot : GetBotPlayerStates())
	{
		UsedNames.Add(Bot->GetPlayerMatchComponent()->GetMatchDisplayName().ToString());
	}
	for (int32 BotNumber = 1;; ++BotNumber)
	{
		const FText Name = FText::Format(NameFormat, BotNumber);
		if (!UsedNames.Contains(Name.ToString()))
		{
			return Name;
		}
	}
}

// Pawn이 준비된 플레이어에게 로비 장비와 기본 상태를 지급한다. 반복 요청의 중복 방지는 지급기가 담당한다.
void ALobbyGameMode::ProvisionLobbyPlayer(AController* Player)
{
	if (!Player || !Player->GetPawn() || !IsReadyForPlayerStart())
	{
		return;
	}
	DefaultPlayerProvisioner->ProvisionPlayer(Player);
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
	if (AActor* Start = SpawnComponent->ChooseConfiguredPlayerStart(Player, TEXT("Spawn_")))
	{
		return Start;
	}
	AActor* Start = Super::ChoosePlayerStart_Implementation(Player);
	SpawnComponent->MarkPlayerStartUsed(Player, Start);
	return Start;
}

APawn* ALobbyGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* Player, const FTransform& Transform)
{
	APawn* Pawn = Super::SpawnDefaultPawnAtTransform_Implementation(Player, Transform);
	if (Pawn)
	{
		SpawnComponent->RecordInitialSpawn(Player, Pawn->GetActorTransform());
	}
	return Pawn;
}

void ALobbyGameMode::HandlePlayerRespawned(AController* Player, bool bCreatedPawn)
{
	// 재사용한 Pawn의 지급 상태는 보존하며 새 Pawn에만 기존 로비 지급을 실행한다.
	if (bCreatedPawn)
	{
		ProvisionLobbyPlayer(Player);
	}
}

FPrimaryAssetId ALobbyGameMode::GetConfiguredExperienceId() const
{
	return APdWorldSettings::FindExperienceId(GetWorld(), false);
}

UExperienceManagerComponent* ALobbyGameMode::GetExperienceManager() const
{
	const ALobbyGameState* State = GetGameState<ALobbyGameState>();
	return State ? State->GetExperienceManagerComponent() : nullptr;
}

void ALobbyGameMode::StartExperienceLoad()
{
	const FPrimaryAssetId Id = GetConfiguredExperienceId();
	if (!Id.IsValid())
	{
		return;
	}
	UExperienceManagerComponent* Manager = GetExperienceManager();
	if (!Manager)
	{
		HandleExperienceLoadFailed(Id, TEXT("LobbyGameState or ExperienceManagerComponent is missing."));
		return;
	}
	Manager->CallOrRegister_OnExperienceLoaded(FOnExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::HandleExperienceLoaded));
	Manager->CallOrRegister_OnExperienceLoadFailed(FOnExperienceLoadFailed::FDelegate::CreateUObject(this, &ThisClass::HandleExperienceLoadFailed));
	if (Manager->GetLoadState() == EExperienceLoadState::Unloaded)
	{
		Manager->SetCurrentExperienceAuth(Id);
	}
}

void ALobbyGameMode::HandleExperienceLoaded(const UExperienceDefinition* Experience)
{
	GetGameState<ALobbyGameState>()->SetExperienceLoadFailed(false);
	ResumeWaitingPlayers();
}

void ALobbyGameMode::HandleExperienceLoadFailed(FPrimaryAssetId Id, const FString& Message)
{
	UE_LOG(LogLobbyGameMode, Error, TEXT("Lobby experience load failed: %s %s"), *Id.ToString(), *Message);
	if (ALobbyGameState* State = GetGameState<ALobbyGameState>())
	{
		State->SetExperienceLoadFailed(true);
	}
}

void ALobbyGameMode::SelectLobbyMapByOffset(int32 Offset)
{
	if (!HasAuthority() || !LobbyConfigurationComponent->IsRuntimeReady() || Offset == 0)
	{
		return;
	}
	const int32 Count = LobbyConfigurationComponent->GetLobbyMapOptionCount();
	const FName Key = LobbyConfigurationComponent->GetSelectedLobbyMapKey();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FLobbyMatchMapOption Option;
		LobbyConfigurationComponent->GetLobbyMapOptionAtIndex(Index, Option);
		if (Option.MapKey != Key)
		{
			continue;
		}
		LobbyConfigurationComponent->GetLobbyMapOptionAtIndex(((Index + Offset) % Count + Count) % Count, Option);
		SelectLobbyMap(Option.MapKey);
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
	return GetHumanPlayerCount() > 0 && ActivePlayerCount <= MaxPlayerCount && AreLobbyTeamsBalanced();
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

// 전용 서버에서 최소 인원과 팀 균형이 지정 시간 동안 유지되면 호스트 버튼과 같은 시작 흐름을 실행한다.
void ALobbyGameMode::UpdateDedicatedServerAutoStart()
{
	const UWorld* World = GetWorld();
	if (!World || GetNetMode() != NM_DedicatedServer || bGameStartRequested)
	{
		DedicatedServerAutoStartReadyTimeSeconds = -1.0;
		return;
	}

	const int32 MinPlayersToStart = FMath::Max(1, CVarDedicatedServerMinPlayersToStart.GetValueOnGameThread());
	if (GetActiveLobbyPlayerCount() < MinPlayersToStart || !CanHostStartGame())
	{
		DedicatedServerAutoStartReadyTimeSeconds = -1.0;
		return;
	}

	const double NowSeconds = World->GetTimeSeconds();
	if (DedicatedServerAutoStartReadyTimeSeconds < 0.0)
	{
		DedicatedServerAutoStartReadyTimeSeconds = NowSeconds;
		UE_LOG(LogLobbyGameMode, Log, TEXT("Dedicated server lobby is ready with %d players. Auto start in %.1fs."),
			GetActiveLobbyPlayerCount(), CVarDedicatedServerAutoStartDelay.GetValueOnGameThread());
	}

	if (NowSeconds - DedicatedServerAutoStartReadyTimeSeconds
		< FMath::Max(0.0f, CVarDedicatedServerAutoStartDelay.GetValueOnGameThread()))
	{
		return;
	}

	DedicatedServerAutoStartReadyTimeSeconds = -1.0;
	TryStartGame();
}

void ALobbyGameMode::HandleStartCountdownElapsed()
{
	if (!bGameStartRequested)
	{
		return;
	}
	if (!AreMatchStartConditionsMet())
	{
		CancelPendingGameStart();
		return;
	}
	// 경기 중인 게임 세션에는 새 player session을 만들지 않게 한다. 시작이 취소되면 CancelPendingGameStart가 다시 연다.
	if (UGameLiftServerSubsystem* GameLift = UGameLiftServerSubsystem::Get(this))
	{
		GameLift->SetPlayerSessionCreationPolicy(false);
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
	if (!HasAuthority())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StartCountdownTimerHandle);
	}
	bGameStartRequested = false;
	if (ALobbyGameState* State = GetGameState<ALobbyGameState>())
	{
		State->SetGameStartPending(false, 0.0);
	}
	TravelCoordinator->CancelPendingTravel();
	if (UGameLiftServerSubsystem* GameLift = UGameLiftServerSubsystem::Get(this))
	{
		GameLift->SetPlayerSessionCreationPolicy(true);
	}
}

void ALobbyGameMode::UpdateAdvertisedSessionSettings() const
{
	if (!HasAuthority())
	{
		return;
	}
	UOnlineSessionsSubsystem* Sessions = GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>();
	if (!Sessions || !Sessions->HasNamedSession())
	{
		return;
	}
	FLobbyMatchMapOption Option;
	if (!LobbyConfigurationComponent->GetSelectedLobbyMapOption(Option))
	{
		return;
	}
	Sessions->UpdateSessionSettings(Option.MapKey.ToString(), Option.MaxPlayerCount, true);
}
