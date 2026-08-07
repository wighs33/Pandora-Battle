#include "Mode/ExperienceGameMode.h"

#include "Character/PdPlayer.h"
#include "Component/Experience/ExperienceManagerComponent.h"
#include "Component/Experience/ExperienceMatchFlowComponent.h"
#include "Component/Experience/ExperiencePlayerProvisioningComponent.h"
#include "Component/Experience/ExperienceSpawnComponent.h"
#include "Definition/Experience/ExperienceDefinition.h"
#include "Definition/Provision/DefaultProvisionDefinition.h"
#include "Engine/World.h"
#include "Experience/PdWorldSettings.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Mode/ExperienceGameState.h"
#include "Mode/PdHUD.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExperienceGameMode)

DEFINE_LOG_CATEGORY(PdExperienceGameModeLog);

AExperienceGameMode::AExperienceGameMode(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = AExperienceGameState::StaticClass();
	PlayerControllerClass = APdPlayerController::StaticClass();
	PlayerStateClass = APdPlayerState::StaticClass();
	DefaultPawnClass = APdPlayer::StaticClass();
	HUDClass = APdHUD::StaticClass();
	bUseSeamlessTravel = true;

	MatchFlowComponent =
		CreateDefaultSubobject<UExperienceMatchFlowComponent>(
			TEXT("ExperienceMatchFlow"));
	SpawnComponent =
		CreateDefaultSubobject<UExperienceSpawnComponent>(
			TEXT("ExperienceSpawn"));
	PlayerProvisioningComponent =
		CreateDefaultSubobject<UExperiencePlayerProvisioningComponent>(
			TEXT("ExperiencePlayerProvisioning"));
}

void AExperienceGameMode::InitGame(
	const FString& MapName,
	const FString& Options,
	FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	ApplyRuntimeComponentSettings();

	if (MatchFlowComponent)
	{
		MatchFlowComponent->InitializeTravelOptions(Options);
	}
}

void AExperienceGameMode::BeginPlay()
{
	Super::BeginPlay();

#if WITH_EDITOR
	if (UWorld* World = GetWorld();
		World && World->WorldType == EWorldType::PIE
		&& bUseSeamlessTravel)
	{
		bUseSeamlessTravel = false;
	}
#endif

	if (!HasAuthority() || !MatchFlowComponent)
	{
		return;
	}

	MatchFlowComponent->StartServerMatchTimerIfNeeded();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(
				MatchFlowComponent.Get(),
				&UExperienceMatchFlowComponent::
					ConfigureRewardChestSpawns));
	}
}

void AExperienceGameMode::InitGameState()
{
	Super::InitGameState();

	if (MatchFlowComponent)
	{
		MatchFlowComponent->InitializeGameState();
	}
	StartExperienceLoad();
}

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

void AExperienceGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (PlayerProvisioningComponent)
	{
		PlayerProvisioningComponent->InitializeLoggedInPlayer(NewPlayer);
		PlayerProvisioningComponent->PreparePlayerForGameplay(
			NewPlayer,
			false);
	}
}

void AExperienceGameMode::Logout(AController* Exiting)
{
	APlayerState* ExitingPlayerState =
		Exiting ? Exiting->PlayerState : nullptr;

	// A real disconnect uses the same settlement policy for listen host and
	// remote players. Client RPC requests cannot call this trusted path.
	if (MatchFlowComponent)
	{
		MatchFlowComponent->HandlePlayerLogout(ExitingPlayerState);
	}
	if (SpawnComponent)
	{
		SpawnComponent->ClearRuntimeStateForController(Exiting);
	}
	if (PlayerProvisioningComponent)
	{
		PlayerProvisioningComponent->ClearRuntimeStateForController(
			Exiting,
			ExitingPlayerState);
	}

	Super::Logout(Exiting);
}

void AExperienceGameMode::HandleStartingNewPlayer_Implementation(
	APlayerController* NewPlayer)
{
	if (IsExperienceLoadPending())
	{
		return;
	}

	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	if (PlayerProvisioningComponent)
	{
		PlayerProvisioningComponent->PreparePlayerForGameplay(
			NewPlayer,
			true);
	}
}

AActor* AExperienceGameMode::ChoosePlayerStart_Implementation(
	AController* Player)
{
	if (SpawnComponent)
	{
		if (AActor* ConfiguredPlayerStart =
			SpawnComponent->ChooseConfiguredPlayerStart(
				Player))
		{
			return ConfiguredPlayerStart;
		}
	}

	AActor* PlayerStart =
		Super::ChoosePlayerStart_Implementation(Player);
	if (SpawnComponent)
	{
		SpawnComponent->MarkPlayerStartUsed(Player, PlayerStart);
	}
	return PlayerStart;
}

