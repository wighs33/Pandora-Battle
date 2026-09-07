#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Definition/Experience/ExperienceGameModeSettings.h"
#include "TimerManager.h"
#include "UObject/ObjectKey.h"
#include "ExperienceSpawnComponent.generated.h"

class AExperienceGameMode;
class APlayerStart;
class UMatchRuleDefinition;

/**
 * GameMode에서 서버의 스폰 위치 배정과 리스폰을 관리한다.
 *
 * 현재 경기의 초기 스폰 위치와 예약된 리스폰을 소유한다.
 * 로비에서 전달받은 스폰 순서 등의 식별 정보는 PlayerState의 경기 컴포넌트에 남긴다.
 */
UCLASS(ClassGroup = (Experience))
class LABPROJECT_API UExperienceSpawnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UExperienceSpawnComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ApplySettings(const FExperienceSpawnSettings& InSettings)
	{
		Settings = InSettings;
	}

	AActor* ChooseConfiguredPlayerStart(AController* Player);
	void MarkPlayerStartUsed(AController* Player, AActor* PlayerStart);
	void RecordInitialSpawn(
		AController* PlayerController,
		const FTransform& InitialSpawnTransform);

	void RequestPlayerRespawn(
		AController* PlayerController,
		APawn* DeadPawn);
	bool TryGetPlayerInitialSpawnTransform(
		AController* PlayerController,
		FTransform& OutSpawnTransform) const;
	void ForceMovePlayersToInitialSpawns();
	void ClearRuntimeStateForController(AController* Controller);

	int32 GetPendingRespawnCount() const
	{
		return PendingPlayerRespawnTimers.Num();
	}

private:
	AExperienceGameMode* GetExperienceGameMode() const;
	const AExperienceGameMode* GetExperienceGameModeConst() const;

	AActor* FindPlayerStartByMatchSpawnIndex(
		int32 SpawnIndex,
		FName PlayerStartTagPrefix) const;
	AActor* FindFirstUnusedPlayerStart() const;
	bool IsPlayerStartUsed(const AActor* PlayerStart) const;

	void FinishPlayerRespawn(
		TWeakObjectPtr<AController> WeakPlayerController,
		TWeakObjectPtr<APawn> WeakDeadPawn);
	bool TryGetPlayerRespawnTransform(
		AController* PlayerController,
		FTransform& OutRespawnTransform);
	AActor* FindRandomRespawnPlayerStart(
		AController* PlayerController,
		const UMatchRuleDefinition& MatchRules) const;
	bool DoesPlayerStartMatchRandomRespawnTags(
		const APlayerStart* PlayerStart,
		const UMatchRuleDefinition& MatchRules) const;
	void ResetPlayerStateForRespawn(AController* PlayerController) const;
	float GetPlayerRespawnDelay() const;

	UPROPERTY(Transient)
	FExperienceSpawnSettings Settings;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> UsedPlayerStarts;

	TMap<TObjectKey<AController>, TWeakObjectPtr<AActor>>
		AssignedPlayerStartsByController;
	TMap<TObjectKey<AController>, FTransform> InitialPlayerSpawnTransforms;
	TMap<TObjectKey<AController>, FName>
		LastRandomRespawnPlayerStartNames;
	TMap<TObjectKey<AController>, FTimerHandle>
		PendingPlayerRespawnTimers;
};
