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
 * Server-only player-start and respawn runtime owned by AExperienceGameMode.
 *
 * Match identity stays on PlayerState's match component. This component only
 * owns world-lifetime assignment caches and pending respawn timers.
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

	int32 ResolveMatchSpawnIndex(AController* Player) const;
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
