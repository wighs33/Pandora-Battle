#include "UI/Info/Item/EquipSlotWidget.h"

#include "Definition/Common/ProjectTagDefinition.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Definition/Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Localization/MenuLocalizationSubsystem.h"
#include "UI/Info/InfoWidget.h"
#include "UI/Info/Item/ItemSlotDragDropOperation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipSlotWidget)

namespace
{
FLinearColor ApplyBackgroundOpacity(const FLinearColor& Color, const float Opacity)
{
	FLinearColor Result = Color;
	Result.A *= FMath::Clamp(Opacity, 0.0f, 1.0f);
	return Result;
}

void ApplyBrushOpacity(FSlateBrush& Brush, const float Opacity)
{
	FLinearColor TintColor = Brush.TintColor.GetSpecifiedColor();
	TintColor.A *= FMath::Clamp(Opacity, 0.0f, 1.0f);
	Brush.TintColor = FSlateColor(TintColor);
}

FButtonStyle ApplyButtonStyleBackgroundOpacity(const FButtonStyle& SourceStyle, const float Opacity)
{
	FButtonStyle ButtonStyle = SourceStyle;
	ApplyBrushOpacity(ButtonStyle.Normal, Opacity);
	ApplyBrushOpacity(ButtonStyle.Hovered, Opacity);
	ApplyBrushOpacity(ButtonStyle.Pressed, Opacity);
	ApplyBrushOpacity(ButtonStyle.Disabled, Opacity);
	return ButtonStyle;
}
}

void UEquipSlotWidget::BroadcastClickedEquipSlot(UEquipSlotWidget* ItemSlot)
{
	OnClicked_EquipSlot.Broadcast(ItemSlot ? ItemSlot : this);
}

void UEquipSlotWidget::SetPandoraWeaponRequirementIcon(
	UTexture2D* InIconTexture,
	const float InOpacity)
{
	PandoraWeaponRequirementIconTexture = InIconTexture;
	PandoraWeaponRequirementIconOpacity = FMath::Clamp(InOpacity, 0.0f, 1.0f);
	ApplySlotVisual();
}

void UEquipSlotWidget::SetData(UItemInstance* Target)
{
	ItemInstance = Target;
	const UItemDefinition* ItemDefinition = ItemInstance ? ItemInstance->ItemDefinition.Get() : nullptr;

	ResetSlotIcons();
	if (!ItemInstance || !ItemDefinition)
	{
		SlotText = FText::GetEmpty();
		CurrentItemIconTexture = nullptr;

		ApplySlotVisual();
		return;
	}

	SlotText = GetLocalization() ? GetLocalization()->GetProductText(ItemDefinition, TEXT("Name"), ItemDefinition->DisplayName) : ItemDefinition->DisplayName;
	CurrentItemIconTexture = ItemDefinition->IconTexture.Get();
	ApplySlotVisual();
}

void UEquipSlotWidget::OnMenuLanguageChanged()
{
	Super::OnMenuLanguageChanged();
	if (const UItemDefinition* Definition = ItemInstance ? ItemInstance->ItemDefinition.Get() : nullptr)
	{
		SlotText = GetLocalization() ? GetLocalization()->GetProductText(Definition, TEXT("Name"), Definition->DisplayName) : Definition->DisplayName;
		ApplySlotVisual();
	}
}

bool UEquipSlotWidget::ShowSlotDetail(UInfoWidget& InfoWidget)
{
	if (!ItemInstance)
	{
		return false;
	}

	InfoWidget.ShowItemDetailAtWidget(ItemInstance, this, false);
	return true;
}

bool UEquipSlotWidget::CanAcceptDragOperation(UDragDropOperation* Operation) const
{
	const UItemSlotDragDropOperation* ItemDragOperation = Cast<UItemSlotDragDropOperation>(Operation);
	return ItemDragOperation && CanAcceptDroppedItem(ItemDragOperation->GetItemInstance());
}

void UEquipSlotWidget::BroadcastAcceptedDrop(UDragDropOperation* Operation)
{
	OnDroppedItem_EquipSlot.Broadcast(this, CastChecked<UItemSlotDragDropOperation>(Operation)->GetItemInstance());
}

void UEquipSlotWidget::BroadcastSlotClicked()
{
	BroadcastClickedEquipSlot(this);
}

