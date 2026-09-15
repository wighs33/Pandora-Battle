#include "UI/Widget/SkinSlotViewData.h"

#include "Definition/Skin/SkinDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinSlotViewData)

void USkinSlotViewData::Initialize(
	const int32 InSlotIndex,
	const USkinDefinition* InSkinDefinition,
	const bool bInAssigned)
{
	SlotIndex = InSlotIndex;
	SkinDefinition = InSkinDefinition;
	bAssigned = bInAssigned;
}
