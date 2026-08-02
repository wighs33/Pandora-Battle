#include "Component/Lobby/LobbyPlayerCoordinatorComponent.h"

#include "Component/Player/PlayerMatchComponent.h"
#include "Definition/Lobby/LobbyModeDefinition.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/GameStateBase.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Lobby/Contents/LobbyHUD.h"
#include "Lobby/Contents/LobbyPlayerController.h"
#include "Lobby/Contents/LobbyPlayerState.h"
#include "Lobby/Coordination/LobbyMatchCoordinator.h"
#include "Mode/PdGameInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyPlayerCoordinatorComponent)

ULobbyPlayerCoordinatorComponent::
ULobbyPlayerCoordinatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void ULobbyPlayerCoordinatorComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	Shutdown();
	Super::EndPlay(EndPlayReason);
}

void ULobbyPlayerCoordinatorComponent::
InitializeLobbyPlayerState(
	APlayerController* PlayerController,
	ALobbyPlayerState* LobbyPlayerState)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !LobbyPlayerState)
	{
		return;
	}

	if (LobbyPlayerState->GetNickname().IsEmpty())
	{
		++NicknameIndex;
		const UPdGameInstance* GameInstance =
			GameMode->GetGameInstance<UPdGameInstance>();
		const FText DefaultNickname = GameInstance
			? GameInstance->ResolveDefaultPlayerNickname(
				PlayerController,
				LobbyPlayerState,
				NicknameIndex)
			: FText::Format(
				NSLOCTEXT(
					"Lobby",
					"DefaultNicknameFormat",
					"User{0}"),
				NicknameIndex);
		LobbyPlayerState->SetDefaultNickname(
			DefaultNickname);
	}

	LobbyPlayerState
		->InitializeLobbyPreviewAbilitySystem(
			GameMode->GetLobbyPreviewDefinition());
	AssignLobbySpawnIndexIfNeeded(LobbyPlayerState);
	if (ULobbyMatchCoordinator* MatchCoordinator =
		GameMode->GetMatchCoordinator())
	{
		MatchCoordinator
			->AssignLobbyTeamColorIfNeeded(
				LobbyPlayerState);
	}
}

void ULobbyPlayerCoordinatorComponent::HandlePlayerLogout(
	AController* ExitingController)
{
	ClearKickTimer(ExitingController);

	ALobbyGameMode* GameMode = GetLobbyGameMode();
	UWorld* World = GameMode
		? GameMode->GetWorld()
		: nullptr;
	if (!GameMode || !World)
	{
		return;
	}

	FTimerDelegate RefreshDelegate;
	RefreshDelegate.BindWeakLambda(this, [this]()
	{
		RefreshLobbyUIForAllPlayers();
		if (ALobbyGameMode* CurrentGameMode =
			GetLobbyGameMode())
		{
			if (ULobbyMatchCoordinator* MatchCoordinator =
				CurrentGameMode->GetMatchCoordinator())
			{
				MatchCoordinator
					->UpdateFullLobbyAutoStartTimer();
			}
		}
	});
	World->GetTimerManager().SetTimerForNextTick(
		RefreshDelegate);
}

