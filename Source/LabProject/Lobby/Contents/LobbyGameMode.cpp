#include "Lobby/Contents/LobbyGameMode.h"

#include "Character/PdPlayer.h"
#include "Common/GameSessionConstants.h"
#include "Component/Lobby/LobbyConfigurationComponent.h"
#include "Component/Lobby/LobbyExperienceComponent.h"
#include "Component/Lobby/LobbyPlayerCoordinatorComponent.h"
#include "Component/Lobby/LobbyRespawnComponent.h"
#include "Definition/Lobby/LobbyModeDefinition.h"
#include "Definition/Lobby/LobbyPreviewDefinition.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Lobby/Contents/LobbyHUD.h"
#include "Lobby/Contents/LobbyPlayerController.h"
#include "Lobby/Contents/LobbyPlayerState.h"
#include "Lobby/Coordination/LobbyMatchCoordinator.h"
#include "Lobby/Coordination/LobbyTravelCoordinator.h"
#include "Lobby/Services/LobbyPreviewGrantService.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyGameMode)

ALobbyGameMode::ALobbyGameMode(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LobbyConfigurationComponent =
		CreateDefaultSubobject<
			ULobbyConfigurationComponent>(
			TEXT("LobbyConfigurationComponent"));
	LobbyExperienceComponent =
		CreateDefaultSubobject<ULobbyExperienceComponent>(
			TEXT("LobbyExperienceComponent"));
	LobbyPlayerCoordinatorComponent =
		CreateDefaultSubobject<
			ULobbyPlayerCoordinatorComponent>(
			TEXT("LobbyPlayerCoordinatorComponent"));
	LobbyRespawnComponent =
		CreateDefaultSubobject<ULobbyRespawnComponent>(
			TEXT("LobbyRespawnComponent"));

	MatchCoordinator =
		CreateDefaultSubobject<ULobbyMatchCoordinator>(
			TEXT("LobbyMatchCoordinator"));
	PreviewGrantService =
		CreateDefaultSubobject<ULobbyPreviewGrantService>(
			TEXT("LobbyPreviewGrantService"));
	TravelCoordinator =
		CreateDefaultSubobject<ULobbyTravelCoordinator>(
			TEXT("LobbyTravelCoordinator"));

	EnsureLobbyFrameworkClasses();
	bUseSeamlessTravel = true;
}

void ALobbyGameMode::BeginPlay()
{
	EnsureLobbyFrameworkClasses();
	Super::BeginPlay();

#if WITH_EDITOR
	if (UWorld* World = GetWorld();
		World
		&& World->WorldType == EWorldType::PIE
		&& bUseSeamlessTravel)
	{
		bUseSeamlessTravel = false;
	}
#endif

	if (LobbyConfigurationComponent)
	{
		LobbyConfigurationComponent->InitializeRuntime(
			FSimpleDelegate::CreateWeakLambda(this, [this]()
			{
				if (LobbyConfigurationComponent)
				{
					LobbyConfigurationComponent
						->ApplyDefaultLobbyConfigIfNeeded();
					LobbyConfigurationComponent
						->SyncSelectedLobbyConfigToRuntime();
				}
				if (MatchCoordinator)
				{
					MatchCoordinator->InitializeSession();
				}
			}));
	}
	else if (MatchCoordinator)
	{
		MatchCoordinator->InitializeSession();
	}
}

void ALobbyGameMode::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (MatchCoordinator)
	{
		MatchCoordinator->Shutdown();
	}
	if (TravelCoordinator)
	{
		TravelCoordinator->Shutdown();
	}
	if (PreviewGrantService)
	{
		PreviewGrantService->Shutdown();
	}
	if (LobbyPlayerCoordinatorComponent)
	{
		LobbyPlayerCoordinatorComponent->Shutdown();
	}
	if (LobbyRespawnComponent)
	{
		LobbyRespawnComponent->Shutdown();
	}

	Super::EndPlay(EndPlayReason);
}

void ALobbyGameMode::InitGameState()
{
	EnsureLobbyFrameworkClasses();
	Super::InitGameState();

	if (LobbyExperienceComponent)
	{
		LobbyExperienceComponent
			->StartExperienceLoad();
	}
}