UClass*
AExperienceGameMode::GetDefaultPawnClassForController_Implementation(
	AController* InController)
{
	if (IsExperienceLoadPending())
	{
		return nullptr;
	}

	if (GetConfiguredExperienceId().IsValid() && IsExperienceLoaded())
	{
		const AExperienceGameState* ExperienceGameState =
			GetGameState<AExperienceGameState>();
		const UExperienceManagerComponent* ExperienceManager =
			ExperienceGameState
				? ExperienceGameState->GetExperienceManagerComponent()
				: nullptr;
		const UExperienceDefinition* Experience =
			ExperienceManager
				? ExperienceManager->GetCurrentExperienceChecked()
				: nullptr;
		if (Experience && Experience->DefaultPawnClass)
		{
			return Experience->DefaultPawnClass;
		}
	}

	return Super::GetDefaultPawnClassForController_Implementation(
		InController);
}

APawn* AExperienceGameMode::
SpawnDefaultPawnAtTransform_Implementation(
	AController* NewPlayer,
	const FTransform& SpawnTransform)
{
	if (IsExperienceLoadPending())
	{
		return nullptr;
	}

	APawn* SpawnedPawn =
		Super::SpawnDefaultPawnAtTransform_Implementation(
			NewPlayer,
			SpawnTransform);
	if (SpawnedPawn && SpawnComponent)
	{
		SpawnComponent->RecordInitialSpawn(
			NewPlayer,
			SpawnedPawn->GetActorTransform());
	}
	return SpawnedPawn;
}

int32 AExperienceGameMode::GrantGameVictoryGoldReward(
	AController* WinnerController,
	const int32 WinningTeamMemberCount)
{
	return MatchFlowComponent
		? MatchFlowComponent->GrantGameVictoryGoldReward(
			WinnerController,
			WinningTeamMemberCount)
		: 0;
}

void AExperienceGameMode::HandleMatchTimerExpired()
{
	if (MatchFlowComponent)
	{
		MatchFlowComponent->HandleMatchTimerExpired();
	}
}

bool AExperienceGameMode::ShowGameResultForWinner(
	APlayerState* WinnerPlayerState)
{
	return MatchFlowComponent
		&& MatchFlowComponent->ShowGameResultForWinner(
			WinnerPlayerState);
}

void AExperienceGameMode::NotifyPlayerKillScored(
	APlayerState* KillerPlayerState,
	APlayerState* VictimPlayerState)
{
	if (MatchFlowComponent)
	{
		MatchFlowComponent->NotifyPlayerKillScored(
			KillerPlayerState,
			VictimPlayerState);
	}
}

bool AExperienceGameMode::RequestAbortMatchToTitle(
	APlayerController* RequestingPlayer)
{
	return MatchFlowComponent
		&& MatchFlowComponent->RequestAbortMatchToTitle(
			RequestingPlayer);
}

void AExperienceGameMode::RequestPlayerRespawn(
	AController* PlayerController,
	APawn* DeadPawn)
{
	if (SpawnComponent)
	{
		SpawnComponent->RequestPlayerRespawn(
			PlayerController,
			DeadPawn);
	}
}

bool AExperienceGameMode::TryGetPlayerInitialSpawnTransform(
	AController* PlayerController,
	FTransform& OutSpawnTransform) const
{
	return SpawnComponent
		&& SpawnComponent->TryGetPlayerInitialSpawnTransform(
			PlayerController,
			OutSpawnTransform);
}

void AExperienceGameMode::
ApplyConfiguredStatusPointsForPlayerState(
	APlayerState* PlayerState)
{
	if (PlayerProvisioningComponent)
	{
		PlayerProvisioningComponent
			->ApplyConfiguredStatusPointsForPlayerState(PlayerState);
	}
}

bool AExperienceGameMode::IsExperienceLoaded() const
{
	if (!GetConfiguredExperienceId().IsValid())
	{
		return true;
	}

	const AExperienceGameState* ExperienceGameState =
		GetGameState<AExperienceGameState>();
	const UExperienceManagerComponent* ExperienceManager =
		ExperienceGameState
			? ExperienceGameState->GetExperienceManagerComponent()
			: nullptr;
	return ExperienceManager && ExperienceManager->IsExperienceLoaded();
}

bool AExperienceGameMode::IsExperienceLoadPending() const
{
	if (!GetConfiguredExperienceId().IsValid())
	{
		return false;
	}

	const AExperienceGameState* ExperienceGameState =
		GetGameState<AExperienceGameState>();
	const UExperienceManagerComponent* ExperienceManager =
		ExperienceGameState
			? ExperienceGameState->GetExperienceManagerComponent()
			: nullptr;
	if (!ExperienceManager)
	{
		// StartExperienceLoad logs this configuration error and falls back
		// to the native pawn path instead of permanently blocking players.
		return false;
	}

	const EExperienceLoadState LoadState =
		ExperienceManager->GetLoadState();
	return LoadState != EExperienceLoadState::Loaded
		&& LoadState != EExperienceLoadState::Failed;
}

