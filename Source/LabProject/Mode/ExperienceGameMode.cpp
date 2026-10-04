#include "Mode/ExperienceGameMode.h"

#include "Character/PdPlayer.h"
#include "Common/GameSessionConstants.h"
#include "Component/Experience/ExperienceManagerComponent.h"
#include "Component/Match/MatchFlowComponent.h"
#include "Component/Match/MatchPlayerSetupComponent.h"
#include "Component/Match/MatchRewardComponent.h"
#include "Component/Player/PlayerSpawnComponent.h"
#include "Component/Player/SelectingPandoraAndWeaponComponent.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Definition/Experience/ExperienceDefinition.h"
#include "Definition/Level/LevelDefinition.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Definition/Provision/DefaultProvisionDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Experience/PdWorldSettings.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Misc/PackageName.h"
#include "Mode/ExperienceGameState.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Online/GameLift/GameLiftServerSubsystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExperienceGameMode)

DEFINE_LOG_CATEGORY(PdExperienceGameModeLog);

namespace
{
	// 심리스 이동 중인 참가자가 도착할 시간을 둔 뒤 빈 경기인지 다시 확인한다.
	constexpr float EmptyDedicatedServerLobbyReturnDelaySeconds = 3.0f;

	bool DoesMapOptionMatchWorld(
		const FLobbyMatchMapOption& MapOption,
		const FString& CurrentPackageName,
		const FString& CurrentLevelName)
	{
		const FString Package = MapOption.Map.ToSoftObjectPath().GetLongPackageName();
		return !Package.IsEmpty() && (Package.Equals(CurrentPackageName, ESearchCase::IgnoreCase)
			|| FPackageName::GetShortName(Package).Equals(CurrentLevelName, ESearchCase::IgnoreCase));
	}
}

// 경기에서 사용할 기본 프레임워크 클래스와 필수 컴포넌트를 구성한다.
AExperienceGameMode::AExperienceGameMode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	GameStateClass = AExperienceGameState::StaticClass();
	PlayerControllerClass = APdPlayerController::StaticClass();
	PlayerStateClass = APdPlayerState::StaticClass();
	DefaultPawnClass = APdPlayer::StaticClass();
	bUseSeamlessTravel = true;

	// 기존 Blueprint의 컴포넌트 기본값 연결을 보존하기 위해 직렬화된 서브오브젝트 이름은 유지한다.
	MatchFlowComponent = CreateDefaultSubobject<UMatchFlowComponent>(TEXT("ExperienceMatchFlow"));
	RewardComponent = CreateDefaultSubobject<UMatchRewardComponent>(TEXT("ExperienceMatchReward"));
	SpawnComponent = CreateDefaultSubobject<UPlayerSpawnComponent>(TEXT("ExperienceSpawn"));
	PlayerSetupComponent = CreateDefaultSubobject<UMatchPlayerSetupComponent>(TEXT("ExperiencePlayerProvisioning"));
	check(MatchFlowComponent && RewardComponent && SpawnComponent && PlayerSetupComponent);
}

// 맵 설정을 전달하고, 비동기 준비가 끝날 때마다 경기 시작 조건을 다시 확인한다.
void AExperienceGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	PlayerSetupComponent->OnPlayerGameplayReady.AddUObject(this, &ThisClass::TryStartServerMatch);
	MatchFlowComponent->InitializeTravelOptions(Options);
	BeginRuntimeContentPreload();
}

// Super::StartPlay가 맵의 모든 액터에 BeginPlay를 보낸 뒤 경기 시작 조건을 확인한다.
void AExperienceGameMode::StartPlay()
{
	Super::StartPlay();
	TryStartServerMatch();
	NotifyRpgWorldReadyIfNeeded();
}

// 맵을 떠난 뒤 준비 완료 콜백이 경기를 시작하지 않도록 연결을 정리한다.
void AExperienceGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseRuntimeContentPreload();
	PlayerSetupComponent->OnPlayerGameplayReady.RemoveAll(this);
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
}

// 복제할 경기 정보를 초기화하고 이 맵의 Experience 로딩을 시작한다.
void AExperienceGameMode::InitGameState()
{
	Super::InitGameState();
	if (IsRuntimeContentReady()) { MatchFlowComponent->InitializeGameState(); }
	StartExperienceLoad();
}

