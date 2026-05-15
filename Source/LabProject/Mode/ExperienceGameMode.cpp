#include "Mode/ExperienceGameMode.h"

#include "Character/PdPlayer.h"
#include "Engine/World.h"
#include "Experience/ExperienceDefinition.h"
#include "Experience/ExperienceManagerComponent.h"
#include "Experience/PdWorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "Mode/ExperienceGameState.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExperienceGameMode)

DEFINE_LOG_CATEGORY(PdExperienceGameModeLog);

AExperienceGameMode::AExperienceGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = AExperienceGameState::StaticClass();
	PlayerControllerClass = APdPlayerController::StaticClass();
	PlayerStateClass = APdPlayerState::StaticClass();
	DefaultPawnClass = APdPlayer::StaticClass();
}

//----------------------------------------------------------------------------------------------------------------------
//--- Engine Events
void AExperienceGameMode::InitGameState()
{
	Super::InitGameState();

	StartExperienceLoad();
}

void AExperienceGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (bWaitingForExperience && !IsExperienceLoaded())
	{
		UE_LOG(PdExperienceGameModeLog, Log, TEXT("HandleStartingNewPlayer delayed until experience is loaded: %s"), *GetNameSafe(NewPlayer));
		return;
	}

	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

UClass* AExperienceGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (bWaitingForExperience && !IsExperienceLoaded())
	{
		return nullptr;
	}

	if (GetConfiguredExperienceId().IsValid() && IsExperienceLoaded())
	{
		if (const AExperienceGameState* ExperienceGameState = GetGameState<AExperienceGameState>())
		{
			if (const UExperienceManagerComponent* ExperienceManager = ExperienceGameState->GetExperienceManagerComponent())
			{
				const UExperienceDefinition* Experience = ExperienceManager->GetCurrentExperienceChecked();
				if (Experience && Experience->DefaultPawnClass)
				{
					return Experience->DefaultPawnClass;
				}
			}
		}
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

APawn* AExperienceGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	if (bWaitingForExperience && !IsExperienceLoaded())
	{
		UE_LOG(PdExperienceGameModeLog, Log, TEXT("Pawn spawn delayed until experience is loaded: %s"), *GetNameSafe(NewPlayer));
		return nullptr;
	}

	return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);
}

//----------------------------------------------------------------------------------------------------------------------
//--- Experience Flow
bool AExperienceGameMode::IsExperienceLoaded() const
{
	if (!GetConfiguredExperienceId().IsValid())
	{
		return true;
	}

	if (const AExperienceGameState* ExperienceGameState = GetGameState<AExperienceGameState>())
	{
		if (const UExperienceManagerComponent* ExperienceManager = ExperienceGameState->GetExperienceManagerComponent())
		{
			return ExperienceManager->IsExperienceLoaded();
		}
	}

	return false;
}

void AExperienceGameMode::StartExperienceLoad()
{
	const FPrimaryAssetId ExperienceId = GetConfiguredExperienceId();
	if (!ExperienceId.IsValid())
	{
		bWaitingForExperience = false;
		return;
	}

	AExperienceGameState* ExperienceGameState = GetGameState<AExperienceGameState>();
	if (!ExperienceGameState)
	{
		UE_LOG(PdExperienceGameModeLog, Error, TEXT("Experience load failed: APdExperienceGameState is missing."));
		bWaitingForExperience = false;
		return;
	}

	UExperienceManagerComponent* ExperienceManager = ExperienceGameState->GetExperienceManagerComponent();
	if (!ExperienceManager)
	{
		UE_LOG(PdExperienceGameModeLog, Error, TEXT("Experience load failed: ExperienceManagerComponent is missing."));
		bWaitingForExperience = false;
		return;
	}

	bWaitingForExperience = true;
	ExperienceManager->CallOrRegister_OnExperienceLoaded(FOnPdExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::HandleExperienceLoaded));
	ExperienceManager->SetCurrentExperienceAuth(ExperienceId);
}

void AExperienceGameMode::HandleExperienceLoaded(const UExperienceDefinition* Experience)
{
	bWaitingForExperience = false;

	UE_LOG(PdExperienceGameModeLog, Log, TEXT("Experience loaded: %s"), *GetNameSafe(Experience));

	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		if (PlayerController && !PlayerController->GetPawn() && PlayerCanRestart(PlayerController))
		{
			RestartPlayer(PlayerController);
		}
	}
}

FPrimaryAssetId AExperienceGameMode::GetConfiguredExperienceId() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const APdWorldSettings* PdWorldSettings = Cast<APdWorldSettings>(World->GetWorldSettings()))
		{
			if (PdWorldSettings->GetDefaultExperienceId().IsValid())
			{
				return PdWorldSettings->GetDefaultExperienceId();
			}
		}
	}

	return DefaultExperienceId;
}
