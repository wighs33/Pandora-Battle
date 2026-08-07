#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "UI/Widget/FilterButtonHighlight.h"
#include "RightInventoryWidget.generated.h"

class UButton;
class UEditableTextBox;
class UInventorySlotViewData;
class UItemDefinition;
class UItemInstance;
class UTextBlock;
class UTileView;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdOnClickedInventoryFilterAllButton);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnClickedInventoryFilterTypeButton, FGameplayTag, TypeTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPdOnDroppedInventorySlot, int32, SourceSlotIndex, int32, TargetSlotIndex, UItemInstance*, SourceItem);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API URightInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	URightInventoryWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Filter
	UFUNCTION(BlueprintCallable, Category = "!UI|Inventory")
	void SelectAllFilter();

	UFUNCTION(BlueprintCallable, Category = "!UI|Inventory", meta = (Categories = "Item"))
	void SelectTypeFilter(FGameplayTag TypeTag);

	UFUNCTION(BlueprintCallable, Category = "!UI|Inventory")
	void ToggleActiveFiliterButtons(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "!UI|Inventory")
	void ResetFilterHighlightToAll();

	//------------------------------------------------------------------------------------------------------------------
	//--- Tile View
	UFUNCTION(BlueprintCallable, Category = "!UI|Inventory")
	void SetTileView(const TArray<UObject*>& InListItems);

	UFUNCTION(BlueprintCallable, Category = "!UI|Inventory")
	void ClearTileViewItemClicked();

	UFUNCTION(BlueprintPure, Category = "!UI|Inventory")
	UTileView* GetTileView() const { return TileView; }

	UFUNCTION(BlueprintCallable, Category = "!UI|Inventory")
	void SelectInventorySlot(UInventorySlotViewData* SlotViewData);

	UFUNCTION(BlueprintPure, Category = "!UI|Inventory")
	int32 GetInventorySlotCount() const { return InventorySlotCount; }

	UFUNCTION(BlueprintCallable, Category = "!UI|Inventory")
	void SetInventorySlotCount(int32 InInventorySlotCount);

	void BroadcastDroppedInventorySlot(int32 SourceSlotIndex, int32 TargetSlotIndex, UItemInstance* SourceItem);

	UPROPERTY(BlueprintAssignable, Category = "!UI|Inventory")
	FPdOnClickedInventoryFilterAllButton OnClicked_FilterAllButton;

	UPROPERTY(BlueprintAssignable, Category = "!UI|Inventory")
	FPdOnClickedInventoryFilterTypeButton OnClicked_FilterTypeButton;

	UPROPERTY(BlueprintAssignable, Category = "!UI|Inventory")
	FPdOnDroppedInventorySlot OnDropped_InventorySlot;

protected:
	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//------------------------------------------------------------------------------------------------------------------
	//--- Filter Buttons
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Inventory", meta = (BindWidget))
	TObjectPtr<UButton> AllButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Inventory", meta = (BindWidget))
	TObjectPtr<UButton> WeaponButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Inventory", meta = (BindWidget))
	TObjectPtr<UButton> EquipmentButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Inventory", meta = (BindWidget))
	TObjectPtr<UButton> ValuableButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Inventory", meta = (BindWidget))
	TObjectPtr<UButton> ConsumableButton;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Inventory")
	TArray<TObjectPtr<UButton>> FilterButtonList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Inventory|Filter")
	FLinearColor SelectedFilterAccentColor = FLinearColor(0.0f, 0.45f, 1.0f, 1.0f);

	//------------------------------------------------------------------------------------------------------------------
	//--- Tile View
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Inventory", meta = (BindWidget))
	TObjectPtr<UTileView> TileView;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Inventory|Search", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Search;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Inventory|Search", meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> SearchBox;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Inventory|Combine", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Inventory|Slots", meta = (ClampMin = "0"))
	int32 InventorySlotCount = 40;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Inventory|Slots")
	TArray<TObjectPtr<UInventorySlotViewData>> CachedSlotViewData;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> CachedSourceListItems;

	UPROPERTY(Transient)
	FString ActiveSearchText;

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Button Callbacks
	UFUNCTION()
	void OnAllButtonClicked();

	UFUNCTION()
	void OnWeaponButtonClicked();

	UFUNCTION()
	void OnEquipmentButtonClicked();

	UFUNCTION()
	void OnValuableButtonClicked();

	UFUNCTION()
	void OnConsumableButtonClicked();

	UFUNCTION()
	void OnSearchButtonClicked();

	void RebuildFilterButtonList();
	void RebuildTileViewFromCachedSourceItems();
	void UpdateCombineMessage(bool bHasCombinableItems) const;
	bool DoesItemMatchSearch(const UItemInstance* ItemInstance, const FString& SearchText) const;
	bool IsDuplicateHighlightCandidate(const UItemDefinition* ItemDefinition) const;
	void ApplyWidgetDefinitionSettings();
	UButton* ResolveFilterButton(FGameplayTag TypeTag) const;
	FGameplayTag GetWeaponTypeTag() const;
	FGameplayTag GetEquipmentTypeTag() const;
	FGameplayTag GetValuableTypeTag() const;
	FGameplayTag GetConsumableTypeTag() const;

	FGameplayTag WeaponTypeTagOverride;
	FGameplayTag EquipmentTypeTagOverride;
	FGameplayTag ValuableTypeTagOverride;
	FGameplayTag ConsumableTypeTagOverride;

	FFilterButtonHighlightState FilterButtonHighlightState;
};
