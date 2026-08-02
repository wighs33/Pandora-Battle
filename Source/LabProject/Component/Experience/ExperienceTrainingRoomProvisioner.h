#pragma once

#include "CoreMinimal.h"
#include "Definition/Experience/ExperienceGameModeSettings.h"
#include "TimerManager.h"
#include "UObject/Object.h"
#include "UObject/ObjectKey.h"
#include "ExperienceTrainingRoomProvisioner.generated.h"

class AController;
class AExperienceGameMode;
class APlayerController;
class APlayerState;
class UInventoryComponent;

/**
 * Owns training-room inventory, stat, and currency initialization policy.
 *
 * Its idempotency sets are kept separate from normal gameplay loadout state,
 * which makes retries and logout cleanup local to the training-room domain.
 */
UCLASS()
class LABPROJECT_API UExperienceTrainingRoomProvisioner : public UObject
{
	GENERATED_BODY()

public:
	void ApplySettings(
		const FExperiencePlayerProvisioningSettings& InSettings);
	void Shutdown();
	void ClearRuntimeStateForController(
		AController* Controller,
		APlayerState* PlayerState);

	bool IsTrainingRoomMap() const;
	void PreparePlayerForGameplay(APlayerController* NewPlayer);
	void GrantTrainingRoomStatusPointsForPlayerState(
		APlayerState* PlayerState);

private:
	AExperienceGameMode* GetExperienceGameMode() const;
	int32 GetInventoryItemQuantityByPrimaryAssetId(
		const UInventoryComponent* InventoryComponent,
		FPrimaryAssetId ItemDefinitionId) const;
	void GrantAllItemsForTrainingRoom(
		APlayerController* NewPlayer,
		int32 RemainingAttempts = INDEX_NONE);
	void ScheduleTrainingRoomItems(
		APlayerController* NewPlayer,
		int32 RemainingAttempts);
	void InitializeTrainingRoomStatusPoints(
		APlayerController* NewPlayer);
	void ScheduleTrainingRoomStatusPoints(
		APlayerController* NewPlayer,
		int32 RemainingAttempts = 5);
	void InitializeTrainingRoomSoulDust(APlayerState* PlayerState);

	bool bGrantAllItemsInTrainingRoom = true;
	TArray<FTrainingRoomItemStackGrant> TrainingRoomItemStackGrants;
	TArray<FGameplayItemStackGrant> DefaultGameplayItemStackGrants;
	int32 TrainingRoomItemGrantMaxAttempts = 50;
	float TrainingRoomItemGrantRetryDelay = 0.1f;
	bool bInitializeStatusPointsInTrainingRoom = true;
	float TrainingRoomStatusPointValue = 100.0f;
	bool bInitializeSoulDustInTrainingRoom = true;
	int32 TrainingRoomSoulDustValue = 100000;
	TArray<FName> TrainingRoomMapNames;

	UPROPERTY(Transient)
	TSet<TObjectPtr<APlayerState>>
		TrainingRoomItemGrantPendingPlayerStates;

	TMap<TObjectKey<AController>, FTimerHandle>
		PendingTrainingRoomItemGrantTimers;

	UPROPERTY(Transient)
	TSet<TObjectPtr<APlayerState>>
		TrainingRoomStatusInitializedPlayerStates;

	UPROPERTY(Transient)
	TSet<TObjectPtr<APlayerState>>
		TrainingRoomSoulDustInitializedPlayerStates;

	bool bShuttingDown = false;
};