void ULobbyPlayerCoordinatorComponent::KickPlayer(
	ALobbyPlayerState* TargetPlayerState)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode
		|| !GameMode->HasAuthority()
		|| !TargetPlayerState)
	{
		return;
	}

	APlayerController* TargetPlayerController =
		ResolvePlayerControllerForPlayerState(
			TargetPlayerState);
	const FString RoomMapName =
		GameMode->GetRoomTravelMapName();
	if (!TargetPlayerController || RoomMapName.IsEmpty())
	{
		return;
	}

	ULobbyMatchCoordinator* MatchCoordinator =
		GameMode->GetMatchCoordinator();
	if (MatchCoordinator
		&& MatchCoordinator->IsGameStartRequested())
	{
		MatchCoordinator->CancelPendingGameStart(
			TEXT("player_kicked"));
	}
	else if (MatchCoordinator)
	{
		MatchCoordinator->ClearStartTimers();
	}

	TargetPlayerState->SetLeavingLobby(true);
	ResetLobbyReadyStates();
	RefreshLobbyUIForAllPlayers();
	if (MatchCoordinator)
	{
		MatchCoordinator
			->UpdateFullLobbyAutoStartTimer();
	}

	ALobbyPlayerController* TargetLobbyPlayerController =
		Cast<ALobbyPlayerController>(
			TargetPlayerController);
	if (!TargetLobbyPlayerController)
	{
		return;
	}

	TargetLobbyPlayerController->Client_KickedByHost(
		RoomMapName);

	const ULobbyModeDefinition* Definition =
		GameMode->GetLobbyModeDefinition();
	const float KickDisconnectDelay = Definition
		? FMath::Max(
			Definition->GetFlowSettings()
				.KickDisconnectDelay,
			0.0f)
		: 0.0f;
	if (KickDisconnectDelay <= 0.0f)
	{
		ForceKickPlayer(TargetPlayerController);
		return;
	}

	ClearKickTimer(TargetPlayerController);
	const TObjectKey<AController> ControllerKey(
		TargetPlayerController);
	TWeakObjectPtr<APlayerController>
		WeakTargetPlayerController(
			TargetPlayerController);
	FTimerDelegate KickDelegate;
	KickDelegate.BindWeakLambda(
		this,
		[this, ControllerKey, WeakTargetPlayerController]()
		{
			PendingKickTimers.Remove(ControllerKey);
			if (APlayerController* PlayerController =
				WeakTargetPlayerController.Get())
			{
				ForceKickPlayer(PlayerController);
			}
		});

	FTimerHandle& KickTimerHandle =
		PendingKickTimers.FindOrAdd(ControllerKey);
	GameMode->GetWorldTimerManager().SetTimer(
		KickTimerHandle,
		KickDelegate,
		KickDisconnectDelay,
		false);
}

void ULobbyPlayerCoordinatorComponent::
ResetLobbyReadyStates() const
{
	const ALobbyGameMode* GameMode =
		GetLobbyGameMode();
	if (!GameMode || !GameMode->GameState)
	{
		return;
	}

	for (APlayerState* PlayerState :
		GameMode->GameState->PlayerArray)
	{
		if (ALobbyPlayerState* LobbyPlayerState =
			Cast<ALobbyPlayerState>(PlayerState))
		{
			LobbyPlayerState->SetReady(false);
		}
	}
}

void ULobbyPlayerCoordinatorComponent::
RefreshLobbyUIForAllPlayers() const
{
	const ALobbyGameMode* GameMode =
		GetLobbyGameMode();
	UWorld* World = GameMode
		? GameMode->GetWorld()
		: nullptr;
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator Iterator =
		World->GetPlayerControllerIterator();
		Iterator;
		++Iterator)
	{
		ALobbyPlayerController* LobbyPlayerController =
			Cast<ALobbyPlayerController>(
				Iterator->Get());
		if (!LobbyPlayerController)
		{
			continue;
		}

		if (LobbyPlayerController->IsLocalController())
		{
			if (ALobbyHUD* LobbyHUD =
				LobbyPlayerController
					->GetHUD<ALobbyHUD>())
			{
				LobbyHUD->RefreshLobbyUI();
			}
		}
		else
		{
			LobbyPlayerController
				->Client_RefreshLobbyUI();
		}
	}
}

APlayerController* ULobbyPlayerCoordinatorComponent::
ResolvePlayerControllerForPlayerState(
	const APlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return nullptr;
	}

	if (APlayerController* PlayerController =
		Cast<APlayerController>(
			PlayerState->GetOwner()))
	{
		return PlayerController;
	}

	const ALobbyGameMode* GameMode =
		GetLobbyGameMode();
	const UWorld* World = GameMode
		? GameMode->GetWorld()
		: nullptr;
	if (!World)
	{
		return nullptr;
	}

	for (FConstPlayerControllerIterator Iterator =
		World->GetPlayerControllerIterator();
		Iterator;
		++Iterator)
	{
		APlayerController* Candidate =
			Iterator->Get();
		if (Candidate
			&& Candidate->PlayerState == PlayerState)
		{
			return Candidate;
		}
	}

	return nullptr;
}

