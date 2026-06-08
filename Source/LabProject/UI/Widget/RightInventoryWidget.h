#pragma once

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "RightInventoryWidget.generated.h"

class UButton;
class UInventorySlotViewData;
class UTileView;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdOnClickedInventoryFilterAllButton);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnClickedInventoryFilterTypeButton, FGameplayTag, TypeTag);

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

	//------------------------------------------------------------------------------------------------------------------
	//--- Tile View
	UFUNCTION(BlueprintCallable, Category = "!UI|Inventory")
	void SetTileView(const TArray<UObject*>& InListItems);

	UFUNCTION(BlueprintCallable, Category = "!UI|Inventory")
	void ClearTileViewItemClicked();

	UFUNCTION(BlueprintPure, Category = "!UI|Inventory")
	UTileView* GetTileView() const { return TileView; }

	UFUNCTION(BlueprintPure, Category = "!UI|Inventory")
	int32 GetInventorySlotCount() const { return InventorySlotCount; }

	UPROPERTY(BlueprintAssignable, Category = "!UI|Inventory")
	FPdOnClickedInventoryFilterAllButton OnClicked_FilterAllButton;

	UPROPERTY(BlueprintAssignable, Category = "!UI|Inventory")
	FPdOnClickedInventoryFilterTypeButton OnClicked_FilterTypeButton;

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

	//------------------------------------------------------------------------------------------------------------------
	//--- Tile View
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Inventory", meta = (BindWidget))
	TObjectPtr<UTileView> TileView;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Inventory|Slots", meta = (ClampMin = "0"))
	int32 InventorySlotCount = 40;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Inventory|Slots")
	TArray<TObjectPtr<UInventorySlotViewData>> CachedSlotViewData;

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

	void RebuildFilterButtonList();
	FGameplayTag GetWeaponTypeTag() const;
	FGameplayTag GetEquipmentTypeTag() const;
	FGameplayTag GetValuableTypeTag() const;
	FGameplayTag GetConsumableTypeTag() const;
};
