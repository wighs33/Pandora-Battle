#include "UI/Widget/LeftSkinWidget.h"

#include "Character/PdPlayer.h"
#include "Common/LabGameplayTags.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Components/Button.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdHUD.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Component/Skin/SkinEquipmentComponent.h"
#include "Skin/SkinInstance.h"
#include "Definition/UI/WidgetClassDefinition.h"

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

	if (DrawButton)
	{
		DrawButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDrawButtonClicked);
	}
}

void ULeftSkinWidget::NativeDestruct()
{
	if (DrawButton)
	{
		DrawButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleDrawButtonClicked);
	}

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

void ULeftSkinWidget::SelectSkinEquipSlot(FGameplayTag EquipTypeTag, USkinEquipSlotWidget* InSelectedSkinEquipSlot)
{
	if (!InSelectedSkinEquipSlot)
	{
		return;
	}

	SelectedSkinEquipSlot = InSelectedSkinEquipSlot;


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
		const FGameplayTag RequiredSkinTag = SlotTag.MatchesTag(LabGameplayTags::Skin_Gesture)
			? LabGameplayTags::Skin_Gesture
			: SlotTag;
		if (!RequiredSkinTag.IsValid() || !SkinDefinition->IdTag.MatchesTag(RequiredSkinTag))
		{
			continue;
		}

		if (!FirstCompatibleSlot)
		{
			FirstCompatibleSlot = SkinEquipSlot;
		}

		if (!SkinEquipSlot->HasEquippedSkin())
		{

			return SkinEquipSlot;
		}
	}
	return FirstCompatibleSlot;
}

void ULeftSkinWidget::BroadcastClickedSkinEquipTypeSlot(FGameplayTag EquipTypeTag, USkinEquipSlotWidget* InSelectedSkinEquipSlot, bool bInIsSelectedAnySlot)
{

	OnClicked_SkinEquipTypeSlot.Broadcast(EquipTypeTag, InSelectedSkinEquipSlot, bInIsSelectedAnySlot);
}

void ULeftSkinWidget::HidePaintCanvasGroup()
{
	if (APdPlayer* PlayerCharacter = GetOwningPdPlayer())
	{
		PlayerCharacter->HidePaintCanvas();
	}

	BroadcastPaintCanvasGroupVisibilityChanged(false);
}

void ULeftSkinWidget::HandleSkinEquipSlotClicked(USkinEquipSlotWidget* SkinEquipSlot)
{
	const FGameplayTag ResolvedTag = ResolveSkinEquipTypeTagForSlot(SkinEquipSlot);

	SelectSkinEquipSlot(ResolvedTag, SkinEquipSlot);
}

void ULeftSkinWidget::RebuildSkinEquipSlotList()
{
	SkinEquipSlotList.Reset();
	SkinEquipSlotList.Reserve(14);

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
	SkinEquipSlotList.Add(PetSlot);
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
			FText::FromString(TEXT("Pet")),
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

	OnDroppedSkin_SkinEquipTypeSlot.Broadcast(ResolvedTag, SkinEquipSlot, SkinInstance);
}

void ULeftSkinWidget::HandleDrawButtonClicked()
{
	APlayerController* PlayerController = GetOwningPlayer();
	APdPlayer* PlayerCharacter = GetOwningPdPlayer();
	if (!PlayerCharacter)
	{
		BroadcastPaintCanvasGroupVisibilityChanged(false);

		return;
	}

	if (PlayerCharacter->HasActivePaintCanvas())
	{
		HidePaintCanvasGroup();
		return;
	}

	FTransform PaintCanvasTransformOffset = FSkinWidgetSettings().PaintCanvasTransformOffset;
	const APdHUD* PdHUD = PlayerController ? Cast<APdHUD>(PlayerController->GetHUD()) : nullptr;
	if (const UWidgetClassDefinition* WidgetDefinition = PdHUD ? PdHUD->GetWidgetClassDefinition() : nullptr)
	{
		PaintCanvasTransformOffset = WidgetDefinition->GetSkinWidgetSettings().PaintCanvasTransformOffset;
	}

	AActor* PaintCanvasActor = PlayerCharacter->ShowPaintCanvasWithCharacterOffset(PaintCanvasTransformOffset);
	BroadcastPaintCanvasGroupVisibilityChanged(PaintCanvasActor != nullptr && PlayerCharacter->HasActivePaintCanvas());

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
	if (SkinEquipSlot == GestureSlot1)
	{
		return LabGameplayTags::Skin_Gesture_Slot1;
	}

	if (SkinEquipSlot == GestureSlot2)
	{
		return LabGameplayTags::Skin_Gesture_Slot2;
	}

	if (SkinEquipSlot == GestureSlot3)
	{
		return LabGameplayTags::Skin_Gesture_Slot3;
	}

	if (SkinEquipSlot == GestureSlot4)
	{
		return LabGameplayTags::Skin_Gesture_Slot4;
	}

	if (SkinEquipSlot && SkinEquipSlot->GetEquipTypeTag().IsValid())
	{

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

	if (SkinEquipSlot == RidingSlot)
	{
		return UProjectTagConfig::Get(this)->GetSkinRidingTypeTag();
	}

	if (SkinEquipSlot == PetSlot)
	{
		return UProjectTagConfig::Get(this)->GetSkinPetTypeTag();
	}

	return FGameplayTag();
}

void ULeftSkinWidget::BroadcastPaintCanvasGroupVisibilityChanged(const bool bVisible)
{
	OnPaintCanvasGroupVisibilityChanged.Broadcast(bVisible);
}

APdPlayer* ULeftSkinWidget::GetOwningPdPlayer() const
{
	const APlayerController* PlayerController = GetOwningPlayer();
	return PlayerController ? Cast<APdPlayer>(PlayerController->GetPawn()) : nullptr;
}
