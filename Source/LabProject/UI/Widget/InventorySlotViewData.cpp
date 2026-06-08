#include "UI/Widget/InventorySlotViewData.h"

#include "Item/ItemInstance.h"
#include "UI/Widget/ItemViewData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InventorySlotViewData)

void UInventorySlotViewData::Initialize(const int32 InSlotIndex, UItemInstance* InItemInstance)
{
	SlotIndex = InSlotIndex;
	ItemInstance = InItemInstance;
	ViewData = FPdItemViewDataBuilder::FromItemInstance(InItemInstance);
}
