#include "UI/Widget/ItemSlotDragDropOperation.h"

#include "Item/ItemInstance.h"
#include "UI/Widget/InventorySlotViewData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ItemSlotDragDropOperation)

void UItemSlotDragDropOperation::Initialize(const int32 InSourceSlotIndex, UItemInstance* InItemInstance, UInventorySlotViewData* InSourceSlotData)
{
	SourceSlotIndex = InSourceSlotIndex;
	ItemInstance = InItemInstance;
	SourceSlotData = InSourceSlotData;
}
