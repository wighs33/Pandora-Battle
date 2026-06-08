#include "UI/Widget/SkinSlotDragDropOperation.h"

#include "Skin/SkinInstance.h"
#include "UI/Widget/SkinSlotViewData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinSlotDragDropOperation)

void USkinSlotDragDropOperation::Initialize(const int32 InSourceSlotIndex, USkinInstance* InSkinInstance, USkinSlotViewData* InSourceSlotData)
{
	SourceSlotIndex = InSourceSlotIndex;
	SkinInstance = InSkinInstance;
	SourceSlotData = InSourceSlotData;
}
