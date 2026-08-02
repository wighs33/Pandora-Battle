#pragma once

#include "CoreMinimal.h"
#include "Common/Enum_Direction.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "InfoUiPresenter.generated.h"

class APdPlayerController;
struct FStreamableHandle;
class UInventoryComponent;
class UEquipSlotWidget;
class UInfoWidget;
class UItemDefinition;
class UItemInstance;
class UPandoraDefinition;
class UPandoraInstance;
class UPandoraEquipSlotWidget;
class UPandoraComponent;
class UPandoraTreeComponent;
class URightInventoryWidget;
class USkinEquipSlotWidget;
class USkinInstance;
class UTileView;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UInfoUiPresenter : public UObject
{
	GENERATED_BODY()

public:
	UInfoUiPresenter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual UWorld* GetWorld() const override;

	void Initialize(APdPlayerController* InController);
	void Deinitialize();
	void BindInfoUi(UInfoWidget* InInfoWidget);
	UItemInstance* GetSelectedWeapon(EEnum_Direction Direction) const;
	UPandoraInstance* GetSelectedPandora(EEnum_Direction Direction) const;
	bool WouldSelectedPandoraDirectionChangeLoadout(EEnum_Direction Direction) const;

	UFUNCTION()
	void HandleSelectedPandoraDirection(EEnum_Direction Direction);

	UFUNCTION()
	void HandleOpenedInfoUi();

	UFUNCTION()
	void HandleClickedStatUpButton(FGameplayTag StatTag);

	UFUNCTION()
	void HandleClickedStatDownButton(FGameplayTag StatTag);

	UFUNCTION()
	void HandleClickedInfoCenterButton(FGameplayTag LeftUiTag, FGameplayTag RightUiTag);

	UFUNCTION()
	void HandleClickedItemSlot(UObject* Item);

	UFUNCTION()
	void HandleClickedItemEquipTypeSlot(FGameplayTag EquipTypeTag, UEquipSlotWidget* SelectedEquipSlot, bool bIsSelectedAnyButton);

	UFUNCTION()
	void HandleDroppedItemEquipTypeSlot(FGameplayTag EquipTypeTag, UEquipSlotWidget* TargetEquipSlot, UItemInstance* ItemInstance);

	UFUNCTION()
	void HandleDroppedItemToCharacterPanel(UItemInstance* ItemInstance);

	UFUNCTION()
	void HandleDroppedInventorySlot(int32 SourceSlotIndex, int32 TargetSlotIndex, UItemInstance* SourceItem);

	UFUNCTION()
	void HandleClickedItemFilterTypeButton(FGameplayTag TypeTag);

	UFUNCTION()
	void HandleClickedItemFilterAllButton();

	UFUNCTION()
	void HandleClickedSkinSlot(UObject* Item);

	UFUNCTION()
	void HandleClickedSkinEquipTypeSlot(FGameplayTag EquipTypeTag, USkinEquipSlotWidget* SelectedEquipSlot, bool bIsSelectedAnyButton);

	UFUNCTION()
	void HandleDroppedSkinEquipTypeSlot(FGameplayTag EquipTypeTag, USkinEquipSlotWidget* TargetSkinEquipSlot, USkinInstance* SkinInstance);

	UFUNCTION()
	void HandleDroppedSkinToCharacterPanel(USkinInstance* SkinInstance);

	UFUNCTION()
	void HandleClickedSkinFilterTypeButton(FGameplayTag TypeTag);

	UFUNCTION()
	void HandleClickedSkinFilterAllButton();

	UFUNCTION()
	void HandleClickedPandoraSlot(UObject* Item);

	UFUNCTION()
	void HandleClickedPandoraEquipSlot(UPandoraEquipSlotWidget* SelectedPandoraEquipSlot, bool bIsSelectedAnyButton);

	UFUNCTION()
	void HandleClickedPandoraFilterTypeButton(FGameplayTag TypeTag);

