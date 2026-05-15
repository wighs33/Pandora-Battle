#include "UI/Widget/SkinEquipSlotWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Skin/SkinDefinition.h"
#include "Skin/SkinInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinEquipSlotWidget)

void USkinEquipSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ItemButton)
	{
		ItemButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleButtonClicked);
	}
}

void USkinEquipSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplySlotText();
}

void USkinEquipSlotWidget::NativeDestruct()
{
	if (ItemButton)
	{
		ItemButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleButtonClicked);
	}

	Super::NativeDestruct();
}

void USkinEquipSlotWidget::BroadcastClickedSkinEquipSlot(USkinEquipSlotWidget* SkinEquipSlot)
{
	OnClicked_SkinEquipSlot.Broadcast(SkinEquipSlot ? SkinEquipSlot : this);
}

void USkinEquipSlotWidget::SetText(const FText& InText)
{
	SlotText = InText;
	ApplySlotText();
}

void USkinEquipSlotWidget::SetData(USkinInstance* Target)
{
	SkinInstance = Target;

	if (!SkinInstance || !SkinInstance->SkinDefinition)
	{
		SetText(FText::GetEmpty());
		return;
	}

	SetText(SkinInstance->SkinDefinition->DisplayName);
}

void USkinEquipSlotWidget::HandleButtonClicked()
{
	BroadcastClickedSkinEquipSlot(this);
}

void USkinEquipSlotWidget::ApplySlotText()
{
	if (ApplyText)
	{
		ApplyText->SetText(SlotText);
	}
}
