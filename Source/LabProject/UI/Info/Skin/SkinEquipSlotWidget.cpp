#include "UI/Info/Skin/SkinEquipSlotWidget.h"

#include "Common/LabGameplayTags.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Localization/MenuLocalizationSubsystem.h"
#include "UI/Info/InfoWidget.h"
#include "UI/Info/Skin/SkinSlotDragDropOperation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinEquipSlotWidget)

void USkinEquipSlotWidget::BroadcastClickedSkinEquipSlot(USkinEquipSlotWidget* SkinEquipSlot)
{
	OnClicked_SkinEquipSlot.Broadcast(SkinEquipSlot ? SkinEquipSlot : this);
}

void USkinEquipSlotWidget::SetSkinDefinition(const USkinDefinition* Target)
{
	SkinDefinition = Target;

	ResetSlotIcons();
	if (!SkinDefinition)
	{
		SlotText = FText::GetEmpty();
		CurrentSkinIconTexture = nullptr;

		ApplySlotVisual();
		return;
	}

	SlotText = GetLocalization() ? GetLocalization()->GetProductText(SkinDefinition, TEXT("Name"), SkinDefinition->DisplayName) : SkinDefinition->DisplayName;
	CurrentSkinIconTexture = SkinDefinition->IconTexture;
	ApplySlotVisual();
}

void USkinEquipSlotWidget::OnMenuLanguageChanged()
{
	Super::OnMenuLanguageChanged();
	if (SkinDefinition)
	{
		SlotText = GetLocalization() ? GetLocalization()->GetProductText(SkinDefinition, TEXT("Name"), SkinDefinition->DisplayName) : SkinDefinition->DisplayName;
		ApplySlotVisual();
	}
}

FGameplayTag USkinEquipSlotWidget::GetAcceptedEquipTypeTag() const
{
	if (ResolvedEquipTypeTag.MatchesTag(LabGameplayTags::Skin_Gesture))
	{
		return ResolvedEquipTypeTag;
	}

	return Super::GetAcceptedEquipTypeTag();
}

bool USkinEquipSlotWidget::ShowSlotDetail(UInfoWidget& InfoWidget)
{
	if (!SkinDefinition)
	{
		return false;
	}

	InfoWidget.ShowSkinDefinitionDetailAtWidget(SkinDefinition, this, false);
	return true;
}

bool USkinEquipSlotWidget::CanAcceptDragOperation(UDragDropOperation* Operation) const
{
	const USkinSlotDragDropOperation* SkinDragOperation = Cast<USkinSlotDragDropOperation>(Operation);
	return SkinDragOperation && CanAcceptDroppedSkin(SkinDragOperation->GetSkinDefinition());
}

void USkinEquipSlotWidget::BroadcastAcceptedDrop(UDragDropOperation* Operation)
{
	OnDroppedSkin_SkinEquipSlot.Broadcast(this, CastChecked<USkinSlotDragDropOperation>(Operation)->GetSkinDefinition());
}

void USkinEquipSlotWidget::BroadcastSlotClicked()
{
	BroadcastClickedSkinEquipSlot(this);
}

void USkinEquipSlotWidget::ApplySlotVisual()
{
	ApplyButtonBackgroundStyle();

	UTexture2D* DisplayIconTexture = GetDisplayIconTexture();
	const bool bHasIcon = DisplayIconTexture != nullptr;
	const bool bHasEquippedSkin = SkinDefinition != nullptr;
	const bool bHasSkinIcon = CurrentSkinIconTexture != nullptr;

	UImage* EquippedSkinImage = bHasEquippedSkin ? (SkinImage ? SkinImage.Get() : IconImage.Get()) : nullptr;

	if (AssignedBadgeRoot)
	{
		AssignedBadgeRoot->SetVisibility(bHasEquippedSkin ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (bHasEquippedSkin)
	{
		if (EquippedSkinImage)
		{
			EquippedSkinImage->SetBrushFromTexture(CurrentSkinIconTexture, false);
			EquippedSkinImage->SetVisibility(bHasSkinIcon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
			if (IconImage && IconImage.Get() != EquippedSkinImage)
			{
				IconImage->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
	else
	{
		if (SkinImage && SkinImage.Get() != IconImage.Get())
		{
			SkinImage->SetBrushFromTexture(nullptr, false);
			SkinImage->SetVisibility(ESlateVisibility::Collapsed);
		}

		if (IconImage)
		{
			IconImage->SetBrushFromTexture(DisplayIconTexture, false);
			IconImage->SetVisibility(bHasIcon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
		else if (bHasIcon && ItemButton)
		{
			ItemButton->SetStyle(MakeSlotIconButtonStyle(ItemButton->GetStyle(), 1.0f));
		}
		else if (ItemButton && bHasDefaultButtonStyle)
		{
			ItemButton->SetStyle(DefaultButtonStyle);
		}
	}

	if (SkinImage && (!EquippedSkinImage || SkinImage.Get() != EquippedSkinImage))
	{
		SkinImage->SetVisibility(ESlateVisibility::Collapsed);
	}

	ApplySlotText(bHasIcon || bHasSkinIcon);
	ApplySelectionBorder();
}

void USkinEquipSlotWidget::ApplyButtonBackgroundStyle()
{
	if (!ItemButton)
	{
		return;
	}
	// 장착한 스킨에도 작가가 만든 프레임과 상호작용 상태를 그대로 둔다.
	if (bHasDefaultButtonStyle && DefaultButtonStyle.Normal.GetResourceObject())
	{
		ItemButton->SetStyle(DefaultButtonStyle);
		return;
	}

	if (!SkinDefinition)
	{
		if (bHasDefaultButtonStyle)
		{
			ItemButton->SetStyle(DefaultButtonStyle);
		}
		return;
	}

	const FLinearColor NormalColor = (bIsButtonHovered || bIsAcceptedDragHovered) ? ButtonHoverColor : ButtonNormalColor;
	FButtonStyle ButtonStyle = bHasDefaultButtonStyle ? DefaultButtonStyle : ItemButton->GetStyle();
	ButtonStyle.SetNormal(MakeSolidBrush(ButtonStyle.Normal, NormalColor));
	ButtonStyle.SetHovered(MakeSolidBrush(ButtonStyle.Hovered, ButtonHoverColor));
	ButtonStyle.SetPressed(MakeSolidBrush(ButtonStyle.Pressed, ButtonPressedColor));
	ButtonStyle.SetDisabled(MakeSolidBrush(ButtonStyle.Disabled, FLinearColor(ButtonNormalColor.R, ButtonNormalColor.G, ButtonNormalColor.B, 0.35f)));
	ItemButton->SetStyle(ButtonStyle);
}

bool USkinEquipSlotWidget::CanAcceptDroppedSkin(const USkinDefinition* DroppedSkin) const
{
	const FGameplayTag AcceptedTag = GetAcceptedEquipTypeTag();
	const FGameplayTag RequiredSkinTag = AcceptedTag.MatchesTag(LabGameplayTags::Skin_Gesture)
		? LabGameplayTags::Skin_Gesture
		: AcceptedTag;
	return IsValid(DroppedSkin) && DroppedSkin->IdTag.IsValid() && RequiredSkinTag.IsValid() && DroppedSkin->IdTag.MatchesTag(RequiredSkinTag);
}