void UEquipSlotWidget::ApplySlotVisual()
{
	ApplyButtonBackgroundStyle();

	UTexture2D* DisplayIconTexture = GetDisplayIconTexture();
	const bool bHasSlotIcon = DisplayIconTexture != nullptr;
	const bool bHasItemIcon = CurrentItemIconTexture != nullptr;
	const float SlotIconOpacity =
		PandoraWeaponRequirementIconTexture && DisplayIconTexture == PandoraWeaponRequirementIconTexture
			? PandoraWeaponRequirementIconOpacity
			: 1.0f;

	if (IconImage)
	{
		IconImage->SetBrushFromTexture(DisplayIconTexture, false);
		IconImage->SetRenderOpacity(SlotIconOpacity);
		IconImage->SetVisibility(bHasSlotIcon && !bHasItemIcon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	else if (bHasSlotIcon && ItemButton && !ItemInstance)
	{
		ItemButton->SetStyle(MakeSlotIconButtonStyle(bHasDefaultButtonStyle ? DefaultButtonStyle : ItemButton->GetStyle(), SlotIconOpacity));
	}

	if (ItemImage)
	{
		ItemImage->SetBrushFromTexture(CurrentItemIconTexture, false);
		ItemImage->SetVisibility(bHasItemIcon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	ApplySlotText(bHasSlotIcon || bHasItemIcon);

	if (QuantityTextBlock)
	{
		const int32 Quantity = ItemInstance ? ItemInstance->Quantity : 0;
		const bool bShowQuantity = Quantity > 0 && bHasItemIcon && IsCurrentItemConsumable();
		QuantityTextBlock->SetText(FText::AsNumber(FMath::Max(0, Quantity)));
		QuantityTextBlock->SetVisibility(bShowQuantity ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	ApplySelectionBorder();
}

void UEquipSlotWidget::ApplyButtonBackgroundStyle()
{
	if (!ItemButton)
	{
		return;
	}
	// 텍스처 프레임은 마우스 올림·누름 표현을 스스로 가진다. 아이템을 장착해도
	// 작가가 만든 장식을 단색 사각형으로 바꾸지 않는다.
	if (bHasDefaultButtonStyle && DefaultButtonStyle.Normal.GetResourceObject())
	{
		ItemButton->SetStyle(ApplyButtonStyleBackgroundOpacity(DefaultButtonStyle, ButtonBackgroundOpacity));
		return;
	}

	if (!ItemInstance)
	{
		const FButtonStyle ButtonStyle = bHasDefaultButtonStyle ? DefaultButtonStyle : ItemButton->GetStyle();
		ItemButton->SetStyle(ApplyButtonStyleBackgroundOpacity(ButtonStyle, ButtonBackgroundOpacity));
		return;
	}

	FButtonStyle ButtonStyle = bHasDefaultButtonStyle ? DefaultButtonStyle : ItemButton->GetStyle();
	ButtonStyle.SetNormal(MakeSolidBrush(ButtonStyle.Normal, ApplyBackgroundOpacity(ButtonNormalColor, ButtonBackgroundOpacity)));
	ButtonStyle.SetHovered(MakeSolidBrush(ButtonStyle.Hovered, ApplyBackgroundOpacity(ButtonHoverColor, ButtonBackgroundOpacity)));
	ButtonStyle.SetPressed(MakeSolidBrush(ButtonStyle.Pressed, ApplyBackgroundOpacity(ButtonPressedColor, ButtonBackgroundOpacity)));
	ButtonStyle.SetDisabled(MakeSolidBrush(ButtonStyle.Disabled, ApplyBackgroundOpacity(FLinearColor(ButtonNormalColor.R, ButtonNormalColor.G, ButtonNormalColor.B, 0.35f), ButtonBackgroundOpacity)));
	ItemButton->SetStyle(ButtonStyle);
}

bool UEquipSlotWidget::CanAcceptDroppedItem(const UItemInstance* DroppedItem) const
{
	const UItemDefinition* ItemDefinition = DroppedItem ? DroppedItem->ItemDefinition.Get() : nullptr;
	const FGameplayTag AcceptedTag = GetAcceptedEquipTypeTag();
	return ItemDefinition && ItemDefinition->IdTag.IsValid() && AcceptedTag.IsValid() && ItemDefinition->IdTag.MatchesTag(AcceptedTag);
}

bool UEquipSlotWidget::IsCurrentItemConsumable() const
{
	const UItemDefinition* ItemDefinition = ItemInstance ? ItemInstance->ItemDefinition.Get() : nullptr;
	const FGameplayTag ConsumableTypeTag = UProjectTagDefinition::Get(this)->GetItemConsumableTypeTag();
	return ItemDefinition
		&& ItemDefinition->IsConsumableDefinition(ConsumableTypeTag);
}

UTexture2D* UEquipSlotWidget::GetCurrentIconTexture(const bool bForHover) const
{
	if (PandoraWeaponRequirementIconTexture)
	{
		return PandoraWeaponRequirementIconTexture.Get();
	}

	return Super::GetCurrentIconTexture(bForHover);
}
