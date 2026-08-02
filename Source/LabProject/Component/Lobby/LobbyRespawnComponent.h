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
 * Lobby-only delayed respawn runtime.
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

	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;

	void RequestLobbyPlayerRespawn(
		AController* PlayerController,
		APawn* DeadPawn);
	void HandlePlayerLogout(AController* ExitingController);
	void Shutdown();

	int32 GetPendingRespawnCount() const
	{
		return PendingLobbyRespawnTimers.Num();
	}

private:
	ALobbyGameMode* GetLobbyGameMode() const;
	void FinishLobbyPlayerRespawn(
		TWeakObjectPtr<AController> WeakPlayerController,
		TWeakObjectPtr<APawn> WeakDeadPawn);
	bool TryGetLobbyPlayerRespawnTransform(
		AController* PlayerController,
		FTransform& OutRespawnTransform);
	void ResetLobbyPlayerStateForRespawn(
		AController* PlayerController) const;
	float GetLobbyRespawnDelay() const;

	TMap<TObjectKey<AController>, FTimerHandle>
		PendingLobbyRespawnTimers;
};
