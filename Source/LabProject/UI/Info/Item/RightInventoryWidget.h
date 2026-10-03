#pragma once

#include "CoreMinimal.h"
#include "UI/Info/RightListPanelWidget.h"
#include "RightInventoryWidget.generated.h"

class UInventorySlotViewData;
class UItemDefinition;
class UItemInstance;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPdOnDroppedInventorySlot, int32, SourceSlotIndex, int32, TargetSlotIndex, UItemInstance*, SourceItem);

/** 인벤토리 슬롯 목록. 빈 칸까지 슬롯 수만큼 보여 주고, 합칠 수 있는 무기·장비가 있으면 안내를 띄운다. */
UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API URightInventoryWidget : public URightListPanelWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void ApplyWidgetDefinitionSettings() override;
	virtual void RebuildTileView() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	void SetAssignedItemIds(const TSet<FGuid>& InAssignedItemIds);

	UFUNCTION(BlueprintCallable, Category = "!UI|Inventory")
	void SelectInventorySlot(UInventorySlotViewData* SlotViewData);

	UFUNCTION(BlueprintPure, Category = "!UI|Inventory")
	int32 GetInventorySlotCount() const { return InventorySlotCount; }

	UFUNCTION(BlueprintCallable, Category = "!UI|Inventory")
	void SetInventorySlotCount(int32 InInventorySlotCount);

	void BroadcastDroppedInventorySlot(int32 SourceSlotIndex, int32 TargetSlotIndex, UItemInstance* SourceItem);

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void UpdateCombineMessage(bool bHasCombinableItems) const;
	bool IsDuplicateHighlightCandidate(const UItemDefinition* ItemDefinition) const;

	// ApplyWidgetDefinitionSettings에서 등록하는 분류 순서.
	static constexpr int32 WeaponFilterIndex = 0;
	static constexpr int32 EquipmentFilterIndex = 1;

public:
	UPROPERTY(BlueprintAssignable, Category = "!UI|Inventory")
	FPdOnDroppedInventorySlot OnDropped_InventorySlot;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Inventory", meta = (BindWidget))
	TObjectPtr<UButton> WeaponButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Inventory", meta = (BindWidget))
	TObjectPtr<UButton> EquipmentButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Inventory", meta = (BindWidget))
	TObjectPtr<UButton> ValuableButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Inventory", meta = (BindWidget))
	TObjectPtr<UButton> ConsumableButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Inventory|Combine", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Inventory|Slots", meta = (ClampMin = "0"))
	int32 InventorySlotCount = 40;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Inventory|Slots")
	TArray<TObjectPtr<UInventorySlotViewData>> CachedSlotViewData;

	TSet<FGuid> AssignedItemIds;
};
