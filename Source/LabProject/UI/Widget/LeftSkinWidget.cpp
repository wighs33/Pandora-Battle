#include "UI/Widget/LeftSkinWidget.h"

#include "Common/ProjectTagConfig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LeftSkinWidget)

ULeftSkinWidget::ULeftSkinWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULeftSkinWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	RebuildSkinEquipSlotList();
	RebuildEquipSlotNameList();
	ApplyEquipSlotNames();
}

void ULeftSkinWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RebuildSkinEquipSlotList();
	RebuildEquipSlotNameList();
	ApplyEquipSlotNames();
	BindSkinEquipSlotCallbacks();
}

void ULeftSkinWidget::NativeDestruct()
{
	UnbindSkinEquipSlotCallbacks();

	Super::NativeDestruct();
}

void ULeftSkinWidget::InitialzeEquipSlots()
{
	ToggleActiveSkinEquipSlots(true);
	bIsSelectedAnySlot = false;
}

void ULeftSkinWidget::ToggleActiveSkinEquipSlots(bool bActive)
{
	RebuildSkinEquipSlotList();

	for (USkinEquipSlotWidget* SkinEquipSlot : SkinEquipSlotList)
	{
		if (SkinEquipSlot)
		{
			SkinEquipSlot->SetIsEnabled(bActive);
		}
	}
}

void ULeftSkinWidget::SelectSkinEquipSlot(FGameplayTag EquipTypeTag, USkinEquipSlotWidget* InSelectedSkinEquipSlot)
{
	if (!InSelectedSkinEquipSlot)
	{
		return;
	}

	SelectedSkinEquipSlot = InSelectedSkinEquipSlot;

	BroadcastClickedSkinEquipTypeSlot(EquipTypeTag, SelectedSkinEquipSlot, bIsSelectedAnySlot);
	ToggleActiveSkinEquipSlots(bIsSelectedAnySlot);

	SelectedSkinEquipSlot->SetIsEnabled(true);
	bIsSelectedAnySlot = !bIsSelectedAnySlot;
}

void ULeftSkinWidget::BroadcastClickedSkinEquipTypeSlot(FGameplayTag EquipTypeTag, USkinEquipSlotWidget* InSelectedSkinEquipSlot, bool bInIsSelectedAnySlot)
{
	OnClicked_SkinEquipTypeSlot.Broadcast(EquipTypeTag, InSelectedSkinEquipSlot, bInIsSelectedAnySlot);
}

void ULeftSkinWidget::HandleSkinEquipSlotClicked(USkinEquipSlotWidget* SkinEquipSlot)
{
	SelectSkinEquipSlot(ResolveSkinEquipTypeTagForSlot(SkinEquipSlot), SkinEquipSlot);
}

void ULeftSkinWidget::RebuildSkinEquipSlotList()
{
	SkinEquipSlotList.Reset();
	SkinEquipSlotList.Reserve(13);

	SkinEquipSlotList.Add(HatSlot);
	SkinEquipSlotList.Add(TopSlot);
	SkinEquipSlotList.Add(BottomSlot);
	SkinEquipSlotList.Add(ShoesSlot);
	SkinEquipSlotList.Add(HairSlot);
	SkinEquipSlotList.Add(FaceSlot);
	SkinEquipSlotList.Add(BackSlot);
	SkinEquipSlotList.Add(AuraSlot);
	SkinEquipSlotList.Add(GestureSlot1);
	SkinEquipSlotList.Add(GestureSlot2);
	SkinEquipSlotList.Add(GestureSlot3);
	SkinEquipSlotList.Add(GestureSlot4);
	SkinEquipSlotList.Add(RidingSlot);
}

void ULeftSkinWidget::RebuildEquipSlotNameList()
{
	EquipSlotNameList =
		{
			FText::FromString(TEXT("Hat")),
			FText::FromString(TEXT("Top")),
			FText::FromString(TEXT("Bottom")),
			FText::FromString(TEXT("Shoes")),
			FText::FromString(TEXT("Hair")),
			FText::FromString(TEXT("Face")),
			FText::FromString(TEXT("Back")),
			FText::FromString(TEXT("Aura")),
			FText::FromString(TEXT("1")),
			FText::FromString(TEXT("2")),
			FText::FromString(TEXT("3")),
			FText::FromString(TEXT("4")),
			FText::FromString(TEXT("Riding")),
		};
}

void ULeftSkinWidget::ApplyEquipSlotNames()
{
	const int32 SlotCount = FMath::Min(SkinEquipSlotList.Num(), EquipSlotNameList.Num());

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		if (SkinEquipSlotList[SlotIndex])
		{
			SkinEquipSlotList[SlotIndex]->SetText(EquipSlotNameList[SlotIndex]);
		}
	}
}

void ULeftSkinWidget::BindSkinEquipSlotCallbacks()
{
	for (USkinEquipSlotWidget* SkinEquipSlot : SkinEquipSlotList)
	{
		if (SkinEquipSlot)
		{
			SkinEquipSlot->OnClicked_SkinEquipSlot.AddUniqueDynamic(this, &ThisClass::HandleSkinEquipSlotClicked);
		}
	}
}

void ULeftSkinWidget::UnbindSkinEquipSlotCallbacks()
{
	for (USkinEquipSlotWidget* SkinEquipSlot : SkinEquipSlotList)
	{
		if (SkinEquipSlot)
		{
			SkinEquipSlot->OnClicked_SkinEquipSlot.RemoveDynamic(this, &ThisClass::HandleSkinEquipSlotClicked);
		}
	}
}

FGameplayTag ULeftSkinWidget::ResolveSkinEquipTypeTagForSlot(const USkinEquipSlotWidget* SkinEquipSlot) const
{
	if (SkinEquipSlot == HatSlot)
	{
		return UProjectTagConfig::Get(this)->GetSkinHatEquipTypeTag();
	}

	if (SkinEquipSlot == TopSlot)
	{
		return UProjectTagConfig::Get(this)->GetSkinTopEquipTypeTag();
	}

	if (SkinEquipSlot == BottomSlot)
	{
		return UProjectTagConfig::Get(this)->GetSkinBottomEquipTypeTag();
	}

	if (SkinEquipSlot == ShoesSlot)
	{
		return UProjectTagConfig::Get(this)->GetSkinShoesEquipTypeTag();
	}

	if (SkinEquipSlot == HairSlot)
	{
		return UProjectTagConfig::Get(this)->GetSkinHairEquipTypeTag();
	}

	if (SkinEquipSlot == FaceSlot)
	{
		return UProjectTagConfig::Get(this)->GetSkinFaceEquipTypeTag();
	}

	if (SkinEquipSlot == BackSlot)
	{
		return UProjectTagConfig::Get(this)->GetSkinBackEquipTypeTag();
	}

	if (SkinEquipSlot == AuraSlot)
	{
		return UProjectTagConfig::Get(this)->GetSkinAuraEquipTypeTag();
	}

	if (SkinEquipSlot == GestureSlot1 || SkinEquipSlot == GestureSlot2 || SkinEquipSlot == GestureSlot3 || SkinEquipSlot == GestureSlot4)
	{
		return UProjectTagConfig::Get(this)->GetSkinGestureTypeTag();
	}

	if (SkinEquipSlot == RidingSlot)
	{
		return UProjectTagConfig::Get(this)->GetSkinRidingTypeTag();
	}

	return FGameplayTag();
}
