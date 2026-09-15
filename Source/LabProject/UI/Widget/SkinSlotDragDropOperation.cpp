#include "UI/Widget/SkinSlotDragDropOperation.h"

#include "Definition/Skin/SkinDefinition.h"
#include "UI/Widget/SkinSlotViewData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinSlotDragDropOperation)

void USkinSlotDragDropOperation::Initialize(const int32 InSourceSlotIndex, const USkinDefinition* InSkinDefinition, USkinSlotViewData* InSourceSlotData)
{
	SourceSlotIndex = InSourceSlotIndex;
	SkinDefinition = InSkinDefinition;
	SourceSlotData = InSourceSlotData;
}
