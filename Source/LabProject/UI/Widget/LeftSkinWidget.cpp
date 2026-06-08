#include "UI/Widget/LeftSkinWidget.h"

#include "Common/ProjectTagConfig.h"
#include "Skin/SkinDefinition.h"
#include "Skin/SkinEquipmentComponent.h"
#include "Skin/SkinInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LeftSkinWidget)

DEFINE_LOG_CATEGORY_STATIC(LogLeftSkinWidget, Log, All);

ULeftSkinWidget::ULeftSkinWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULeftSkinWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	RebuildSkinEquipSlotList();
	RebuildEquipSlotNameList();
	ApplyResolvedEquipTypeTags();
	ApplyEquipSlotNames();
}

void ULeftSkinWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RebuildSkinEquipSlotList();
	RebuildEquipSlotNameList();
	ApplyResolvedEquipTypeTags();
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
	for (USkinEquipSlotWidget* SkinEquipSlot : SkinEquipSlotList)
	{
		if (SkinEquipSlot)
		{
			SkinEquipSlot->SetIsEnabled(true);
			SkinEquipSlot->SetSelected(false);
		}
	}
	SelectedSkinEquipSlot = nullptr;
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
	UE_LOG(LogLeftSkinWidget, Log, TEXT("[SkinSlotFlow] SkinEquipSlot clicked for filter/clear: widget=%s slot=%s tag=%s occupied=%s slotOwnTag=%s"),
		*GetNameSafe(this),
		*GetNameSafe(SelectedSkinEquipSlot),
		EquipTypeTag.IsValid() ? *EquipTypeTag.ToString() : TEXT("None"),
		SelectedSkinEquipSlot && SelectedSkinEquipSlot->HasEquippedSkin() ? TEXT("true") : TEXT("false"),
		SelectedSkinEquipSlot->GetEquipTypeTag().IsValid() ? *SelectedSkinEquipSlot->GetEquipTypeTag().ToString() : TEXT("None"));

	BroadcastClickedSkinEquipTypeSlot(EquipTypeTag, SelectedSkinEquipSlot, false);

	for (USkinEquipSlotWidget* SkinEquipSlot : SkinEquipSlotList)
	{
		if (SkinEquipSlot)
		{
			SkinEquipSlot->SetIsEnabled(true);
			SkinEquipSlot->SetSelected(false);
		}
	}

	bIsSelectedAnySlot = false;
}

void ULeftSkinWidget::RefreshEquippedSkinSlots(const USkinEquipmentComponent* SkinEquipmentComponent)
{
	RebuildSkinEquipSlotList();
	UE_LOG(LogLeftSkinWidget, Log, TEXT("[SkinSlotFlow] RefreshEquippedSkinSlots: widget=%s component=%s slotCount=%d"),
		*GetNameSafe(this),
		*GetNameSafe(SkinEquipmentComponent),
		SkinEquipSlotList.Num());

	for (USkinEquipSlotWidget* SkinEquipSlot : SkinEquipSlotList)
	{
		if (!SkinEquipSlot)
		{
			continue;
		}

		const FGameplayTag SlotTag = ResolveSkinEquipTypeTagForSlot(SkinEquipSlot);
		const USkinDefinition* SkinDefinition = SkinEquipmentComponent && SlotTag.IsValid()
			? SkinEquipmentComponent->GetEquippedSkinDefinition(SlotTag)
			: nullptr;
		UE_LOG(LogLeftSkinWidget, Log, TEXT("[SkinSlotFlow] RefreshEquippedSkinSlots apply: slot=%s slotTag=%s definition=%s idTag=%s"),
			*GetNameSafe(SkinEquipSlot),
			SlotTag.IsValid() ? *SlotTag.ToString() : TEXT("None"),
			*GetNameSafe(SkinDefinition),
			SkinDefinition && SkinDefinition->IdTag.IsValid() ? *SkinDefinition->IdTag.ToString() : TEXT("None"));
		SkinEquipSlot->SetSkinDefinition(SkinDefinition);
	}
}

USkinEquipSlotWidget* ULeftSkinWidget::FindFirstCompatibleSkinEquipSlot(USkinInstance* SkinInstance) const
{
	const USkinDefinition* SkinDefinition = SkinInstance ? SkinInstance->SkinDefinition.Get() : nullptr;
	if (!SkinDefinition || !SkinDefinition->IdTag.IsValid())
	{
		return nullptr;
	}

	USkinEquipSlotWidget* FirstCompatibleSlot = nullptr;
	for (USkinEquipSlotWidget* SkinEquipSlot : SkinEquipSlotList)
	{
		if (!SkinEquipSlot)
		{
			continue;
		}

		const FGameplayTag SlotTag = ResolveSkinEquipTypeTagForSlot(SkinEquipSlot);
		if (!SlotTag.IsValid() || !SkinDefinition->IdTag.MatchesTag(SlotTag))
		{
			continue;
		}

		if (!FirstCompatibleSlot)
		{
			FirstCompatibleSlot = SkinEquipSlot;
		}

		if (!SkinEquipSlot->HasEquippedSkin())
		{
			UE_LOG(LogLeftSkinWidget, Log, TEXT("[CharacterPanelDrop] Found empty compatible skin slot: widget=%s slot=%s tag=%s skin=%s idTag=%s"),
				*GetNameSafe(this),
				*GetNameSafe(SkinEquipSlot),
				*SlotTag.ToString(),
				*GetNameSafe(SkinInstance),
				*SkinDefinition->IdTag.ToString());
			return SkinEquipSlot;
		}
	}

	if (FirstCompatibleSlot)
	{
		UE_LOG(LogLeftSkinWidget, Log, TEXT("[CharacterPanelDrop] No empty compatible skin slot, replacing first compatible slot: widget=%s slot=%s skin=%s idTag=%s"),
			*GetNameSafe(this),
			*GetNameSafe(FirstCompatibleSlot),
			*GetNameSafe(SkinInstance),
			*SkinDefinition->IdTag.ToString());
	}
	return FirstCompatibleSlot;
}

