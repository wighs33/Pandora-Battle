#include "UI/Widget/LeftPandoraWidget.h"

#include "Common/Enum_Direction.h"
#include "Pandora/PandoraComponent.h"
#include "Pandora/PandoraInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LeftPandoraWidget)

void ULeftPandoraWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RebuildPandoraEquipSlotList();
	BindPandoraEquipSlotCallbacks();
}

void ULeftPandoraWidget::NativeDestruct()
{
	UnbindPandoraEquipSlotCallbacks();

	Super::NativeDestruct();
}

void ULeftPandoraWidget::ToggleActiveEquipSlots(bool bActive)
{
	RebuildPandoraEquipSlotList();

	for (UPandoraEquipSlotWidget* PandoraEquipSlot : PandoraEquipSlotList)
	{
		if (PandoraEquipSlot)
		{
			PandoraEquipSlot->SetIsEnabled(bActive);
		}
	}
}

void ULeftPandoraWidget::SelectPandoraEquipSlot(UPandoraEquipSlotWidget* InSelectedPandoraEquipSlot)
{
	if (!InSelectedPandoraEquipSlot)
	{
		return;
	}

	SelectedPandoraEquipSlot = InSelectedPandoraEquipSlot;

	OnClicked_PandoraEquipSlot.Broadcast(SelectedPandoraEquipSlot, bIsSelectedAnyButton);
	ToggleActiveEquipSlots(bIsSelectedAnyButton);

	SelectedPandoraEquipSlot->SetIsEnabled(true);
	bIsSelectedAnyButton = !bIsSelectedAnyButton;
}

void ULeftPandoraWidget::RefreshPandoraLoadoutSlots(const UPandoraComponent* PandoraComponent)
{
	RebuildPandoraEquipSlotList();

	if (FirstPandora)
	{
		FirstPandora->SetData(PandoraComponent ? PandoraComponent->GetPandoraLoadoutInstance(EEnum_Direction::Left) : nullptr);
	}

	if (SecondPandora)
	{
		SecondPandora->SetData(PandoraComponent ? PandoraComponent->GetPandoraLoadoutInstance(EEnum_Direction::Up) : nullptr);
	}

	if (ThirdPandora)
	{
		ThirdPandora->SetData(PandoraComponent ? PandoraComponent->GetPandoraLoadoutInstance(EEnum_Direction::Right) : nullptr);
	}
}

void ULeftPandoraWidget::HandlePandoraEquipSlotClicked(UPandoraEquipSlotWidget* PandoraEquipSlot)
{
	SelectPandoraEquipSlot(PandoraEquipSlot);
}

void ULeftPandoraWidget::RebuildPandoraEquipSlotList()
{
	PandoraEquipSlotList.Reset();
	PandoraEquipSlotList.Reserve(3);
	PandoraEquipSlotList.Add(FirstPandora);
	PandoraEquipSlotList.Add(SecondPandora);
	PandoraEquipSlotList.Add(ThirdPandora);
}

void ULeftPandoraWidget::BindPandoraEquipSlotCallbacks()
{
	RebuildPandoraEquipSlotList();

	for (UPandoraEquipSlotWidget* PandoraEquipSlot : PandoraEquipSlotList)
	{
		if (PandoraEquipSlot)
		{
			PandoraEquipSlot->OnClicked_PandoraEquipSlot.AddUniqueDynamic(this, &ThisClass::HandlePandoraEquipSlotClicked);
		}
	}
}

void ULeftPandoraWidget::UnbindPandoraEquipSlotCallbacks()
{
	for (UPandoraEquipSlotWidget* PandoraEquipSlot : PandoraEquipSlotList)
	{
		if (PandoraEquipSlot)
		{
			PandoraEquipSlot->OnClicked_PandoraEquipSlot.RemoveDynamic(this, &ThisClass::HandlePandoraEquipSlotClicked);
		}
	}
}
