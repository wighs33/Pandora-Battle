#pragma once

#include "Blueprint/DragDropOperation.h"
#include "ItemSlotDragDropOperation.generated.h"

class UInventorySlotViewData;
class UItemInstance;

UCLASS(BlueprintType)
class LABPROJECT_API UItemSlotDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	void Initialize(int32 InSourceSlotIndex, UItemInstance* InItemInstance, UInventorySlotViewData* InSourceSlotData);

	UFUNCTION(BlueprintPure, Category = "!UI|Inventory|DragDrop")
	int32 GetSourceSlotIndex() const { return SourceSlotIndex; }

	UFUNCTION(BlueprintPure, Category = "!UI|Inventory|DragDrop")
	UItemInstance* GetItemInstance() const { return ItemInstance; }

	UFUNCTION(BlueprintPure, Category = "!UI|Inventory|DragDrop")
	UInventorySlotViewData* GetSourceSlotData() const { return SourceSlotData; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!UI|Inventory|DragDrop", meta = (AllowPrivateAccess = "true"))
	int32 SourceSlotIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!UI|Inventory|DragDrop", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UItemInstance> ItemInstance;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!UI|Inventory|DragDrop", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInventorySlotViewData> SourceSlotData;
};