	UFUNCTION()
	void HandleClickedPandoraFilterAllButton();

private:
	UInfoWidget* GetInfoWidget() const;
	class USelectPandoraWidget* GetSelectPandoraWidget() const;
	APdPlayerController* GetController() const;
	class APdPlayerState* GetCachedPlayerState() const;
	void BindInventoryChangeNotification();
	void UnbindInventoryChangeNotification();
	void BeginItemPresentationPreload();
	void ReleaseItemPresentationPreload();
	void BindPandoraLoadoutChangeNotification();
	void UnbindPandoraLoadoutChangeNotification();
	void UnbindInfoUiEvents();
	void BindStatusWidgetEvents();
	void HandleInventoryChanged();
	void HandlePandoraWeaponLoadoutChanged();
	UFUNCTION()
	void HandlePandoraLoadoutChanged();
	void RefreshInventoryTileView();
	int32 RemoveEquippedItemsFromInventoryList(TArray<UObject*>& InOutItemList) const;
	void ResetInventoryDisplaySlots();
	void ReconcileInventoryDisplaySlots(const TArray<UObject*>& InventoryItems);
	void BuildInventoryViewSlots(const TArray<UObject*>& SourceItems, TArray<UObject*>& OutViewItems);
	int32 FindInventoryDisplaySlotIndexByItemId(FGuid ItemId) const;
	void BindRightInventoryWidgetEvents(URightInventoryWidget* RightInventoryWidget);
	FGameplayTag GetProfileLeftUiTag() const;
	FGameplayTag GetEquipmentLeftUiTag() const;
	FGameplayTag GetSkinEquipmentLeftUiTag() const;
	FGameplayTag GetPandoraEquipmentLeftUiTag() const;
	FGameplayTag GetWeaponItemTypeTag() const;
	FGameplayTag GetConsumableItemTypeTag() const;
	void RefreshSelectPandoraCompatibilityState() const;
	void RefreshSelectPandoraLoadoutImages() const;
	void ReconcileCurrentWeaponLoadoutDirection();
	int32 ResolveCurrentEquippedWeaponSlotNumber() const;
	void RefreshLeftEquipmentSlots() const;
	void RefreshLeftSkinSlots() const;
	void RefreshLeftPandoraSlots() const;
	bool IsPandoraOwnedForEquipInventory(const UPandoraInstance* PandoraInstance) const;
	void BuildPandoraTileViewItems(TArray<UObject*>& OutListItems, FGameplayTag TypeTag, bool bOwnedOnly) const;
	void RefreshPandoraTileView() const;
	void BindPandoraTileItemClicked();
	bool ResolvePandoraLoadoutSlotForClick(
		const UPandoraComponent* PandoraComponent,
		const UPandoraDefinition* PandoraDefinition,
		EEnum_Direction& OutDirection,
		int32& OutSlotNumber) const;
	void ResetPandoraEquipSlotClickState();
	void ClearInventoryClickEquipBinding() const;
	void ClearSkinClickEquipBinding() const;
	void ClearEquipmentSlot(UEquipSlotWidget* TargetEquipSlot, FGameplayTag EquipTypeTag);
	void ClearSkinEquipSlot(USkinEquipSlotWidget* TargetSkinEquipSlot, FGameplayTag EquipTypeTag);
	void ClearPandoraEquipSlot(UPandoraEquipSlotWidget* TargetPandoraEquipSlot);

	UPROPERTY(Transient)
	TObjectPtr<APdPlayerController> OwningController = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UInfoWidget> InfoWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> BoundInventoryComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPandoraComponent> BoundPandoraComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UEquipSlotWidget> CachedSelectedEquipSlot = nullptr;

	UPROPERTY(Transient)
	FGameplayTag CachedSelectedEquipTypeTag;

	UPROPERTY(Transient)
	TObjectPtr<USkinEquipSlotWidget> CachedSelectedSkinEquipSlot = nullptr;

	UPROPERTY(Transient)
	FGameplayTag CachedSelectedSkinEquipTypeTag;

	UPROPERTY(Transient)
	TObjectPtr<UPandoraEquipSlotWidget> CachedSelectedPandoraEquipSlot = nullptr;

	UPROPERTY(Transient)
	FGameplayTag CurrentPandoraFilterTag;

	UPROPERTY(Transient)
	bool bUsePandoraTypeFilter = false;

	UPROPERTY(Transient)
	bool bShowOnlyOwnedPandorasForEquipSlot = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UItemInstance>> InventoryDisplaySlots;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UItemInstance>> CachedInventoryViewSlots;

	UPROPERTY(Transient)
	FGameplayTag CurrentLeftUiTag;

	UPROPERTY(Transient)
	FGameplayTag CurrentItemFilterTag;

	UPROPERTY(Transient)
	bool bUseItemTypeFilter = false;

	UPROPERTY(Transient)
	bool bInventoryDisplaySlotsInitialized = false;

	TWeakObjectPtr<UTileView> BoundPandoraTileView;
	FDelegateHandle InventoryChangedDelegateHandle;
	FDelegateHandle PandoraWeaponLoadoutChangedDelegateHandle;
	FDelegateHandle PandoraTileItemClickedDelegateHandle;
	int32 ItemPresentationPreloadGeneration = 0;
	TSharedPtr<FStreamableHandle> ItemPresentationPreloadHandle;

	FGuid PendingClearedWeaponId;
	EEnum_Direction PendingClearedWeaponDirection = EEnum_Direction::Center;
};
