#pragma once

#include "CoreMinimal.h"
#include "Definition/Experience/ExperienceGameModeSettings.h"
#include "TimerManager.h"
#include "UObject/Object.h"
#include "UObject/ObjectKey.h"
#include "ExperienceGameplayLoadoutProvisioner.generated.h"

class AController;
class AExperienceGameMode;
class APlayerController;
class APlayerState;
class UInventoryComponent;
struct FStreamableHandle;

/**
 * Owns normal-gameplay starter loadout policy and its retry state.
 *
 * Inventory replacement during seamless travel is handled here so the
 * coordinator does not need to know about concrete inventory lifetimes.
 */
UCLASS()
class LABPROJECT_API UExperienceGameplayLoadoutProvisioner : public UObject
{
	GENERATED_BODY()

public:
	void ApplySettings(
		const FExperiencePlayerProvisioningSettings& InSettings);
	void Shutdown();
	void ClearRuntimeStateForController(
		AController* Controller,
		APlayerState* PlayerState);

	void PrepareGameplayLoadout(
		APlayerController* NewPlayer,
		bool bIsTrainingRoom);
	void GrantDefaultGameplayGestures(APlayerController* NewPlayer);

	int32 GetPendingDefaultItemGrantCount() const
	{
		return PendingDefaultGameplayItemGrantTimers.Num();
	}

private:
	AExperienceGameMode* GetExperienceGameMode() const;
	void ClearLobbyPreviewContentForGameplay(
		APlayerController* NewPlayer,
		bool bIsTrainingRoom);
	void GrantDefaultGameplayPandoras(APlayerController* NewPlayer);
	bool GrantDefaultGameplayItems(
		APlayerController* NewPlayer,
		bool bIsTrainingRoom);
	void ScheduleDefaultGameplayItems(
		APlayerController* NewPlayer,
		bool bIsTrainingRoom,
		int32 RemainingAttempts = INDEX_NONE);
	void ApplyLoadedDefaultGameplayGestures(APlayerController* NewPlayer);
	void CleanupGestureLoadHandles();
	void CancelGestureLoads();

	TArray<FGameplayItemStackGrant> DefaultGameplayItemStackGrants;
	int32 DefaultGameplayItemGrantMaxAttempts = 50;
	float DefaultGameplayItemGrantRetryDelay = 0.1f;
	TArray<FGameplayGestureSlotGrant> DefaultGameplayGestureSlotGrants;

	TMap<TObjectKey<AController>, FTimerHandle>
		PendingDefaultGameplayItemGrantTimers;
	TMap<TObjectKey<APlayerState>, TWeakObjectPtr<UInventoryComponent>>
		DefaultGameplayInitializedInventoriesByPlayerState;
	TSet<TObjectKey<AController>> PendingDefaultGameplayGestureControllers;
	TArray<TSharedPtr<FStreamableHandle>> PendingGestureLoadHandles;
	bool bShuttingDown = false;
};
