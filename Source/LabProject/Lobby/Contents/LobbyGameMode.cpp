#include "Lobby/Contents/LobbyGameMode.h"

#include "Character/PdPlayer.h"
#include "Component/Lobby/LobbyConfigurationComponent.h"
#include "Component/Lobby/LobbyExperienceComponent.h"
#include "Component/Lobby/LobbyPlayerCoordinatorComponent.h"
#include "Component/Lobby/LobbyRespawnComponent.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Lobby/Contents/LobbyHUD.h"
#include "Lobby/Contents/LobbyPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Lobby/Coordination/LobbyMatchCoordinator.h"
#include "Lobby/Coordination/LobbyTravelCoordinator.h"
#include "Mode/PdPlayerController.h"
#include "Provision/DefaultPlayerProvisioner.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyGameMode)

DEFINE_LOG_CATEGORY_STATIC(LogLobbyGameMode, Log, All);

// 로비의 필수 처리 객체와 기본 프레임워크 클래스를 구성한다.
ALobbyGameMode::ALobbyGameMode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	LobbyConfigurationComponent = CreateDefaultSubobject<ULobbyConfigurationComponent>(TEXT("LobbyConfigurationComponent"));
	LobbyExperienceComponent = CreateDefaultSubobject<ULobbyExperienceComponent>(TEXT("LobbyExperienceComponent"));
	LobbyPlayerCoordinatorComponent = CreateDefaultSubobject<ULobbyPlayerCoordinatorComponent>(TEXT("LobbyPlayerCoordinatorComponent"));
	LobbyRespawnComponent = CreateDefaultSubobject<ULobbyRespawnComponent>(TEXT("LobbyRespawnComponent"));
	MatchCoordinator = CreateDefaultSubobject<ULobbyMatchCoordinator>(TEXT("LobbyMatchCoordinator"));
	DefaultPlayerProvisioner = CreateDefaultSubobject<UDefaultPlayerProvisioner>(TEXT("DefaultPlayerProvisioner"));
	TravelCoordinator = CreateDefaultSubobject<ULobbyTravelCoordinator>(TEXT("LobbyTravelCoordinator"));

	PlayerControllerClass = ALobbyPlayerController::StaticClass();
	GameStateClass = ALobbyGameState::StaticClass();
	PlayerStateClass = APdPlayerState::StaticClass();
	HUDClass = ALobbyHUD::StaticClass();
	DefaultPawnClass = APdPlayer::StaticClass();
	bUseSeamlessTravel = true;
}

// GameState와 플레이어 객체가 만들어지기 전에 잘못된 클래스 설정을 확인한다.
void ALobbyGameMode::PreInitializeComponents()
{
	EnsureLobbyFrameworkClasses();
	Super::PreInitializeComponents();
}

// 로비 설정을 준비한 뒤 선택한 맵과 서버의 세션 광고 정보를 맞춘다.
void ALobbyGameMode::BeginPlay()
{
	Super::BeginPlay();
	LobbyConfigurationComponent->InitializeRuntime(FSimpleDelegate::CreateWeakLambda(this, [this]()
	{
		if (!DefaultPlayerProvisioner->Initialize(
			LobbyConfigurationComponent->GetDefaultProvisionDefinition(), EDefaultProvisionMode::Lobby))
		{
			return;
		}
		LobbyConfigurationComponent->ApplyDefaultLobbyConfigIfNeeded();
		LobbyConfigurationComponent->SyncSelectedLobbyConfigToRuntime();
		MatchCoordinator->UpdateAdvertisedSessionSettingsFromLobbyConfig();
		// Experience와 로비 설정 중 어느 쪽이 먼저 로딩되어도 두 준비가 끝난 뒤 플레이어를 시작한다.
		ResumeWaitingPlayers();
	}));
}

// 로비가 종료되면 조정 객체가 보유한 타이머와 비동기 요청을 정리한다.
void ALobbyGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MatchCoordinator->Shutdown();
	TravelCoordinator->Shutdown();
	DefaultPlayerProvisioner->Shutdown();
	LobbyExperienceComponent->OnExperienceReady.RemoveAll(this);
	Super::EndPlay(EndPlayReason);
}

// 생성된 GameState의 Experience 관리자를 통해 로비 콘텐츠 준비를 시작한다.
void ALobbyGameMode::InitGameState()
{
	Super::InitGameState();
	LobbyExperienceComponent->OnExperienceReady.AddUObject(this, &ThisClass::ResumeWaitingPlayers);
	LobbyExperienceComponent->StartExperienceLoad();
}

