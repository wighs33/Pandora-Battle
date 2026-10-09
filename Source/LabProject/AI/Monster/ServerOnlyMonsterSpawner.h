#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ServerOnlyMonsterSpawner.generated.h"

class AMonsterCharacter;

/**
 * 레벨에 배치된 몬스터 스포너의 서버 실행을 담당한다.
 *
 * 블루프린트 자식 클래스는 기본값만 제공한다. 생성·리스폰·델리게이트 정리와
 * 월드 파티션 언로드 시 정리는 이 클래스에서 처리한다.
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
	/** 스포너마다 따로 정할 몬스터 클래스. 비어 있으면 DA_EnemyBase의 기본 몬스터 클래스를 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster Spawner", meta = (DisplayName = "Monster Class"))
	TSubclassOf<AMonsterCharacter> MonsterClass;

	/** 몬스터가 파괴된 뒤 새로 채우기까지의 지연. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster Spawner", meta = (ClampMin = "0.0", Units = "s"))
	float RespawnCooldown = 5.0f;

	/** 스폰한 몬스터의 expose-on-spawn 추적 범위(leash) 설정에 넘기는 값. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster Spawner", meta = (ClampMin = "0.0", Units = "cm"))
	double MaxLeashDistanceFromSpawnPoint = 3000.0;

	/** 스폰한 몬스터의 expose-on-spawn 배회 설정에 넘기는 값. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster Spawner", meta = (ClampMin = "0.0", Units = "cm"))
	double MinLeashDistanceFromSpawnPointToResumeRoaming = 500.0;

private:
	UPROPERTY(Transient)
	TObjectPtr<AMonsterCharacter> SpawnedMonster;

	FTimerHandle RespawnTimerHandle;
	bool bEndingPlay = false;
};
