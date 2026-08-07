#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "LobbyPlayerCoordinatorComponent.generated.h"

class AController;
class ALobbyGameMode;
class ALobbyPlayerState;
class APlayerController;
class APlayerState;

/**
 * Lobby roster identity, slot assignment, UI fan-out, and kick lifecycle.
 */
UCLASS(ClassGroup = (Lobby))
class LABPROJECT_API ULobbyPlayerCoordinatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULobbyPlayerCoordinatorComponent();

	void InitializeLobbyPlayerState(
		APlayerController* PlayerController,
		ALobbyPlayerState* LobbyPlayerState);
	void HandlePlayerLogout(AController* ExitingController);
	void KickPlayer(ALobbyPlayerState* TargetPlayerState);
	void ResetLobbyReadyStates() const;
	void RefreshLobbyUIForAllPlayers() const;
	APlayerController* ResolvePlayerControllerForPlayerState(
		const APlayerState* PlayerState) const;

private:
	ALobbyGameMode* GetLobbyGameMode() const;
	void AssignLobbySpawnIndexIfNeeded(
		ALobbyPlayerState* LobbyPlayerState) const;
	int32 FindAvailableLobbySpawnIndex(
		const ALobbyPlayerState* IgnoredPlayerState) const;
	void ForceKickPlayer(
		APlayerController* TargetPlayerController);

	int32 NicknameIndex = 0;
};