// 접속 승인 전 엔진의 인증 결과와 현재 서버의 정원을 확인한다.
void AExperienceGameMode::PreLogin(
	const FString& Options,
	const FString& Address,
	const FUniqueNetIdRepl& UniqueId,
	FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	const int32 CurrentPlayerCount =
		GameState ? GameState->PlayerArray.Num() : 0;
	if (CurrentPlayerCount >= LabGameSession::MaxPlayerCount)
	{
		ErrorMessage = TEXT("Server is full.");
	}
	if (UGameLiftServerSubsystem* GameLift = UGameLiftServerSubsystem::Get(this))
	{
		GameLift->ValidatePlayerJoin(Options, ErrorMessage);
	}
}

// 일반 접속과 심리스 이동에서 새 경기의 팀·기록·선택 슬롯을 초기화한다.
void AExperienceGameMode::GenericPlayerInitialization(AController* Controller)
{
	Super::GenericPlayerInitialization(Controller);

	APdPlayerState* PlayerState = Controller ? Controller->GetPlayerState<APdPlayerState>() : nullptr;
	UPlayerMatchComponent* PlayerMatch = PlayerState ? PlayerState->GetPlayerMatchComponent() : nullptr;
	if (!HasAuthority() || !PlayerState)
	{
		return;
	}

	// 같은 경기의 리스폰에서는 이 초기화를 반복하지 않는다.
	PlayerState->SetNetUpdateFrequency(100.0f);
	SpawnComponent->ClearRuntimeStateForController(Controller);
	PlayerSetupComponent->InitializeMatchIdentity(Cast<APlayerController>(Controller));

	EPlayerMapRegion InitialMapRegion = EPlayerMapRegion::Dome;
	FLobbyMatchMapOption MapOption;
	if (FindCurrentMatchMapOption(MapOption))
	{
		InitialMapRegion = MapOption.InitialPlayerMapRegion;
	}

	// 엔진의 CopyProperties가 전달한 Score도 새 경기에서는 초기화한다.
	if (PlayerMatch)
	{
		PlayerMatch->ResetForNewMatch(InitialMapRegion);
	}
	if (USelectingPandoraAndWeaponComponent* PlayerLoadout = PlayerState->GetSelectingPandoraAndWeaponComponent())
	{
		PlayerLoadout->RequestSelectPandoraAndWeapon(0);
	}
}

// 최초 접속의 프로필 복원을 스폰 전에 요청한다. 심리스 이동에서는 인계된 상태를 사용한다.
void AExperienceGameMode::OnPostLogin(AController* NewPlayer)
{
	PlayerSetupComponent->InitializeLoggedInPlayer(Cast<APlayerController>(NewPlayer));
	Super::OnPostLogin(NewPlayer);
}

// 실제 연결 종료를 정산 경로로 전달하고 해당 참가자의 스폰·지급 대기를 제거한다.
void AExperienceGameMode::Logout(AController* Exiting)
{
	APlayerState* ExitingPlayerState = Exiting ? Exiting->PlayerState : nullptr;
	MatchFlowComponent->HandlePlayerLogout(ExitingPlayerState);
	SpawnComponent->ClearRuntimeStateForController(Exiting);
	PlayerSetupComponent->ClearRuntimeStateForController(Exiting, ExitingPlayerState);
	Super::Logout(Exiting);

	// 엔진은 Logout 전에 퇴장 컨트롤러를 파괴 중으로 표시하므로, TryStartServerMatch는 남은 참가자만 확인한다.
	TryStartServerMatch();

	// Listen Server는 호스트가 나가면 서버도 끝나지만, 전용 서버는 빈 경기장에 남으므로 로비로 되돌린다.
	// RPG 공유 월드는 비어도 그대로 열어 두고 다음 참가자를 기다린다.
	if (GetNetMode() == NM_DedicatedServer && !MatchFlowComponent->IsRpgMode())
	{
		GetWorldTimerManager().SetTimer(
			EmptyDedicatedServerLobbyReturnTimerHandle,
			this,
			&ThisClass::ReturnEmptyDedicatedServerToLobby,
			EmptyDedicatedServerLobbyReturnDelaySeconds,
			false);
	}
}

