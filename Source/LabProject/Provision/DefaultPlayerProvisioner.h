#pragma once

#include "CoreMinimal.h"
#include "Definition/Provision/DefaultProvisionDefinition.h"
#include "TimerManager.h"
#include "UObject/Object.h"
#include "UObject/ObjectKey.h"
#include "DefaultPlayerProvisioner.generated.h"

class AController;
class APlayerController;
class APlayerState;
class APdPlayerState;
class UInventoryComponent;
class UPandoraComponent;
class UPandoraTreeComponent;
class USkinEquipmentComponent;
struct FStreamableHandle;

/**
 * Applies DA_DefaultProvision to a player.
 *
 * Lobby, training-room, and gameplay differences are data selected by Mode;
 * they do not require separate provisioner implementations.
 */
UCLASS(Transient)
class LABPROJECT_API UDefaultPlayerProvisioner : public UObject
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;

	void SetDefinition(const UDefaultProvisionDefinition* InDefinition);
	void ProvisionPlayer(
		APlayerController* PlayerController,
		EDefaultProvisionMode Mode);
	void ClearRuntimeStateForController(
		AController* Controller,
		APlayerState* PlayerState);
	void Shutdown();

	bool ApplyConfiguredStatusPointsForPlayerState(
		APlayerState* PlayerState,
		EDefaultProvisionMode Mode) const;

private:
	struct FInitializedPandoraState
	{
		EDefaultProvisionMode Mode = EDefaultProvisionMode::Lobby;
		TWeakObjectPtr<UPandoraComponent> PandoraComponent;
		TWeakObjectPtr<UPandoraTreeComponent> PandoraTreeComponent;
	};

	const UDefaultProvisionDefinition* GetDefinition() const;
	bool EnsureContentLoaded(
		APlayerController* PlayerController,
		EDefaultProvisionMode Mode);
	bool TryProvisionPlayer(
		APlayerController* PlayerController,
		EDefaultProvisionMode Mode);
	bool ApplyItems(
		APdPlayerState* PlayerState,
		EDefaultProvisionMode Mode);
	bool ApplyModeValues(
		APdPlayerState* PlayerState,
		EDefaultProvisionMode Mode);
	bool ApplyPandoras(
		APdPlayerState* PlayerState,
		EDefaultProvisionMode Mode);
	bool ApplyGestures(
		APlayerController* PlayerController,
		APdPlayerState* PlayerState);
	void ScheduleRetry(
		APlayerController* PlayerController,
		EDefaultProvisionMode Mode);
	void ClearRetryTimer(APlayerController* PlayerController);
	void CleanupLoadHandles();
	int32 GetInventoryItemQuantity(
		const UInventoryComponent* InventoryComponent,
		FPrimaryAssetId ItemDefinitionId) const;

	UPROPERTY(Transient)
	TObjectPtr<UDefaultProvisionDefinition> ProvisionDefinition;

	TMap<TObjectKey<APlayerController>, FTimerHandle> PendingRetryTimers;
	TMap<TObjectKey<APlayerController>, EDefaultProvisionMode>
		PendingRetryModes;
	TSet<TObjectKey<AController>> PendingContentControllers;
	TMap<TObjectKey<AController>, EDefaultProvisionMode>
		AttemptedContentLoadModes;
	TArray<TSharedPtr<FStreamableHandle>> PendingLoadHandles;
	TSet<TObjectKey<APlayerState>> PendingItemPlayerStates;
	TMap<TObjectKey<APlayerState>, TWeakObjectPtr<UInventoryComponent>>
		InitializedInventories;
	TMap<TObjectKey<APlayerState>, EDefaultProvisionMode>
		InitializedInventoryModes;
	TSet<TObjectKey<APlayerState>> CompletedItemPlayerStates;
	TMap<TObjectKey<APlayerState>, EDefaultProvisionMode>
		InitializedModeValues;
	TMap<TObjectKey<APlayerState>, FInitializedPandoraState>
		InitializedPandoras;
	TMap<TObjectKey<APlayerState>, TWeakObjectPtr<USkinEquipmentComponent>>
		InitializedGestureEquipment;
	bool bLoggedMissingProvisionDefinition = false;
	bool bShuttingDown = false;
};
