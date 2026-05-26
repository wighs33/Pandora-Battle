#include "UI/Widget/LeftPandoraWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Pandora/PandoraDefinition.h"
#include "Pandora/PandoraInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LeftPandoraWidget)

namespace
{
	void SetImageResource(UImage* Image, UObject* ResourceObject)
	{
		if (!Image)
		{
			return;
		}

		FSlateBrush Brush = Image->GetBrush();
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.SetResourceObject(ResourceObject);
		Image->SetBrush(Brush);
	}
}

void ULeftPandoraWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RebuildSkillWidgetLists();
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

void ULeftPandoraWidget::SetSkillInfo(const TArray<FSkill>& InSkills)
{
	RebuildSkillWidgetLists();

	const int32 SkillCount = FMath::Min3(InSkills.Num(), SkillIconList.Num(), SkillNameList.Num());
	for (int32 SkillIndex = 0; SkillIndex < SkillCount; ++SkillIndex)
	{
		const FSkill& Skill = InSkills[SkillIndex];

		if (SkillIconList[SkillIndex])
		{
			SetImageResource(SkillIconList[SkillIndex], Skill.GetIconResource());
		}

		if (SkillNameList[SkillIndex])
		{
			SkillNameList[SkillIndex]->SetText(Skill.GetDisplayName());
		}
	}
}

void ULeftPandoraWidget::HandlePandoraEquipSlotClicked(UPandoraEquipSlotWidget* PandoraEquipSlot)
{
	SelectPandoraEquipSlot(PandoraEquipSlot);
}

void ULeftPandoraWidget::HandlePandoraEquipSlotHovered(UPandoraEquipSlotWidget* PandoraEquipSlot)
{
	const UPandoraInstance* PandoraInstance = GetCachedPandoraInstance(PandoraEquipSlot);
	const UPandoraDefinition* PandoraDefinition = PandoraInstance ? PandoraInstance->PandoraDefinition.Get() : nullptr;
	if (!PandoraDefinition)
	{
		return;
	}

	SetSkillInfo(PandoraDefinition->Skill);
}

void ULeftPandoraWidget::RebuildSkillWidgetLists()
{
	SkillIconList.Reset();
	SkillIconList.Reserve(4);
	SkillIconList.Add(FirstSkillIcon);
	SkillIconList.Add(SecondSkillIcon);
	SkillIconList.Add(ThirdSkillIcon);
	SkillIconList.Add(FourthSkillIcon);

	SkillNameList.Reset();
	SkillNameList.Reserve(4);
	SkillNameList.Add(FirstSkillName);
	SkillNameList.Add(SecondSkillName);
	SkillNameList.Add(ThirdSkillName);
	SkillNameList.Add(FourthSkillName);
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
			PandoraEquipSlot->OnHovered_PandoraEquipSlot.AddUniqueDynamic(this, &ThisClass::HandlePandoraEquipSlotHovered);
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
			PandoraEquipSlot->OnHovered_PandoraEquipSlot.RemoveDynamic(this, &ThisClass::HandlePandoraEquipSlotHovered);
		}
	}
}

UPandoraInstance* ULeftPandoraWidget::GetCachedPandoraInstance(const UPandoraEquipSlotWidget* PandoraEquipSlot) const
{
	if (!PandoraEquipSlot)
	{
		return nullptr;
	}

	return PandoraEquipSlot->GetCachedData();
}
