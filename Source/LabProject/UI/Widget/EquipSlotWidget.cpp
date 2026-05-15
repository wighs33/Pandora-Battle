#include "UI/Widget/EquipSlotWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Item/ItemDefinition.h"
#include "Item/ItemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipSlotWidget)

DEFINE_LOG_CATEGORY_STATIC(LogEquipSlotWidget, Log, All);

void UEquipSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogEquipSlotWidget, Log, TEXT("[InventoryFilter] EquipSlot NativeConstruct: widget=%s button=%s nth=%d"),
		*GetNameSafe(this),
		*GetNameSafe(ItemButton),
		Nth);

	if (ItemButton)
	{
		ItemButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleButtonClicked);
	}
}

void UEquipSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplySlotText();
}

void UEquipSlotWidget::NativeDestruct()
{
	if (ItemButton)
	{
		ItemButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleButtonClicked);
	}

	Super::NativeDestruct();
}

void UEquipSlotWidget::BroadcastClickedEquipSlot(UEquipSlotWidget* ItemSlot)
{
	UE_LOG(LogEquipSlotWidget, Log, TEXT("[InventoryFilter] EquipSlot broadcast clicked: widget=%s itemSlot=%s nth=%d text=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ItemSlot ? ItemSlot : this),
		(ItemSlot ? ItemSlot : this)->GetNth(),
		*(ItemSlot ? ItemSlot : this)->GetSlotText().ToString());
	OnClicked_EquipSlot.Broadcast(ItemSlot ? ItemSlot : this);
}

void UEquipSlotWidget::SetText(const FText& InText)
{
	SlotText = InText;
	ApplySlotText();
}

void UEquipSlotWidget::SetData(UItemInstance* Target)
{
	ItemInstance = Target;
	UE_LOG(LogEquipSlotWidget, Log, TEXT("[InventoryFilter] EquipSlot SetData: widget=%s nth=%d item=%s definition=%s"),
		*GetNameSafe(this),
		Nth,
		*GetNameSafe(ItemInstance),
		*GetNameSafe(ItemInstance ? ItemInstance->ItemDefinition.Get() : nullptr));

	if (!ItemInstance || !ItemInstance->ItemDefinition)
	{
		SetText(FText::GetEmpty());
		return;
	}

	SetText(ItemInstance->ItemDefinition->DisplayName);
}

void UEquipSlotWidget::HandleButtonClicked()
{
	UE_LOG(LogEquipSlotWidget, Log, TEXT("[InventoryFilter] EquipSlot button clicked: widget=%s nth=%d text=%s button=%s"),
		*GetNameSafe(this),
		Nth,
		*SlotText.ToString(),
		*GetNameSafe(ItemButton));
	BroadcastClickedEquipSlot(this);
}

void UEquipSlotWidget::ApplySlotText()
{
	if (ApplyText)
	{
		ApplyText->SetText(SlotText);
	}
}