void ULeftSkinWidget::BroadcastClickedSkinEquipTypeSlot(FGameplayTag EquipTypeTag, USkinEquipSlotWidget* InSelectedSkinEquipSlot, bool bInIsSelectedAnySlot)
{
	UE_LOG(LogLeftSkinWidget, Log, TEXT("[SkinSlotFlow] Broadcast SkinEquipTypeSlot: widget=%s slot=%s tag=%s selectedAny=%s"),
		*GetNameSafe(this),
		*GetNameSafe(InSelectedSkinEquipSlot),
		EquipTypeTag.IsValid() ? *EquipTypeTag.ToString() : TEXT("None"),
		bInIsSelectedAnySlot ? TEXT("true") : TEXT("false"));
	OnClicked_SkinEquipTypeSlot.Broadcast(EquipTypeTag, InSelectedSkinEquipSlot, bInIsSelectedAnySlot);
}

void ULeftSkinWidget::HandleSkinEquipSlotClicked(USkinEquipSlotWidget* SkinEquipSlot)
{
	const FGameplayTag ResolvedTag = ResolveSkinEquipTypeTagForSlot(SkinEquipSlot);
	UE_LOG(LogLeftSkinWidget, Log, TEXT("[SkinSlotFlow] SkinEquipSlot clicked: widget=%s slot=%s resolvedTag=%s"),
		*GetNameSafe(this),
		*GetNameSafe(SkinEquipSlot),
		ResolvedTag.IsValid() ? *ResolvedTag.ToString() : TEXT("None"));
	SelectSkinEquipSlot(ResolvedTag, SkinEquipSlot);
}

void ULeftSkinWidget::RebuildSkinEquipSlotList()
{
	SkinEquipSlotList.Reset();
	SkinEquipSlotList.Reserve(13);

	SkinEquipSlotList.Add(HatSlot);
	SkinEquipSlotList.Add(TopSlot);
	SkinEquipSlotList.Add(BottomSlot);
	SkinEquipSlotList.Add(ShoesSlot);
	SkinEquipSlotList.Add(HeadSlot);
	SkinEquipSlotList.Add(SkinColorSlot);
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
			FText::FromString(TEXT("Head")),
			FText::FromString(TEXT("Skin Color")),
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
			SkinEquipSlot->OnDroppedSkin_SkinEquipSlot.AddUniqueDynamic(this, &ThisClass::HandleSkinEquipSlotSkinDropped);
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
			SkinEquipSlot->OnDroppedSkin_SkinEquipSlot.RemoveDynamic(this, &ThisClass::HandleSkinEquipSlotSkinDropped);
		}
	}
}

void ULeftSkinWidget::HandleSkinEquipSlotSkinDropped(USkinEquipSlotWidget* SkinEquipSlot, USkinInstance* SkinInstance)
{
	const FGameplayTag ResolvedTag = ResolveSkinEquipTypeTagForSlot(SkinEquipSlot);
	UE_LOG(LogLeftSkinWidget, Log, TEXT("[SkinSlotDragDrop] Skin dropped on left skin slot: widget=%s slot=%s resolvedTag=%s skin=%s definition=%s"),
		*GetNameSafe(this),
		*GetNameSafe(SkinEquipSlot),
		ResolvedTag.IsValid() ? *ResolvedTag.ToString() : TEXT("None"),
		*GetNameSafe(SkinInstance),
		*GetNameSafe(SkinInstance ? SkinInstance->SkinDefinition.Get() : nullptr));
	OnDroppedSkin_SkinEquipTypeSlot.Broadcast(ResolvedTag, SkinEquipSlot, SkinInstance);
}

void ULeftSkinWidget::ApplyResolvedEquipTypeTags()
{
	for (USkinEquipSlotWidget* SkinEquipSlot : SkinEquipSlotList)
	{
		if (SkinEquipSlot)
		{
			SkinEquipSlot->SetResolvedEquipTypeTag(ResolveSkinEquipTypeTagForSlot(SkinEquipSlot));
		}
	}
}

FGameplayTag ULeftSkinWidget::ResolveSkinEquipTypeTagForSlot(const USkinEquipSlotWidget* SkinEquipSlot) const
{
	if (SkinEquipSlot && SkinEquipSlot->GetEquipTypeTag().IsValid())
	{
		UE_LOG(LogLeftSkinWidget, Log, TEXT("[SkinSlotFlow] ResolveSkinEquipTypeTagForSlot using slot override: widget=%s slot=%s tag=%s"),
			*GetNameSafe(this),
			*GetNameSafe(SkinEquipSlot),
			*SkinEquipSlot->GetEquipTypeTag().ToString());
		return SkinEquipSlot->GetEquipTypeTag();
	}

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

	if (SkinEquipSlot == HeadSlot)
	{
		return UProjectTagConfig::Get(this)->GetSkinHeadEquipTypeTag();
	}

	if (SkinEquipSlot == SkinColorSlot)
	{
		return UProjectTagConfig::Get(this)->GetSkinColorEquipTypeTag();
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
