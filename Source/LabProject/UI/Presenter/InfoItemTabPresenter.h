#pragma once

#include "CoreMinimal.h"
#include "Common/Enum_Direction.h"
#include "GameplayTagContainer.h"
#include "UI/Presenter/InfoTabPresenterBase.h"
#include "InfoItemTabPresenter.generated.h"

class UEquipSlotWidget;
class UInfoLoadoutStore;
class UItemInstance;
class URightInventoryWidget;

UCLASS()
class LABPROJECT_API UInfoItemTabPresenter : public UInfoTabPresenterBase
{
	GENERATED_BODY()

public:
	virtual void BindInfoUi(UInfoWidget* InInfoWidget) override;
	virtual void Deinitialize() override;

	void SetLoadoutStore(UInfoLoadoutStore* InLoadoutStore);
	void SetActive(bool bInActive);
	void Activate();
	void HandleInfoUiOpened();
	void HandleInventoryChanged();
	void HandleWeaponLoadoutChanged();
	void HandlePandoraLoadoutChanged();
	void HandlePresentationAssetsReady();
	void ResetInventoryDisplaySlots();

	UItemInstance* GetSelectedWeapon(EEnum_Direction Direction) const;
	void RefreshEquipmentSlots() const;
	void RefreshInventoryTileView();
	void ReconcileCurrentWeaponLoadoutDirection();

	UFUNCTION()
	void HandleItemSlotClicked(UObject* Item);

	UFUNCTION()
	void HandleItemEquipSlotClicked(
		FGameplayTag EquipTypeTag,
		UEquipSlotWidget* SelectedEquipSlot,
		bool bIsSelectedAnyButton);

	UFUNCTION()
	void HandleItemEquipSlotDropped(
		FGameplayTag EquipTypeTag,
		UEquipSlotWidget* TargetEquipSlot,
		UItemInstance* ItemInstance);

	UFUNCTION()
	void HandleItemDroppedToCharacter(UItemInstance* ItemInstance);

	UFUNCTION()
	void HandleInventorySlotDropped(
		int32 SourceSlotIndex,
		int32 TargetSlotIndex,
		UItemInstance* SourceItem);

	UFUNCTION()
	void HandleItemFilterTypeClicked(FGameplayTag TypeTag);

	UFUNCTION()
	void HandleItemFilterAllClicked();

private:
	void BindEvents();
	void UnbindEvents();
	void BindInventoryTileItemClicked();
	void ClearInventoryTileItemClicked() const;
	void ClearEquipmentSlot(UEquipSlotWidget* TargetEquipSlot, FGameplayTag EquipTypeTag);
	void ReconcileInventoryDisplaySlots(const TArray<UObject*>& InventoryItems);
	void BuildInventoryViewSlots(const TArray<UObject*>& SourceItems, TArray<UObject*>& OutViewItems);
	int32 FindInventoryDisplaySlotIndexByItemId(FGuid ItemId) const;
	int32 RemoveEquippedItemsFromInventoryList(TArray<UObject*>& InOutItemList) const;
	FGameplayTag GetWeaponItemTypeTag() const;
	FGameplayTag GetConsumableItemTypeTag() const;

	UPROPERTY(Transient)
	TObjectPtr<UEquipSlotWidget> SelectedEquipSlot;

	UPROPERTY(Transient)
	FGameplayTag SelectedEquipTypeTag;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UItemInstance>> InventoryDisplaySlots;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UItemInstance>> CachedInventoryViewSlots;

	UPROPERTY(Transient)
	FGameplayTag CurrentItemFilterTag;

	TWeakObjectPtr<UInfoLoadoutStore> LoadoutStore;
	FGuid PendingClearedWeaponId;
	EEnum_Direction PendingClearedWeaponDirection = EEnum_Direction::Center;
	bool bUseItemTypeFilter = false;
	bool bInventoryDisplaySlotsInitialized = false;
	bool bActive = false;
};