void ALobbyGameMode::PreLogin(
	const FString& Options,
	const FString& Address,
	const FUniqueNetIdRepl& UniqueId,
	FString& ErrorMessage)
{
	Super::PreLogin(
		Options,
		Address,
		UniqueId,
		ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	const int32 CurrentPlayerCount = GameState
		? GameState->PlayerArray.Num()
		: 0;
	if (CurrentPlayerCount
		>= GetConfiguredMaxPlayerCount())
	{
		ErrorMessage = TEXT("Server is full.");
	}
}

void ALobbyGameMode::PostLogin(
	APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	ALobbyPlayerState* LobbyPlayerState =
		GetLobbyPlayerState(NewPlayer);
	if (!LobbyPlayerState)
	{
		return;
	}

	if (LobbyPlayerCoordinatorComponent)
	{
		LobbyPlayerCoordinatorComponent
			->InitializeLobbyPlayerState(
				NewPlayer,
				LobbyPlayerState);
	}
	RefreshLobbyUIForAllPlayers();
	if (MatchCoordinator)
	{
		MatchCoordinator
			->UpdateFullLobbyAutoStartTimer();
	}
	if (PreviewGrantService)
	{
		PreviewGrantService->ScheduleGrant(NewPlayer);
	}
}

void ALobbyGameMode::
HandleStartingNewPlayer_Implementation(
	APlayerController* NewPlayer)
{
	if (LobbyExperienceComponent
		&& LobbyExperienceComponent
			->ShouldDelayPlayerStart())
	{
		return;
	}

	Super::HandleStartingNewPlayer_Implementation(
		NewPlayer);
	if (PreviewGrantService)
	{
		PreviewGrantService->ScheduleGrant(NewPlayer);
	}
	const ALobbyGameState* LobbyGameState =
		GetGameState<ALobbyGameState>();
	if (LobbyGameState
		&& LobbyGameState->IsGameStartPending()
		&& TravelCoordinator)
	{
		TravelCoordinator->SetLobbyPawnTravelLocked(
			NewPlayer,
			true);
	}
}

UClass* ALobbyGameMode::
GetDefaultPawnClassForController_Implementation(
	AController* InController)
{
	if (LobbyExperienceComponent)
	{
		if (LobbyExperienceComponent
			->ShouldDelayPlayerStart())
		{
			return nullptr;
		}
		if (UClass* ExperiencePawnClass =
			LobbyExperienceComponent
				->ResolveExperiencePawnClass())
		{
			return ExperiencePawnClass;
		}
	}

	return Super::
		GetDefaultPawnClassForController_Implementation(
			InController);
}

void ALobbyGameMode::Logout(AController* Exiting)
{
	if (PreviewGrantService)
	{
		PreviewGrantService->HandlePlayerLogout(Exiting);
	}
	if (LobbyRespawnComponent)
	{
		LobbyRespawnComponent->HandlePlayerLogout(
			Exiting);
	}

	Super::Logout(Exiting);

	if (LobbyPlayerCoordinatorComponent)
	{
		LobbyPlayerCoordinatorComponent
			->HandlePlayerLogout(Exiting);
	}
}

void ALobbyGameMode::SaveConfig(
	const FName MapKey,
	const int32 InMaxPlayerCount,
	const int32 InMaxBotCount)
{
	if (LobbyConfigurationComponent)
	{
		LobbyConfigurationComponent->SaveConfig(
			MapKey,
			InMaxPlayerCount,
			InMaxBotCount);
	}
}

void ALobbyGameMode::TryStartGame()
{
	if (MatchCoordinator)
	{
		MatchCoordinator->TryStartGame();
	}
}

bool ALobbyGameMode::CanHostStartGame() const
{
	return MatchCoordinator
		&& MatchCoordinator->CanHostStartGame();
}

bool ALobbyGameMode::AreLobbyTeamsBalanced() const
{
	return MatchCoordinator
		&& MatchCoordinator->AreLobbyTeamsBalanced();
}

void ALobbyGameMode::NotifyLobbyTeamChanged()
{
	if (MatchCoordinator)
	{
		MatchCoordinator->NotifyLobbyTeamChanged();
	}
}

void ALobbyGameMode::KickPlayer(
	ALobbyPlayerState* TargetPlayerState)
{
	if (LobbyPlayerCoordinatorComponent)
	{
		LobbyPlayerCoordinatorComponent->KickPlayer(
			TargetPlayerState);
	}
}

void ALobbyGameMode::RequestLobbyPlayerRespawn(
	AController* PlayerController,
	APawn* DeadPawn)
{
	if (LobbyRespawnComponent)
	{
		LobbyRespawnComponent
			->RequestLobbyPlayerRespawn(
				PlayerController,
				DeadPawn);
	}
}

FString ALobbyGameMode::GetRoomTravelMapName() const
{
	return LobbyConfigurationComponent
		? LobbyConfigurationComponent
			->GetRoomTravelMapName()
		: FString();
}

FString ALobbyGameMode::ResolveTravelMapName(
	const FName MapKey) const
{
	return LobbyConfigurationComponent
		? LobbyConfigurationComponent
			->ResolveTravelMapName(MapKey)
		: FString();
}

FName ALobbyGameMode::GetFirstMapKey() const
{
	return LobbyConfigurationComponent
		? LobbyConfigurationComponent->GetFirstMapKey()
		: NAME_None;
}

int32 ALobbyGameMode::GetLobbyMapOptionCount() const
{
	return LobbyConfigurationComponent
		? LobbyConfigurationComponent
			->GetLobbyMapOptionCount()
		: 0;
}

bool ALobbyGameMode::GetLobbyMapOptionAtIndex(
	const int32 Index,
	FLobbyMatchMapOption& OutMapOption) const
{
	return LobbyConfigurationComponent
		&& LobbyConfigurationComponent
			->GetLobbyMapOptionAtIndex(
				Index,
				OutMapOption);
}

bool ALobbyGameMode::GetSelectedLobbyMapOption(
	FLobbyMatchMapOption& OutMapOption) const
{
	return LobbyConfigurationComponent
		&& LobbyConfigurationComponent
			->GetSelectedLobbyMapOption(OutMapOption);
}

FName ALobbyGameMode::GetSelectedLobbyMapKey() const
{
	return LobbyConfigurationComponent
		? LobbyConfigurationComponent
			->GetSelectedLobbyMapKey()
		: NAME_None;
}

int32 ALobbyGameMode::
GetSelectedLobbyMaxPlayerCount() const
{
	return LobbyConfigurationComponent
		? LobbyConfigurationComponent
			->GetSelectedLobbyMaxPlayerCount()
		: LabGameSession::MaxPlayerCount;
}

void ALobbyGameMode::SelectLobbyMapByOffset(
	const int32 Offset)
{
	if (LobbyConfigurationComponent)
	{
		LobbyConfigurationComponent
			->SelectLobbyMapByOffset(Offset);
	}
}

const ULobbyModeDefinition*
ALobbyGameMode::GetLobbyModeDefinition() const
{
	return LobbyConfigurationComponent
		? LobbyConfigurationComponent
			->GetLobbyModeDefinition()
		: GetDefault<ULobbyModeDefinition>();
}

const UMatchRuleDefinition*
ALobbyGameMode::GetMatchRuleDefinition() const
{
	return LobbyConfigurationComponent
		? LobbyConfigurationComponent
			->GetMatchRuleDefinition()
		: GetDefault<UMatchRuleDefinition>();
}

const ULobbyPreviewDefinition*
ALobbyGameMode::GetLobbyPreviewDefinition() const
{
	return LobbyConfigurationComponent
		? LobbyConfigurationComponent
			->GetLobbyPreviewDefinition()
		: GetDefault<ULobbyPreviewDefinition>();
}

float ALobbyGameMode::GetFullLobbyAutoStartDelay() const
{
	const ULobbyModeDefinition* Definition =
		GetLobbyModeDefinition();
	return Definition
		? FMath::Max(
			Definition->GetFlowSettings()
				.FullLobbyAutoStartDelay,
			0.0f)
		: 0.0f;
}

bool ALobbyGameMode::
ShouldAutoCreateDedicatedServerSession() const
{
	const ULobbyModeDefinition* Definition =
		GetLobbyModeDefinition();
	return Definition
		&& Definition->GetDedicatedSessionSettings()
			.bAutoCreateDedicatedServerSession;
}

FString ALobbyGameMode::GetDedicatedServerRoomName() const
{
	const ULobbyModeDefinition* Definition =
		GetLobbyModeDefinition();
	return Definition
		? Definition->GetDedicatedSessionSettings()
			.DedicatedServerRoomName
		: FString();
}

bool ALobbyGameMode::IsDedicatedServerSessionLAN() const
{
	const ULobbyModeDefinition* Definition =
		GetLobbyModeDefinition();
	return Definition
		&& Definition->GetDedicatedSessionSettings()
			.bDedicatedServerSessionLAN;
}

void ALobbyGameMode::EnsureLobbyFrameworkClasses()
{
	if (!PlayerControllerClass
		|| !PlayerControllerClass->IsChildOf(
			ALobbyPlayerController::StaticClass()))
	{
		PlayerControllerClass =
			ALobbyPlayerController::StaticClass();
	}
	if (!GameStateClass
		|| !GameStateClass->IsChildOf(
			ALobbyGameState::StaticClass()))
	{
		GameStateClass = ALobbyGameState::StaticClass();
	}
	if (!PlayerStateClass
		|| !PlayerStateClass->IsChildOf(
			ALobbyPlayerState::StaticClass()))
	{
		PlayerStateClass =
			ALobbyPlayerState::StaticClass();
	}
	if (!HUDClass
		|| !HUDClass->IsChildOf(
			ALobbyHUD::StaticClass()))
	{
		HUDClass = ALobbyHUD::StaticClass();
	}
	if (!DefaultPawnClass
		|| !DefaultPawnClass->IsChildOf(
			APdPlayer::StaticClass()))
	{
		DefaultPawnClass = APdPlayer::StaticClass();
	}
}

FName ALobbyGameMode::ResolveConfiguredMapKey(
	const FName MapKey) const
{
	return LobbyConfigurationComponent
		? LobbyConfigurationComponent
			->ResolveConfiguredMapKey(MapKey)
		: MapKey;
}

bool ALobbyGameMode::FindConfiguredMapOption(
	const FName MapKey,
	FLobbyMatchMapOption& OutMapOption) const
{
	return LobbyConfigurationComponent
		&& LobbyConfigurationComponent
			->FindConfiguredMapOption(
				MapKey,
				OutMapOption);
}

int32 ALobbyGameMode::GetConfiguredMaxPlayerCount() const
{
	return LobbyConfigurationComponent
		? LobbyConfigurationComponent
			->GetConfiguredMaxPlayerCount()
		: LabGameSession::MaxPlayerCount;
}

int32 ALobbyGameMode::GetConfiguredMaxBotCount(
	const FName MapKey) const
{
	return LobbyConfigurationComponent
		? LobbyConfigurationComponent
			->GetConfiguredMaxBotCount(MapKey)
		: 10;
}

ALobbyPlayerState* ALobbyGameMode::GetLobbyPlayerState(
	APlayerController* PlayerController) const
{
	return PlayerController
		? PlayerController
			->GetPlayerState<ALobbyPlayerState>()
		: nullptr;
}

APlayerController* ALobbyGameMode::
ResolvePlayerControllerForPlayerState(
	const APlayerState* PlayerState) const
{
	return LobbyPlayerCoordinatorComponent
		? LobbyPlayerCoordinatorComponent
			->ResolvePlayerControllerForPlayerState(
				PlayerState)
		: nullptr;
}

void ALobbyGameMode::RefreshLobbyUIForAllPlayers()
{
	if (LobbyPlayerCoordinatorComponent)
	{
		LobbyPlayerCoordinatorComponent
			->RefreshLobbyUIForAllPlayers();
	}
}