void AExperienceGameMode::StartExperienceLoad()
{
	const FPrimaryAssetId ExperienceId = GetConfiguredExperienceId();
	if (!ExperienceId.IsValid())
	{
		return;
	}

	AExperienceGameState* ExperienceGameState =
		GetGameState<AExperienceGameState>();
	if (!ExperienceGameState)
	{
		UE_LOG(
			PdExperienceGameModeLog,
			Error,
			TEXT("Experience load failed: AExperienceGameState is "
				"missing."));
		return;
	}

	UExperienceManagerComponent* ExperienceManager =
		ExperienceGameState->GetExperienceManagerComponent();
	if (!ExperienceManager)
	{
		UE_LOG(
			PdExperienceGameModeLog,
			Error,
			TEXT("Experience load failed: ExperienceManagerComponent "
				"is missing."));
		return;
	}

	ExperienceManager->CallOrRegister_OnExperienceLoaded(
		FOnPdExperienceLoaded::FDelegate::CreateUObject(
			this,
			&ThisClass::HandleExperienceLoaded));
	ExperienceManager->CallOrRegister_OnExperienceLoadFailed(
		FOnPdExperienceLoadFailed::FDelegate::CreateUObject(
			this,
			&ThisClass::HandleExperienceLoadFailed));
	ExperienceManager->SetCurrentExperienceAuth(ExperienceId);
}

void AExperienceGameMode::HandleExperienceLoaded(
	const UExperienceDefinition* Experience)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator Iterator =
		World->GetPlayerControllerIterator();
		Iterator;
		++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		if (PlayerController && !PlayerController->GetPawn()
			&& PlayerCanRestart(PlayerController))
		{
			RestartPlayer(PlayerController);
		}

		if (PlayerController && PlayerProvisioningComponent)
		{
			PlayerProvisioningComponent->PreparePlayerForGameplay(
				PlayerController,
				true);
		}
	}
}

void AExperienceGameMode::HandleExperienceLoadFailed(
	const FPrimaryAssetId ExperienceId,
	const FString& FailureMessage)
{
	UE_LOG(
		PdExperienceGameModeLog,
		Error,
		TEXT("Experience load failed. Experience=%s Reason=%s"),
		*ExperienceId.ToString(),
		*FailureMessage);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// A failed optional Experience falls back to native GameMode classes so a
	// configuration error does not leave connected players without a pawn.
	for (FConstPlayerControllerIterator Iterator =
		World->GetPlayerControllerIterator();
		Iterator;
		++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		if (PlayerController && !PlayerController->GetPawn()
			&& PlayerCanRestart(PlayerController))
		{
			RestartPlayer(PlayerController);
		}

		if (PlayerController && PlayerProvisioningComponent)
		{
			PlayerProvisioningComponent->PreparePlayerForGameplay(
				PlayerController,
				true);
		}
	}
}

FPrimaryAssetId AExperienceGameMode::GetConfiguredExperienceId() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const APdWorldSettings* PdWorldSettings =
			Cast<APdWorldSettings>(World->GetWorldSettings()))
		{
			if (PdWorldSettings->GetDefaultExperienceId().IsValid())
			{
				return PdWorldSettings->GetDefaultExperienceId();
			}
		}
	}

	return DefaultExperienceId;
}

void AExperienceGameMode::ApplyRuntimeComponentSettings()
{
	if (SpawnComponent)
	{
		FExperienceSpawnSettings SpawnSettings;
		SpawnSettings.bUseLobbySpawnIndexPlayerStarts =
			bUseLobbySpawnIndexPlayerStarts;
		SpawnSettings.LobbySpawnPlayerStartTagPrefix =
			LobbySpawnPlayerStartTagPrefix;
		SpawnComponent->ApplySettings(SpawnSettings);
	}

	if (MatchFlowComponent)
	{
		FExperienceMatchFlowSettings MatchFlowSettings;
		MatchFlowSettings.GameVictoryRewardDefinition =
			GameVictoryRewardDefinition;
		MatchFlowSettings.VictoryGoldPerKill = VictoryGoldPerKill;
		MatchFlowSettings.VictoryGoldPenaltyPerDeath =
			VictoryGoldPenaltyPerDeath;
		MatchFlowSettings.VictoryGoldPerWinningTeamMember =
			VictoryGoldPerWinningTeamMember;
		MatchFlowSettings.ChestSpawnRewardDefinition =
			ChestSpawnRewardDefinition;
		MatchFlowSettings.MatchRuleDefinition = MatchRuleDefinition;
		MatchFlowComponent->ApplySettings(MatchFlowSettings);
	}

	if (PlayerProvisioningComponent)
	{
		FExperiencePlayerProvisioningSettings ProvisioningSettings;
		ProvisioningSettings.DefaultProvisionDefinition =
			const_cast<UDefaultProvisionDefinition*>(
				UDefaultProvisionDefinition::ResolveDefaultDefinition());
		ProvisioningSettings.MatchRuleDefinition = MatchRuleDefinition;
		ProvisioningSettings.bAssignDefaultTeamWhenLobbyTeamMissing =
			bAssignDefaultTeamWhenLobbyTeamMissing;
		ProvisioningSettings.DefaultLobbyTeamColorIndex =
			DefaultLobbyTeamColorIndex;
		PlayerProvisioningComponent->ApplySettings(
			ProvisioningSettings);
	}
}
