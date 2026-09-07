#include "Component/Lobby/LobbyExperienceComponent.h"

#include "Component/Experience/ExperienceManagerComponent.h"
#include "Definition/Experience/ExperienceDefinition.h"
#include "Experience/PdWorldSettings.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Lobby/Contents/LobbyGameState.h"

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
		return;
	}

	ALobbyGameState* LobbyGameState =
		GameMode->GetGameState<ALobbyGameState>();
	if (!LobbyGameState)
	{
		HandleExperienceLoadFailed(ExperienceId, TEXT("LobbyGameState is missing."));
		return;
	}

	UExperienceManagerComponent* ExperienceManager =
		LobbyGameState->GetExperienceManagerComponent();
	if (!ExperienceManager)
	{
		HandleExperienceLoadFailed(ExperienceId, TEXT("ExperienceManagerComponent is missing."));
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
	return !IsExperienceLoaded();
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

	return FPrimaryAssetId();
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
	if (ALobbyGameMode* GameMode = GetLobbyGameMode())
	{
		if (ALobbyGameState* GameState = GameMode->GetGameState<ALobbyGameState>())
		{
			GameState->SetExperienceLoadFailed(false);
		}
	}
	ResumeWaitingPlayers();
}

// 필수 Experience 실패는 기본 Pawn 시작으로 우회하지 않고 서버와 클라이언트에 실패 상태를 남긴다.
void ULobbyExperienceComponent::HandleExperienceLoadFailed(
	const FPrimaryAssetId ExperienceId,
	const FString& FailureMessage)
{
	UE_LOG(
		LogLobbyExperience,
		Error,
		TEXT("Lobby experience load failed: id=%s reason=%s"),
		*ExperienceId.ToString(),
		*FailureMessage);
	if (ALobbyGameMode* GameMode = GetLobbyGameMode())
	{
		if (ALobbyGameState* GameState = GameMode->GetGameState<ALobbyGameState>())
		{
			GameState->SetExperienceLoadFailed(true);
		}
	}
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
			GameMode->HandleStartingNewPlayer(PlayerController);
		}
	}
}
