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
 * 로비 전용 리스폰 처리를 담당한다.
 *
 * 로비는 미리보기 Pawn을 재사용하고 게임의 무작위 스폰 정책을 사용하지 않으므로
 * UExperienceSpawnComponent와 분리한다.
 */
UCLASS(ClassGroup = (Lobby))
class LABPROJECT_API ULobbyRespawnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	ULobbyRespawnComponent();

	void RequestLobbyPlayerRespawn(
		AController* PlayerController,
		APawn* DeadPawn);

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
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

private:
	TMap<TObjectKey<AController>, FTimerHandle>
		PendingLobbyPlayerRespawnTimers;
};
