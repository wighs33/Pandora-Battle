#include "Component/Lobby/LobbyExperienceComponent.h"

#include "Component/Experience/ExperienceManagerComponent.h"
#include "Definition/Experience/ExperienceDefinition.h"
#include "Definition/Lobby/LobbyModeDefinition.h"
#include "Experience/PdWorldSettings.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Lobby/Services/LobbyPreviewGrantService.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyExperienceComponent)

DEFINE_LOG_CATEGORY_STATIC(LogLobbyExperience, Log, All);

ULobbyExperienceComponent::ULobbyExperienceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void ULobbyExperienceComponent::StartExperienceLoad()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	const FPrimaryAssetId ExperienceId =
		GetConfiguredExperienceId();
	if (!GameMode || !ExperienceId.IsValid())
	{
		bWaitingForExperience = false;
		return;
	}

	ALobbyGameState* LobbyGameState =
		GameMode->GetGameState<ALobbyGameState>();
	if (!LobbyGameState)
	{
		UE_LOG(
			LogLobbyExperience,
			Error,
			TEXT("Lobby experience load failed: LobbyGameState is missing."));
		bWaitingForExperience = false;
		return;
	}

	UExperienceManagerComponent* ExperienceManager =
		LobbyGameState->GetExperienceManagerComponent();
	if (!ExperienceManager)
	{
		UE_LOG(
			LogLobbyExperience,
			Error,
			TEXT("Lobby experience load failed: ExperienceManagerComponent is missing."));
		bWaitingForExperience = false;
		return;
	}

	bWaitingForExperience = true;
	ExperienceManager->CallOrRegister_OnExperienceLoaded(
		FOnPdExperienceLoaded::FDelegate::CreateUObject(
			this,
			&ThisClass::HandleExperienceLoaded));
	ExperienceManager->CallOrRegister_OnExperienceLoadFailed(
		FOnPdExperienceLoadFailed::FDelegate::CreateUObject(
			this,
			&ThisClass::HandleExperienceLoadFailed));
	ExperienceManager->SetCurrentExperienceAuth(
		ExperienceId);
}

bool ULobbyExperienceComponent::IsExperienceLoaded() const
{
	if (!GetConfiguredExperienceId().IsValid())
	{
		return true;
	}

	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const ALobbyGameState* LobbyGameState = GameMode
		? GameMode->GetGameState<ALobbyGameState>()
		: nullptr;
	const UExperienceManagerComponent* ExperienceManager =
		LobbyGameState
			? LobbyGameState
				->GetExperienceManagerComponent()
			: nullptr;
	return ExperienceManager
		&& ExperienceManager->IsExperienceLoaded();
}

bool ULobbyExperienceComponent::ShouldDelayPlayerStart() const
{
	return bWaitingForExperience
		&& !IsExperienceLoaded();
}

UClass*
ULobbyExperienceComponent::ResolveExperiencePawnClass() const
{
	if (!GetConfiguredExperienceId().IsValid()
		|| !IsExperienceLoaded())
	{
		return nullptr;
	}

	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const ALobbyGameState* LobbyGameState = GameMode
		? GameMode->GetGameState<ALobbyGameState>()
		: nullptr;
	const UExperienceManagerComponent* ExperienceManager =
		LobbyGameState
			? LobbyGameState
				->GetExperienceManagerComponent()
			: nullptr;
	const UExperienceDefinition* Experience =
		ExperienceManager
			? ExperienceManager
				->GetCurrentExperienceChecked()
			: nullptr;
	return Experience && Experience->DefaultPawnClass
		? Experience->DefaultPawnClass.Get()
		: nullptr;
}

FPrimaryAssetId
ULobbyExperienceComponent::GetConfiguredExperienceId() const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode)
	{
		return FPrimaryAssetId();
	}

	if (const UWorld* World = GameMode->GetWorld())
	{
		if (const APdWorldSettings* WorldSettings =
			Cast<APdWorldSettings>(
				World->GetWorldSettings()))
		{
			if (WorldSettings
				->GetDefaultExperienceId().IsValid())
			{
				return WorldSettings
					->GetDefaultExperienceId();
			}
		}
	}

	const ULobbyModeDefinition* Definition =
		GameMode->GetLobbyModeDefinition();
	return Definition
		? Definition->GetContentSettings()
			.DefaultExperienceId
		: FPrimaryAssetId();
}

ALobbyGameMode*
ULobbyExperienceComponent::GetLobbyGameMode() const
{
	return Cast<ALobbyGameMode>(GetOwner());
}

void ULobbyExperienceComponent::HandleExperienceLoaded(
	const UExperienceDefinition* Experience)
{
	static_cast<void>(Experience);
	bWaitingForExperience = false;
	ResumeWaitingPlayers();
}

void ULobbyExperienceComponent::HandleExperienceLoadFailed(
	const FPrimaryAssetId ExperienceId,
	const FString& FailureMessage)
{
	bWaitingForExperience = false;
	UE_LOG(
		LogLobbyExperience,
		Error,
		TEXT("Lobby experience load failed: id=%s reason=%s"),
		*ExperienceId.ToString(),
		*FailureMessage);
	ResumeWaitingPlayers();
}

void ULobbyExperienceComponent::ResumeWaitingPlayers()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	UWorld* World = GameMode
		? GameMode->GetWorld()
		: nullptr;
	if (!GameMode || !World)
	{
		return;
	}

	for (FConstPlayerControllerIterator Iterator =
		World->GetPlayerControllerIterator();
		Iterator;
		++Iterator)
	{
		APlayerController* PlayerController =
			Iterator->Get();
		if (PlayerController
			&& !PlayerController->GetPawn()
			&& GameMode->PlayerCanRestart(
				PlayerController))
		{
			GameMode->RestartPlayer(PlayerController);
		}

		if (ULobbyPreviewGrantService* PreviewGrantService =
			GameMode->GetPreviewGrantService())
		{
			PreviewGrantService->ScheduleGrant(
				PlayerController);
		}
	}

	GameMode->RefreshLobbyUIForAllPlayers();
}
