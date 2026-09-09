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

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDefaultPlayerProvisioned, APlayerController*);

/**
 * Applies DA_DefaultProvision to a player.
 *
 * The owner loads the definition and initializes one provisioner per play space.
 * Definition and mode remain fixed until Shutdown; repeated initialization never resets grants.
 */
UCLASS(Transient)
class LABPROJECT_API UDefaultPlayerProvisioner : public UObject
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;

	FOnDefaultPlayerProvisioned OnPlayerProvisioned;

	bool Initialize(const UDefaultProvisionDefinition* InDefinition, EDefaultProvisionMode InMode);
	bool IsInitialized() const { return ProvisionDefinition != nullptr && !bShuttingDown; }
	void ProvisionPlayer(APlayerController* PlayerController);
	void ClearRuntimeStateForController(
		AController* Controller,
		APlayerState* PlayerState);
	void Shutdown();

	bool ApplyConfiguredStatusPointsForPlayerState(
		APlayerState* PlayerState) const;

private:
	enum class EContentState : uint8 { NotStarted, Loading, Ready, Failed };
	struct FInventoryProvisionState
	{
		TWeakObjectPtr<UInventoryComponent> Inventory;
		bool bCompleted = false;
	};

	struct FInitializedPandoraState
	{
		TWeakObjectPtr<UPandoraComponent> PandoraComponent;
		TWeakObjectPtr<UPandoraTreeComponent> PandoraTreeComponent;
	};

	const UDefaultProvisionDefinition* GetDefinition() const;
	bool EnsureContentLoaded(APlayerController* PlayerController);
	void HandleContentLoaded();
	bool TryProvisionPlayer(APlayerController* PlayerController);
	bool ApplyItems(APdPlayerState* PlayerState);
	bool ApplyModeValues(APdPlayerState* PlayerState);
	bool ApplyPandoras(APdPlayerState* PlayerState);
	bool ApplyGestures(
		APlayerController* PlayerController,
		APdPlayerState* PlayerState);
	void ScheduleRetry(APlayerController* PlayerController);
	void ClearRetryTimer(APlayerController* PlayerController);
	int32 GetInventoryItemQuantity(
		const UInventoryComponent* InventoryComponent,
		FPrimaryAssetId ItemDefinitionId) const;

	UPROPERTY(Transient)
	TObjectPtr<UDefaultProvisionDefinition> ProvisionDefinition;

	TMap<TObjectKey<APlayerController>, FTimerHandle> PendingRetryTimers;
	TArray<TWeakObjectPtr<APlayerController>> PendingContentControllers;
	TArray<FPrimaryAssetId> RequiredContentIds;
	TSharedPtr<FStreamableHandle> ContentLoadHandle;
	EContentState ContentState = EContentState::NotStarted;
	EDefaultProvisionMode Mode = EDefaultProvisionMode::Lobby;
	TMap<TObjectKey<APlayerState>, FInventoryProvisionState> InventoryStates;
	TSet<TObjectKey<APlayerState>> InitializedModeValues;
	TMap<TObjectKey<APlayerState>, FInitializedPandoraState>
		InitializedPandoras;
	TMap<TObjectKey<APlayerState>, TWeakObjectPtr<USkinEquipmentComponent>>
		InitializedGestureEquipment;
	bool bShuttingDown = false;
};