// 엔진의 접속 승인을 유지하면서 현재 로비 설정의 인원 제한을 적용한다.
void ALobbyGameMode::PreLogin(
	const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (ErrorMessage.IsEmpty() && GameState && GameState->PlayerArray.Num() >= LobbyConfigurationComponent->GetConfiguredMaxPlayerCount())
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

	LobbyPlayerCoordinatorComponent->InitializeLobbyPlayerState(PlayerController, LobbyPlayerState);
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
	if (UClass* ExperiencePawnClass = LobbyExperienceComponent->ResolveExperiencePawnClass())
	{
		return ExperiencePawnClass;
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

// 퇴장 플레이어의 지급 상태를 정리한다. UI의 목록 갱신은 GameState의 제거 알림이 담당한다.
void ALobbyGameMode::Logout(AController* Exiting)
{
	DefaultPlayerProvisioner->ClearRuntimeStateForController(Exiting, Exiting ? Exiting->PlayerState : nullptr);
	Super::Logout(Exiting);
}

// 호스트가 확정한 로비 설정을 적용한다.
void ALobbyGameMode::SaveConfig(const FName MapKey, const int32 InMaxPlayerCount, const int32 InMaxBotCount)
{
	LobbyConfigurationComponent->SaveConfig(MapKey, InMaxPlayerCount, InMaxBotCount);
}

// 호스트의 시작 요청을 인원과 팀 조건을 검증하는 담당 객체에 전달한다.
void ALobbyGameMode::TryStartGame()
{
	MatchCoordinator->TryStartGame();
}

// 호스트의 시작 버튼이 현재 콘텐츠와 로비 조건을 만족하는지 조회한다.
bool ALobbyGameMode::CanHostStartGame() const
{
	return MatchCoordinator->CanHostStartGame();
}

// 팀 변경 시 진행 중인 시작 카운트다운을 취소하게 한다.
void ALobbyGameMode::NotifyLobbyTeamChanged()
{
	MatchCoordinator->NotifyLobbyTeamChanged();
}

// 호스트가 지정한 플레이어의 강퇴를 처리한다.
void ALobbyGameMode::KickPlayer(APdPlayerState* TargetPlayerState)
{
	LobbyPlayerCoordinatorComponent->KickPlayer(TargetPlayerState);
}

// 로비에서 사망한 플레이어의 부활을 요청한다.
void ALobbyGameMode::RequestLobbyPlayerRespawn(AController* PlayerController, APawn* DeadPawn)
{
	LobbyRespawnComponent->RequestLobbyPlayerRespawn(PlayerController, DeadPawn);
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
		&& LobbyExperienceComponent->IsExperienceLoaded();
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

// 유효한 파생 BP 클래스는 유지하고, 로비 계약을 어긴 설정만 생성 전에 경고와 함께 보정한다.
void ALobbyGameMode::EnsureLobbyFrameworkClasses()
{
	if (!PlayerControllerClass || !PlayerControllerClass->IsChildOf(ALobbyPlayerController::StaticClass()))
	{
		UE_LOG(LogLobbyGameMode, Warning, TEXT("PlayerControllerClass '%s' is not a ALobbyPlayerController; using the native lobby default."),
			*GetNameSafe(PlayerControllerClass.Get()));
		PlayerControllerClass = ALobbyPlayerController::StaticClass();
	}
	if (!GameStateClass || !GameStateClass->IsChildOf(ALobbyGameState::StaticClass()))
	{
		UE_LOG(LogLobbyGameMode, Warning, TEXT("GameStateClass '%s' is not a ALobbyGameState; using the native lobby default."),
			*GetNameSafe(GameStateClass.Get()));
		GameStateClass = ALobbyGameState::StaticClass();
	}
	if (!PlayerStateClass || !PlayerStateClass->IsChildOf(APdPlayerState::StaticClass()))
	{
		UE_LOG(LogLobbyGameMode, Warning, TEXT("PlayerStateClass '%s' is not an APdPlayerState; using the native lobby default."),
			*GetNameSafe(PlayerStateClass.Get()));
		PlayerStateClass = APdPlayerState::StaticClass();
	}
	if (!HUDClass || !HUDClass->IsChildOf(ALobbyHUD::StaticClass()))
	{
		UE_LOG(LogLobbyGameMode, Warning, TEXT("HUDClass '%s' is not a ALobbyHUD; using the native lobby default."),
			*GetNameSafe(HUDClass.Get()));
		HUDClass = ALobbyHUD::StaticClass();
	}
	if (!DefaultPawnClass || !DefaultPawnClass->IsChildOf(APdPlayer::StaticClass()))
	{
		UE_LOG(LogLobbyGameMode, Warning, TEXT("DefaultPawnClass '%s' is not a APdPlayer; using the native lobby default."),
			*GetNameSafe(DefaultPawnClass.Get()));
		DefaultPawnClass = APdPlayer::StaticClass();
	}
}
