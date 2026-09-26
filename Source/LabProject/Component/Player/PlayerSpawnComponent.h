#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "UObject/ObjectKey.h"
#include "PlayerSpawnComponent.generated.h"

class APlayerStart;
class UMatchRuleDefinition;

enum class EPlayerRespawnLocation : uint8 { InitialSpawn, RandomPlayerStart };
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPlayerRespawned, APlayerController*, bool /* bCreatedPawn */);

/** 서버의 PlayerStart 배정, 최초 위치 기록과 플레이어 부활을 관리한다. 모드 정책은 호출자가 지정한다. */
UCLASS(ClassGroup = (Player))
class LABPROJECT_API UPlayerSpawnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Public API ------------------------------------------------------------------------------------------------------
	UPlayerSpawnComponent();
	void Initialize(const UMatchRuleDefinition* InMatchRules);
	AActor* ChooseConfiguredPlayerStart(AController* Player, FName SpawnIndexTagPrefix);
	void MarkPlayerStartUsed(AController* Player, AActor* PlayerStart);
	void RecordInitialSpawn(AController* PlayerController, const FTransform& InitialSpawnTransform);
	void RequestPlayerRespawn(AController* PlayerController, APawn* DeadPawn);
	bool TryGetPlayerInitialSpawnTransform(AController* PlayerController, FTransform& OutSpawnTransform) const;
	TArray<APlayerController*> MovePlayersToInitialSpawns();
	void ClearRuntimeStateForController(AController* Controller);
	void StopRespawning();
	void SetRespawnLocation(EPlayerRespawnLocation Location) { RespawnLocation = Location; }
	int32 GetPendingRespawnCount() const { return PendingPlayerRespawnTimers.Num(); }
	FOnPlayerRespawned OnPlayerRespawned;

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	AActor* FindPlayerStartBySpawnIndex(int32 SpawnIndex, FName PlayerStartTagPrefix) const;
	AActor* FindFirstUnusedPlayerStart() const;
	bool IsPlayerStartUsed(const AActor* PlayerStart) const;
	void FinishPlayerRespawn(TWeakObjectPtr<AController> WeakPlayerController, TWeakObjectPtr<APawn> WeakDeadPawn);
	bool TryGetPlayerRespawnTransform(AController* PlayerController, FTransform& OutRespawnTransform);
	AActor* FindRandomRespawnPlayerStart(AController* PlayerController, const UMatchRuleDefinition& Rules) const;
	bool DoesPlayerStartMatchRandomRespawnTags(const APlayerStart* PlayerStart, const UMatchRuleDefinition& Rules) const;

	UPROPERTY(Transient)
	TObjectPtr<const UMatchRuleDefinition> MatchRules;
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> UsedPlayerStarts;
	EPlayerRespawnLocation RespawnLocation = EPlayerRespawnLocation::InitialSpawn;
	// 초기화 전과 StopRespawning 이후에는 새 요청도 받지 않는다.
	bool bRespawningEnabled = false;
	TMap<TObjectKey<AController>, TWeakObjectPtr<AActor>> AssignedPlayerStartsByController;
	TMap<TObjectKey<AController>, FTransform> InitialPlayerSpawnTransforms;
	TMap<TObjectKey<AController>, FName> LastRandomRespawnPlayerStartNames;
	TMap<TObjectKey<AController>, FTimerHandle> PendingPlayerRespawnTimers;
};
