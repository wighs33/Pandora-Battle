#pragma once

#include "Common/Enum_Direction.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "InfoLoadoutStore.generated.h"

class APdPlayerController;
class UEquipmentComponent;
class UInventoryComponent;
class UItemInstance;
class UPandoraComponent;
class UPandoraDefinition;
class UPandoraInstance;

/** A single state transition emitted after the store has refreshed its cached read model. */
enum class EInfoLoadoutStateChange : uint8
{
	Bindings,
	Inventory,
	WeaponLoadout,
	PandoraLoadout,
	PresentationAssets
};

DECLARE_MULTICAST_DELEGATE_OneParam(FInfoLoadoutStateChanged, EInfoLoadoutStateChange);

/**
 * Shared read model and command boundary for the Info screen's item/Pandora loadout.
 *
 * Views issue commands through this object and only render state that has flowed back
 * through OnStateChanged. This keeps the item and Pandora presenters independent.
 */
UCLASS()
class LABPROJECT_API UInfoLoadoutStore : public UObject
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;

	void Initialize(APdPlayerController* InController);
	void Deinitialize();
	void RefreshBindings();

	UInventoryComponent* GetInventoryComponent() const;
	UPandoraComponent* GetPandoraComponent() const;
	UEquipmentComponent* GetEquipmentComponent() const;

	UItemInstance* GetSelectedWeapon(EEnum_Direction Direction) const;
	UPandoraInstance* GetSelectedPandora(EEnum_Direction Direction) const;
	const UPandoraDefinition* GetSelectedPandoraDefinition(EEnum_Direction Direction) const;
	bool WouldSelectedDirectionChangeLoadout(EEnum_Direction Direction) const;

	bool RequestSetConsumableQuickSlot(int32 SlotIndex, UItemInstance* ItemInstance);
	bool RequestClearConsumableQuickSlot(int32 SlotIndex);
	bool RequestSetEquipmentSlot(FGameplayTag SlotTag, UItemInstance* ItemInstance);
	bool RequestClearEquipmentSlot(FGameplayTag SlotTag);
	bool RequestSetWeaponLoadoutSlot(EEnum_Direction Direction, UItemInstance* ItemInstance);
	bool RequestClearWeaponLoadoutSlot(EEnum_Direction Direction);
	bool RequestSetPandoraLoadoutSlot(
		EEnum_Direction Direction,
		const UPandoraDefinition* PandoraDefinition);
	bool RequestSelectLoadoutDirection(EEnum_Direction Direction);

	void NotifyPresentationAssetsReady();

	FInfoLoadoutStateChanged OnStateChanged;

private:
	void UnbindInventoryComponent();
	void UnbindPandoraComponent();
	void RebuildLoadoutState();
	void PublishStateChange(EInfoLoadoutStateChange Change);
	bool PublishRejectedCommand(EInfoLoadoutStateChange Change);

	void HandleInventoryChanged();
	void HandleWeaponLoadoutChanged();

	UFUNCTION()
	void HandlePandoraLoadoutChanged();

	UPROPERTY(Transient)
	TObjectPtr<APdPlayerController> OwningController;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> BoundInventoryComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPandoraComponent> BoundPandoraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UItemInstance> LeftWeapon;

	UPROPERTY(Transient)
	TObjectPtr<UItemInstance> UpWeapon;

	UPROPERTY(Transient)
	TObjectPtr<UItemInstance> RightWeapon;

	UPROPERTY(Transient)
	TObjectPtr<UPandoraInstance> LeftPandora;

	UPROPERTY(Transient)
	TObjectPtr<UPandoraInstance> UpPandora;

	UPROPERTY(Transient)
	TObjectPtr<UPandoraInstance> RightPandora;

	UPROPERTY(Transient)
	TObjectPtr<const UPandoraDefinition> LeftPandoraDefinition;

	UPROPERTY(Transient)
	TObjectPtr<const UPandoraDefinition> UpPandoraDefinition;

	UPROPERTY(Transient)
	TObjectPtr<const UPandoraDefinition> RightPandoraDefinition;

	FDelegateHandle InventoryChangedDelegateHandle;
	FDelegateHandle WeaponLoadoutChangedDelegateHandle;
};
