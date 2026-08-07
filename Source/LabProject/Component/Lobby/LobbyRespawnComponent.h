#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "TimerManager.h"
#include "UObject/ObjectKey.h"
#include "LobbyRespawnComponent.generated.h"

class AController;
class ALobbyGameMode;
class APawn;

/**
 * Lobby-only respawn runtime.
 *
 * This intentionally stays separate from UExperienceSpawnComponent because
 * lobby respawn reuses preview pawns and has no gameplay random-spawn policy.
 */
UCLASS(ClassGroup = (Lobby))
class LABPROJECT_API ULobbyRespawnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULobbyRespawnComponent();

	void RequestLobbyPlayerRespawn(
		AController* PlayerController,
		APawn* DeadPawn);

private:
	ALobbyGameMode* GetLobbyGameMode() const;
	float GetLobbyPlayerRespawnDelay() const;
	void FinishLobbyPlayerRespawn(
		TWeakObjectPtr<AController> WeakPlayerController,
		TWeakObjectPtr<APawn> WeakDeadPawn);
	bool TryGetLobbyPlayerRespawnTransform(
		AController* PlayerController,
		FTransform& OutRespawnTransform);
	void ResetLobbyPlayerStateForRespawn(
		AController* PlayerController) const;

	TMap<TObjectKey<AController>, FTimerHandle>
		PendingLobbyPlayerRespawnTimers;
};
