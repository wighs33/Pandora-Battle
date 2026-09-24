#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ServerOnlyMonsterSpawner.generated.h"

class AMonsterCharacter;

/**
 * Server-owned runtime implementation for level-authored monster spawners.
 *
 * Blueprint children only provide defaults. Spawning, respawning, delegate
 * cleanup, and World Partition unload cleanup are all handled here.
 */
UCLASS(Abstract, Blueprintable)
class LABPROJECT_API AServerOnlyMonsterSpawner : public AActor
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	AServerOnlyMonsterSpawner();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void SpawnMonster();

	UFUNCTION()
	void HandleSpawnedMonsterDestroyed(AActor* DestroyedActor);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ScheduleRespawn();
	void CleanupSpawnedMonster();
	bool ApplyMonsterSpawnParameters(AMonsterCharacter* Monster) const;

protected:
	/** Optional per-spawner override. When empty, DA_EnemyBase supplies the default monster class. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster Spawner", meta = (DisplayName = "Monster Class"))
	TSubclassOf<AMonsterCharacter> MonsterClass;

	/** Delay before replacing a destroyed monster. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster Spawner", meta = (ClampMin = "0.0", Units = "s"))
	float RespawnCooldown = 5.0f;

	/** Value forwarded to the spawned monster's expose-on-spawn leash setting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster Spawner", meta = (ClampMin = "0.0", Units = "cm"))
	double MaxLeashDistanceFromSpawnPoint = 3000.0;

	/** Value forwarded to the spawned monster's expose-on-spawn roaming setting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster Spawner", meta = (ClampMin = "0.0", Units = "cm"))
	double MinLeashDistanceFromSpawnPointToResumeRoaming = 500.0;

private:
	UPROPERTY(Transient)
	TObjectPtr<AMonsterCharacter> SpawnedMonster;

	FTimerHandle RespawnTimerHandle;
	bool bEndingPlay = false;
};