void AExperienceGameMode::ReturnEmptyDedicatedServerToLobby()
{
	UWorld* World = GetWorld();
	if (!World || GetNetMode() != NM_DedicatedServer || GetNumPlayers() > 0)
	{
		return;
	}

	// GameLift 게임 세션은 한 경기만 진행하므로 로비로 돌아가지 않는다. 빈 세션은 GameLift 서브시스템이 종료한다.
	const UGameLiftServerSubsystem* GameLift = UGameLiftServerSubsystem::Get(this);
	if (GameLift && GameLift->IsGameLiftActive())
	{
		return;
	}

	const FString LobbyMapName = LoadedLevelDefinition ? LoadedLevelDefinition->GetLobbyTravelMapName() : FString();
	if (LobbyMapName.IsEmpty())
	{
		UE_LOG(PdExperienceGameModeLog, Warning, TEXT("Empty dedicated server match could not resolve the lobby map."));
		return;
	}

	UE_LOG(PdExperienceGameModeLog, Log, TEXT("All players left the dedicated server match. Returning to %s."), *LobbyMapName);
	World->ServerTravel(LobbyMapName);
}

// 일반 접속과 심리스 이동 모두 Experience가 준비되면 스폰하고 기본 지급을 요청한다.
void AExperienceGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (!IsValid(NewPlayer) || !CanStartGameplay())
	{
		return;
	}

	if (!NewPlayer->GetPawn())
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	}
	if (NewPlayer->GetPawn() && !MustSpectate(NewPlayer))
	{
		PlayerSetupComponent->PreparePlayerForGameplay(NewPlayer);
	}
	TryStartServerMatch();
}

// 로비에서 배정받은 스폰 위치를 우선 사용하고, 없으면 엔진의 기본 선택을 따른다.
AActor* AExperienceGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (AActor* ConfiguredPlayerStart = bUseLobbySpawnIndexPlayerStarts
		? SpawnComponent->ChooseConfiguredPlayerStart(Player, LobbySpawnPlayerStartTagPrefix) : nullptr)
	{
		return ConfiguredPlayerStart;
	}

	AActor* PlayerStart = Super::ChoosePlayerStart_Implementation(Player);
	SpawnComponent->MarkPlayerStartUsed(Player, PlayerStart);
	return PlayerStart;
}

// 로딩 중에는 스폰을 막고, 준비된 Experience의 Pawn 또는 허용된 기본 Pawn을 선택한다.
UClass* AExperienceGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (!CanStartGameplay())
	{
		return nullptr;
	}

	const UExperienceManagerComponent* ExperienceManager = GetExperienceManager();
	if (GetConfiguredExperienceId().IsValid() && ExperienceManager && ExperienceManager->IsExperienceLoaded())
	{
		const UExperienceDefinition* Experience = ExperienceManager->GetCurrentExperienceChecked();
		if (Experience->DefaultPawnClass)
		{
			return Experience->DefaultPawnClass;
		}
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

// Pawn을 생성하고 같은 경기의 리스폰에 사용할 최초 위치를 기록한다.
APawn* AExperienceGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	if (!CanStartGameplay())
	{
		return nullptr;
	}

	APawn* SpawnedPawn = Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);
	if (SpawnedPawn)
	{
		SpawnComponent->RecordInitialSpawn(NewPlayer, SpawnedPawn->GetActorTransform());
	}
	return SpawnedPawn;
}

