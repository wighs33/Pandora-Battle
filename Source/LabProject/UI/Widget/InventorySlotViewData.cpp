#include "UI/Widget/InventorySlotViewData.h"

#include "Item/ItemInstance.h"
#include "UI/Widget/ItemViewData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InventorySlotViewData)

void UInventorySlotViewData::Initialize(
	const int32 InSlotIndex,
	UItemInstance* InItemInstance,
	const bool bInDuplicateWeaponOrEquipment,
	const bool bInAssigned)
{
	SlotIndex = InSlotIndex;
	ItemInstance = InItemInstance;
	ViewData = FItemViewDataBuilder::FromItemInstance(InItemInstance);
	bDuplicateWeaponOrEquipment = bInDuplicateWeaponOrEquipment;
	bAssigned = bInAssigned;
}