void ULobbyPlayerCoordinatorComponent::Shutdown()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (GameMode)
	{
		for (TPair<TObjectKey<AController>, FTimerHandle>&
			KickTimer : PendingKickTimers)
		{
			GameMode->GetWorldTimerManager().ClearTimer(
				KickTimer.Value);
		}
	}

	PendingKickTimers.Reset();
}

ALobbyGameMode*
ULobbyPlayerCoordinatorComponent::GetLobbyGameMode() const
{
	return Cast<ALobbyGameMode>(GetOwner());
}

void ULobbyPlayerCoordinatorComponent::
AssignLobbySpawnIndexIfNeeded(
	ALobbyPlayerState* LobbyPlayerState) const
{
	if (!LobbyPlayerState
		|| LobbyPlayerState->GetPlayerMatchComponent()
			->GetMatchSpawnIndex() != INDEX_NONE)
	{
		return;
	}

	LobbyPlayerState->GetPlayerMatchComponent()
		->SetMatchSpawnIndex(
			FindAvailableLobbySpawnIndex(
				LobbyPlayerState));
}

int32 ULobbyPlayerCoordinatorComponent::
FindAvailableLobbySpawnIndex(
	const ALobbyPlayerState* IgnoredPlayerState) const
{
	const ALobbyGameMode* GameMode =
		GetLobbyGameMode();
	TSet<int32> UsedSpawnIndices;
	if (GameMode && GameMode->GameState)
	{
		for (APlayerState* PlayerState :
			GameMode->GameState->PlayerArray)
		{
			const ALobbyPlayerState* LobbyPlayerState =
				Cast<ALobbyPlayerState>(PlayerState);
			if (!LobbyPlayerState
				|| LobbyPlayerState
					== IgnoredPlayerState)
			{
				continue;
			}

			const int32 SpawnIndex =
				LobbyPlayerState
					->GetPlayerMatchComponent()
					->GetMatchSpawnIndex();
			if (SpawnIndex != INDEX_NONE)
			{
				UsedSpawnIndices.Add(SpawnIndex);
			}
		}
	}

	const int32 SearchLimit = FMath::Max(
		GameMode
			? GameMode
				->GetSelectedLobbyMaxPlayerCount()
			: 1,
		1);
	for (int32 SpawnIndex = 0;
		SpawnIndex < SearchLimit;
		++SpawnIndex)
	{
		if (!UsedSpawnIndices.Contains(SpawnIndex))
		{
			return SpawnIndex;
		}
	}

	return UsedSpawnIndices.Num();
}

void ULobbyPlayerCoordinatorComponent::ForceKickPlayer(
	APlayerController* TargetPlayerController)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode
		|| !GameMode->HasAuthority()
		|| !TargetPlayerController
		|| !GameMode->GameSession)
	{
		return;
	}

	GameMode->GameSession->KickPlayer(
		TargetPlayerController,
		NSLOCTEXT(
			"Lobby",
			"KickedByHost",
			"Kicked by host"));
}

void ULobbyPlayerCoordinatorComponent::ClearKickTimer(
	AController* Controller)
{
	if (!Controller)
	{
		return;
	}

	const TObjectKey<AController> ControllerKey(
		Controller);
	if (FTimerHandle* TimerHandle =
		PendingKickTimers.Find(ControllerKey))
	{
		if (ALobbyGameMode* GameMode =
			GetLobbyGameMode())
		{
			GameMode->GetWorldTimerManager().ClearTimer(
				*TimerHandle);
		}
		PendingKickTimers.Remove(ControllerKey);
	}
}