bool AExperienceGameMode::FindCurrentMatchMapOption(FLobbyMatchMapOption& OutMapOption) const
{
	if (!LoadedLevelDefinition || LoadedLevelDefinition->IngameLevels.IsEmpty())
	{
		return false;
	}

	const UWorld* CurrentWorld = GetWorld();
	const FString CurrentPackageName = CurrentWorld && CurrentWorld->GetOutermost()
		? CurrentWorld->GetOutermost()->GetName()
		: FString();
	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	const bool bHasCurrentLevelContext = !CurrentPackageName.IsEmpty() || !CurrentLevelName.IsEmpty();

	if (const ULobbyRuntimeSubsystem* LobbySubsystem = UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GetGameInstance()))
	{
		const FName SelectedMapKey = LobbySubsystem->GetLobbySelectedMapKey();
		if (!SelectedMapKey.IsNone()
			&& LoadedLevelDefinition->FindIngameLevel(SelectedMapKey, OutMapOption)
			&& (!bHasCurrentLevelContext || DoesMapOptionMatchWorld(OutMapOption, CurrentPackageName, CurrentLevelName)))
		{
			return true;
		}
	}

	for (const FLobbyMatchMapOption& MapOption : LoadedLevelDefinition->IngameLevels)
	{
		if (DoesMapOptionMatchWorld(MapOption, CurrentPackageName, CurrentLevelName))
		{
			OutMapOption = MapOption;
			return true;
		}
	}
	return false;
}

// 참가자의 나가기 요청을 경기 중단·정산 정책에 따라 처리한다.
bool AExperienceGameMode::RequestAbortMatchToTitle(APlayerController* RequestingPlayer)
{
	return MatchFlowComponent->RequestAbortMatchToTitle(RequestingPlayer);
}

// 필수 경기 콘텐츠와 지정된 Experience가 모두 준비된 뒤 입장을 허용한다.
bool AExperienceGameMode::CanStartGameplay() const
{
	if (!IsRuntimeContentReady()) { return false; }
	if (!GetConfiguredExperienceId().IsValid())
	{
		return true;
	}

	const UExperienceManagerComponent* ExperienceManager = GetExperienceManager();
	return ExperienceManager && ExperienceManager->IsExperienceLoaded();
}

// 맵에서 지정한 Experience를 로드하고 성공·실패의 후속 입장 처리를 연결한다.
void AExperienceGameMode::StartExperienceLoad()
{
	const FPrimaryAssetId ExperienceId = GetConfiguredExperienceId();
	if (!ExperienceId.IsValid())
	{
		return;
	}

	UExperienceManagerComponent* ExperienceManager = GetExperienceManager();
	if (!ExperienceManager)
	{
		HandleExperienceLoadFailed(ExperienceId, TEXT("AExperienceGameState or ExperienceManagerComponent is missing."));
		return;
	}

	ExperienceManager->CallOrRegister_OnExperienceLoaded(
		FOnPdExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::HandleExperienceLoaded));
	ExperienceManager->CallOrRegister_OnExperienceLoadFailed(
		FOnPdExperienceLoadFailed::FDelegate::CreateUObject(this, &ThisClass::HandleExperienceLoadFailed));
	if (ExperienceManager->GetLoadState() == EExperienceLoadState::Unloaded)
	{
		ExperienceManager->SetCurrentExperienceAuth(ExperienceId);
	}
}

// Experience를 기다리던 참가자의 입장을 재개한다.
void AExperienceGameMode::HandleExperienceLoaded(const UExperienceDefinition* Experience)
{
	ResumeStartingPlayers();
}

// 필수 Experience 실패 시 입장과 경기 시작을 중단한다.
void AExperienceGameMode::HandleExperienceLoadFailed(FPrimaryAssetId ExperienceId, const FString& FailureMessage)
{
	UE_LOG(PdExperienceGameModeLog, Error, TEXT("Experience load failed. Experience=%s Reason=%s"), *ExperienceId.ToString(), *FailureMessage);
}

// 현재 맵이 사용할 Experience 식별자를 월드 설정에서 읽는다.
FPrimaryAssetId AExperienceGameMode::GetConfiguredExperienceId() const
{
	return APdWorldSettings::FindDefaultExperienceId(GetWorld());
}

