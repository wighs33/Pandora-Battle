#include "UI/Widget/SkinSlotWidget.h"

#include "Components/TextBlock.h"
#include "Skin/SkinDefinition.h"
#include "Skin/SkinInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinSlotWidget)

void USkinSlotWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	if (USkinInstance* SkinInstance = Cast<USkinInstance>(ListItemObject))
	{
		SetData(SkinInstance);
	}
}

void USkinSlotWidget::SetData(USkinInstance* Target)
{
	CachedData = Target;

	const USkinDefinition* SkinDefinition = CachedData ? CachedData->SkinDefinition.Get() : nullptr;
	const FText DisplayName = SkinDefinition ? SkinDefinition->DisplayName : FText::GetEmpty();

	if (TextBlock)
	{
		TextBlock->SetText(DisplayName);
	}
}
