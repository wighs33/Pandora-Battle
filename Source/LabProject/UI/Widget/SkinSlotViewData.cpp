#include "UI/Widget/SkinSlotViewData.h"

#include "Skin/SkinInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinSlotViewData)

void USkinSlotViewData::Initialize(const int32 InSlotIndex, USkinInstance* InSkinInstance)
{
	SlotIndex = InSlotIndex;
	SkinInstance = InSkinInstance;
}
