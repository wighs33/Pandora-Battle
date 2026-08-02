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

public:
	AServerOnlyMonsterSpawner();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Monster spawned by this spawner. BP_Bug is the project default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster Spawner", meta = (DisplayName = "Bug Class"))
	TSubclassOf<AMonsterCharacter> MonsterClass;

	/** Delay before replacing a destroyed monster. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster Spawner", meta = (ClampMin = "0.0", Units = "s"))
	float RespawnCooldown = 5.0f;

	/** Value forwarded to the BP_Bug expose-on-spawn leash setting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster Spawner", meta = (ClampMin = "0.0", Units = "cm"))
	double MaxLeashDistanceFromSpawnPoint = 3000.0;

	/** Value forwarded to the BP_Bug expose-on-spawn roaming setting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster Spawner", meta = (ClampMin = "0.0", Units = "cm"))
	double MinLeashDistanceFromSpawnPointToResumeRoaming = 500.0;

private:
	void SpawnMonster();
	void ScheduleRespawn();
	void CleanupSpawnedMonster();
	bool ApplyMonsterSpawnParameters(AMonsterCharacter* Monster) const;

	UFUNCTION()
	void HandleSpawnedMonsterDestroyed(AActor* DestroyedActor);

	UPROPERTY(Transient)
	TObjectPtr<AMonsterCharacter> SpawnedMonster;

	FTimerHandle RespawnTimerHandle;
	bool bEndingPlay = false;
};
