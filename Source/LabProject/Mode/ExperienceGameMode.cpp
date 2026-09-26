#include "Mode/ExperienceGameMode.h"

#include "Character/PdPlayer.h"
#include "Common/GameSessionConstants.h"
#include "Component/Experience/ExperienceManagerComponent.h"
#include "Component/Match/MatchFlowComponent.h"
#include "Component/Match/MatchPlayerSetupComponent.h"
#include "Component/Player/PlayerSpawnComponent.h"
#include "Component/Player/SelectingPandoraAndWeaponComponent.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Definition/Experience/ExperienceDefinition.h"
#include "Definition/Level/LevelDefinition.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Definition/Provision/DefaultProvisionDefinition.h"
#include "Engine/World.h"
#include "Experience/PdWorldSettings.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Mode/ExperienceGameState.h"
#include "Mode/PdHUD.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExperienceGameMode)

DEFINE_LOG_CATEGORY(PdExperienceGameModeLog);

// 경기에서 사용할 기본 프레임워크 클래스와 필수 컴포넌트를 구성한다.
AExperienceGameMode::AExperienceGameMode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	GameStateClass = AExperienceGameState::StaticClass();
	PlayerControllerClass = APdPlayerController::StaticClass();
	PlayerStateClass = APdPlayerState::StaticClass();
	DefaultPawnClass = APdPlayer::StaticClass();
	HUDClass = APdHUD::StaticClass();
	bUseSeamlessTravel = true;

	// 기존 Blueprint의 컴포넌트 기본값 연결을 보존하기 위해 직렬화된 서브오브젝트 이름은 유지한다.
	MatchFlowComponent = CreateDefaultSubobject<UMatchFlowComponent>(TEXT("ExperienceMatchFlow"));
	SpawnComponent = CreateDefaultSubobject<UPlayerSpawnComponent>(TEXT("ExperienceSpawn"));
	PlayerSetupComponent = CreateDefaultSubobject<UMatchPlayerSetupComponent>(TEXT("ExperiencePlayerProvisioning"));
	check(MatchFlowComponent && SpawnComponent && PlayerSetupComponent);
}

// 맵 설정을 전달하고, 비동기 준비가 끝날 때마다 경기 시작 조건을 다시 확인한다.
void AExperienceGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	MatchFlowComponent->OnRuntimeContentReady.AddUObject(this, &ThisClass::HandleRuntimeContentReady);
	PlayerSetupComponent->OnPlayerGameplayReady.AddUObject(this, &ThisClass::TryStartServerMatch);
	MatchFlowComponent->InitializeRuntime();
	MatchFlowComponent->InitializeTravelOptions(Options);
}

// 액터의 BeginPlay가 모두 끝난 다음 경기 시작과 보상 상자 배치를 확인한다.
void AExperienceGameMode::BeginPlay()
{
	Super::BeginPlay();
	GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::TryStartServerMatch);
	GetWorldTimerManager().SetTimerForNextTick(
		MatchFlowComponent.Get(), &UMatchFlowComponent::ConfigureRewardChestSpawns);
}

// 맵을 떠난 뒤 준비 완료 콜백이 경기를 시작하지 않도록 연결을 정리한다.
void AExperienceGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MatchFlowComponent->OnRuntimeContentReady.RemoveAll(this);
	PlayerSetupComponent->OnPlayerGameplayReady.RemoveAll(this);
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
}

// 복제할 경기 정보를 초기화하고 이 맵의 Experience 로딩을 시작한다.
void AExperienceGameMode::InitGameState()
{
	Super::InitGameState();
	MatchFlowComponent->InitializeGameState();
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
	if (MatchFlowComponent->FindCurrentMatchMapOption(MapOption))
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

	// 엔진의 컨트롤러 목록에서도 퇴장자가 제거된 뒤 남은 참가자의 준비를 확인한다.
	GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::TryStartServerMatch);
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

// 처치 결과를 경기의 득점·승리 판정에 전달한다.
void AExperienceGameMode::NotifyPlayerKillScored(APlayerState* KillerPlayerState, APlayerState* VictimPlayerState)
{
	MatchFlowComponent->NotifyPlayerKillScored(KillerPlayerState, VictimPlayerState);
}

// 참가자의 나가기 요청을 경기 중단·정산 정책에 따라 처리한다.
bool AExperienceGameMode::RequestAbortMatchToTitle(APlayerController* RequestingPlayer)
{
	return MatchFlowComponent->RequestAbortMatchToTitle(RequestingPlayer);
}

// 필수 경기 콘텐츠와 지정된 Experience가 모두 준비된 뒤 입장을 허용한다.
bool AExperienceGameMode::CanStartGameplay() const
{
	if (!MatchFlowComponent->IsRuntimeContentReady()) { return false; }
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
	const UWorld* World = GetWorld();
	const APdWorldSettings* Settings = World ? Cast<APdWorldSettings>(World->GetWorldSettings()) : nullptr;
	return Settings ? Settings->GetDefaultExperienceId() : FPrimaryAssetId();
}

// 콘텐츠 준비가 끝나면 각 실행 객체를 초기화하고 대기 중인 입장을 재개한다.
void AExperienceGameMode::HandleRuntimeContentReady()
{
	const UMatchRuleDefinition* Rules = MatchFlowComponent->GetMatchRuleDefinition();
	SpawnComponent->Initialize(Rules, Rules->bUseRandomPlayerStartRespawns
		? EPlayerRespawnLocation::RandomPlayerStart : EPlayerRespawnLocation::InitialSpawn);
	PlayerSetupComponent->InitializeRuntime();
	ResumeStartingPlayers();
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

// 현재 입장한 참가자의 Pawn과 기본 지급이 모두 준비됐을 때 경기 시간을 한 번만 시작한다.
void AExperienceGameMode::TryStartServerMatch()
{
	if (!HasActorBegunPlay() || !CanStartGameplay()
		|| !MatchFlowComponent->IsRuntimeContentReady() || MatchFlowComponent->IsGameResultShown())
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