// 경기에서 공유하는 필수 Definition을 한 번 로드하고 맵 수명 동안 소유한다.
void AExperienceGameMode::BeginRuntimeContentPreload()
{
	ReleaseRuntimeContentPreload();
	const FProjectDefinitionReferences& Definitions = UPdGameInstanceDefinition::GetConfiguredDefinitionReferences();
	TArray<FSoftObjectPath> Paths;
	for (const FSoftObjectPath& Path : {Definitions.MatchRule.ToSoftObjectPath(),
		Definitions.LevelDefinition.ToSoftObjectPath(), Definitions.DefaultProvision.ToSoftObjectPath()})
	{
		if (!Path.IsNull()) { Paths.AddUnique(Path); }
	}
	if (Paths.IsEmpty())
	{
		HandleRuntimeContentPreloadComplete();
		return;
	}
	UContentDataSubsystem* ContentSubsystem = UGameInstance::GetSubsystem<UContentDataSubsystem>(GetGameInstance());
	if (!ContentSubsystem)
	{
		HandleRuntimeContentPreloadComplete();
		return;
	}
	RuntimeContentLease = ContentSubsystem->AcquireContent(Paths,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleRuntimeContentPreloadComplete));
}

void AExperienceGameMode::HandleRuntimeContentPreloadComplete()
{
	const FProjectDefinitionReferences& Definitions = UPdGameInstanceDefinition::GetConfiguredDefinitionReferences();
	LoadedMatchRuleDefinition = Definitions.MatchRule.Get();
	LoadedLevelDefinition = Definitions.LevelDefinition.Get();
	LoadedDefaultProvisionDefinition = Definitions.DefaultProvision.Get();
	if (!IsRuntimeContentReady())
	{
		UE_LOG(PdExperienceGameModeLog, Error, TEXT("Required match definitions failed to load. Player and match start are blocked."));
		return;
	}
	SpawnComponent->Initialize(LoadedMatchRuleDefinition);
	MatchFlowComponent->InitializeGameState();
	PlayerSetupComponent->InitializeRuntime();
	RewardComponent->PreloadRewardContent();
	ResumeStartingPlayers();
}

void AExperienceGameMode::ReleaseRuntimeContentPreload()
{
	RuntimeContentLease.Reset();
	LoadedMatchRuleDefinition = nullptr;
	LoadedLevelDefinition = nullptr;
	LoadedDefaultProvisionDefinition = nullptr;
}

// GameState가 소유한 Experience 로딩 상태를 조회한다.
UExperienceManagerComponent* AExperienceGameMode::GetExperienceManager() const
{
	const AExperienceGameState* ExperienceGameState = GetGameState<AExperienceGameState>();
	return ExperienceGameState ? ExperienceGameState->GetExperienceManagerComponent() : nullptr;
}

// 엔진과 Blueprint의 참가·관전 정책을 그대로 거쳐 대기 중인 플레이어를 재개한다.
void AExperienceGameMode::ResumeStartingPlayers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (APlayerController* Player = Iterator->Get(); IsValid(Player))
		{
			HandleStartingNewPlayer(Player);
		}
	}
	TryStartServerMatch();
}

// RPG 게임 세션은 이 맵이 열려 접속을 받을 수 있을 때 활성화한다. 그 전에는 백엔드가 player session을 만들지 않는다.
// Experience 로딩은 기다리지 않는다. 로딩 중에 들어온 참가자는 로비에서 넘어온 참가자처럼 준비가 끝난 뒤 스폰된다.
void AExperienceGameMode::NotifyRpgWorldReadyIfNeeded()
{
	if (GetNetMode() != NM_DedicatedServer || !MatchFlowComponent->IsRpgMode())
	{
		return;
	}

	if (UGameLiftServerSubsystem* GameLift = UGameLiftServerSubsystem::Get(this))
	{
		GameLift->ActivatePendingGameSession();
	}
}

// 현재 입장한 참가자의 Pawn과 기본 지급이 모두 준비됐을 때 경기 시간을 한 번만 시작한다.
void AExperienceGameMode::TryStartServerMatch()
{
	if (!HasActorBegunPlay() || !CanStartGameplay()
		|| MatchFlowComponent->IsGameResultShown())
	{
		return;
	}

	bool bHasParticipant = false;
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* Player = Iterator->Get();
		if (!IsValid(Player) || Player->IsActorBeingDestroyed() || MustSpectate(Player))
		{
			continue;
		}

		bHasParticipant = true;
		if (!PlayerSetupComponent->IsPlayerReadyForGameplay(Player))
		{
			return;
		}
	}
	if (bHasParticipant)
	{
		MatchFlowComponent->StartServerMatchTimerIfNeeded();
	}
}
