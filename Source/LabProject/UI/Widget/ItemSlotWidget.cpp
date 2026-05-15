#include "UI/Widget/ItemSlotWidget.h"

#include "Components/TextBlock.h"
#include "Item/ItemDefinition.h"
#include "Item/ItemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ItemSlotWidget)

void UItemSlotWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	if (UItemInstance* ItemInstance = Cast<UItemInstance>(ListItemObject))
	{
		SetData(ItemInstance);
	}
}

void UItemSlotWidget::SetData(UItemInstance* Target)
{
	CachedData = Target;

	const UItemDefinition* ItemDefinition = CachedData ? CachedData->ItemDefinition.Get() : nullptr;
	const FText DisplayName = ItemDefinition ? ItemDefinition->DisplayName : FText::GetEmpty();

	if (TextBlock)
	{
		TextBlock->SetText(DisplayName);
	}
}
