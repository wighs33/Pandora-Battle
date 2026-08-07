#include "UI/Widget/EquipSlotWidget.h"

#include "Definition/Common/ProjectTagConfig.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Definition/Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Mode/PdHUD.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/ItemSlotDragDropOperation.h"
#include "UI/WidgetLookup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipSlotWidget)

namespace
{
FSlateBrush MakeSolidBrush(const FSlateBrush& SourceBrush, const FLinearColor& TintColor)
{
	FSlateBrush Brush = SourceBrush;
	Brush.SetResourceObject(nullptr);
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.TintColor = FSlateColor(TintColor);
	return Brush;
}

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

FSlateBrush MakeSlotIconBrush(const FSlateBrush& SourceBrush, UTexture2D* IconTexture, const FLinearColor& TintColor)
{
	FSlateBrush Brush = SourceBrush;
	Brush.SetResourceObject(IconTexture);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.TintColor = FSlateColor(TintColor);

	if (IconTexture)
	{
		Brush.ImageSize = FVector2D(IconTexture->GetSurfaceWidth(), IconTexture->GetSurfaceHeight());
	}

	return Brush;
}

const TCHAR* VisibilityToString(const ESlateVisibility Visibility)
{
	switch (Visibility)
	{
	case ESlateVisibility::Visible:
		return TEXT("Visible");
	case ESlateVisibility::Collapsed:
		return TEXT("Collapsed");
	case ESlateVisibility::Hidden:
		return TEXT("Hidden");
	case ESlateVisibility::HitTestInvisible:
		return TEXT("HitTestInvisible");
	case ESlateVisibility::SelfHitTestInvisible:
		return TEXT("SelfHitTestInvisible");
	default:
		return TEXT("Unknown");
	}
}

UInfoWidget* ResolveInfoWidgetFromEquipSlot(const UUserWidget* Widget)
{
	const APlayerController* PlayerController = Widget ? Widget->GetOwningPlayer() : nullptr;
	const APdHUD* Hud = PlayerController ? PlayerController->GetHUD<APdHUD>() : nullptr;
	return Hud ? Hud->GetInfoWidget() : nullptr;
}
}

void UEquipSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ItemButton)
	{
		CacheDefaultButtonStyle();
		ItemButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleButtonClicked);
		ItemButton->OnHovered.AddUniqueDynamic(this, &ThisClass::HandleButtonHovered);
		ItemButton->OnUnhovered.AddUniqueDynamic(this, &ThisClass::HandleButtonUnhovered);
	}
}

void UEquipSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplySlotVisual();
}

void UEquipSlotWidget::NativeDestruct()
{
	if (ItemButton)
	{
		ItemButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleButtonClicked);
		ItemButton->OnHovered.RemoveDynamic(this, &ThisClass::HandleButtonHovered);
		ItemButton->OnUnhovered.RemoveDynamic(this, &ThisClass::HandleButtonUnhovered);
	}

	Super::NativeDestruct();
}

void UEquipSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	if (UInfoWidget* InfoWidget = ResolveInfoWidgetFromEquipSlot(this))
	{
		if (ItemInstance)
		{
			InfoWidget->ShowItemDetailAtWidget(ItemInstance, this, false);
		}
		else
		{
			InfoWidget->HideDetailWidgets();
		}
	}
}

void UEquipSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	if (UInfoWidget* InfoWidget = ResolveInfoWidgetFromEquipSlot(this))
	{
		InfoWidget->HideDetailWidgets();
	}

	Super::NativeOnMouseLeave(InMouseEvent);
}

void UEquipSlotWidget::BroadcastClickedEquipSlot(UEquipSlotWidget* ItemSlot)
{

	OnClicked_EquipSlot.Broadcast(ItemSlot ? ItemSlot : this);
}

bool UEquipSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	bIsAcceptedDragHovered = false;

	if (UItemSlotDragDropOperation* ItemDragOperation = Cast<UItemSlotDragDropOperation>(InOperation))
	{
		UItemInstance* DroppedItem = ItemDragOperation->GetItemInstance();
		if (CanAcceptDroppedItem(DroppedItem))
		{

			OnDroppedItem_EquipSlot.Broadcast(this, DroppedItem);
			ApplySlotVisual();
			return true;
		}

}

	ApplySlotVisual();
	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UEquipSlotWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	bIsAcceptedDragHovered = false;
	if (UItemSlotDragDropOperation* ItemDragOperation = Cast<UItemSlotDragDropOperation>(InOperation))
	{
		bIsAcceptedDragHovered = CanAcceptDroppedItem(ItemDragOperation->GetItemInstance());
	}

	ApplySlotVisual();
}

void UEquipSlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);

	bIsAcceptedDragHovered = false;
	ApplySlotVisual();
}

void UEquipSlotWidget::SetText(const FText& InText)
{
	SlotText = InText;
	ApplySlotVisual();
}

void UEquipSlotWidget::SetIcon(UTexture2D* InIconTexture)
{
	SlotIconTexture = InIconTexture;

	if (!ItemInstance)
	{
		CurrentIconTexture = SlotIconTexture;
		CurrentHoverIconTexture = SlotHoverIconTexture ? SlotHoverIconTexture.Get() : CurrentIconTexture.Get();
	}

	ApplySlotVisual();
}

void UEquipSlotWidget::SetHoverIcon(UTexture2D* InIconTexture)
{
	SlotHoverIconTexture = InIconTexture;

	if (!ItemInstance)
	{
		CurrentHoverIconTexture = SlotHoverIconTexture ? SlotHoverIconTexture.Get() : CurrentIconTexture.Get();
	}

	ApplySlotVisual();
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

	if (!ItemInstance || !ItemDefinition)
	{
		bUseSelectedEmptyIcon = false;
		SlotText = FText::GetEmpty();
		CurrentIconTexture = SlotIconTexture;
		CurrentHoverIconTexture = SlotHoverIconTexture ? SlotHoverIconTexture.Get() : CurrentIconTexture.Get();
		CurrentItemIconTexture = nullptr;

		ApplySlotVisual();
		return;
	}

	bUseSelectedEmptyIcon = false;
	SlotText = ItemDefinition->DisplayName;
	CurrentIconTexture = SlotIconTexture;
	CurrentHoverIconTexture = SlotHoverIconTexture ? SlotHoverIconTexture.Get() : CurrentIconTexture.Get();
	CurrentItemIconTexture = ItemDefinition->IconTexture.Get();
	ApplySlotVisual();
}

void UEquipSlotWidget::SetResolvedEquipTypeTag(const FGameplayTag InResolvedEquipTypeTag)
{
	ResolvedEquipTypeTag = InResolvedEquipTypeTag;
}

FGameplayTag UEquipSlotWidget::GetAcceptedEquipTypeTag() const
{
	return EquipTypeTag.IsValid() ? EquipTypeTag : ResolvedEquipTypeTag;
}

void UEquipSlotWidget::SetSelected(const bool bInSelected)
{
	if (bIsSelected == bInSelected)
	{
		return;
	}

	bIsSelected = bInSelected;
	ApplySlotVisual();
}

void UEquipSlotWidget::HandleButtonClicked()
{

	BroadcastClickedEquipSlot(this);
}

void UEquipSlotWidget::HandleButtonHovered()
{
	bIsButtonHovered = true;
	ApplySlotVisual();
}

void UEquipSlotWidget::HandleButtonUnhovered()
{
	bIsButtonHovered = false;
	ApplySlotVisual();
}

void UEquipSlotWidget::ApplySlotVisual()
{
	CacheOptionalWidgets();
	ApplyButtonBackgroundStyle();

	UTexture2D* NormalIconTexture = GetCurrentIconTexture(false);
	UTexture2D* HoverIconTexture = GetCurrentIconTexture(true);
	UTexture2D* DisplayIconTexture = (bIsButtonHovered || bUseSelectedEmptyIcon || bIsAcceptedDragHovered) ? HoverIconTexture : NormalIconTexture;
	const bool bHasSlotIcon = DisplayIconTexture != nullptr;
	const bool bHasItemIcon = CurrentItemIconTexture != nullptr;
	const bool bShowingPandoraWeaponRequirement =
		PandoraWeaponRequirementIconTexture
		&& DisplayIconTexture == PandoraWeaponRequirementIconTexture;

	if (IconImage)
	{

		IconImage->SetBrushFromTexture(DisplayIconTexture, false);
		IconImage->SetRenderOpacity(
			bShowingPandoraWeaponRequirement
				? PandoraWeaponRequirementIconOpacity
				: 1.0f);
		IconImage->SetVisibility(bHasSlotIcon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	}
	else if (bHasSlotIcon && ItemButton && !ItemInstance)
	{
		UTexture2D* RestIconTexture = (bUseSelectedEmptyIcon || bIsAcceptedDragHovered) ? HoverIconTexture : NormalIconTexture;
		const float SlotIconOpacity =
			bShowingPandoraWeaponRequirement
				? PandoraWeaponRequirementIconOpacity
				: 1.0f;
		FButtonStyle ButtonStyle = bHasDefaultButtonStyle ? DefaultButtonStyle : ItemButton->GetStyle();
		ButtonStyle.SetNormal(MakeSlotIconBrush(ButtonStyle.Normal, RestIconTexture, FLinearColor(0.9f, 0.9f, 0.9f, SlotIconOpacity)));
		ButtonStyle.SetHovered(MakeSlotIconBrush(ButtonStyle.Hovered, HoverIconTexture, FLinearColor(1.0f, 1.0f, 1.0f, SlotIconOpacity)));
		ButtonStyle.SetPressed(MakeSlotIconBrush(ButtonStyle.Pressed, HoverIconTexture, FLinearColor(0.75f, 0.75f, 0.75f, SlotIconOpacity)));
		ButtonStyle.SetDisabled(MakeSlotIconBrush(ButtonStyle.Disabled, RestIconTexture, FLinearColor(0.35f, 0.35f, 0.35f, SlotIconOpacity)));
		ItemButton->SetStyle(ButtonStyle);

	}

	if (ItemImage)
	{
		ItemImage->SetBrushFromTexture(CurrentItemIconTexture, false);
		ItemImage->SetVisibility(bHasItemIcon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	}

	if (ApplyText)
	{
		ApplyText->SetText(SlotText);
		ApplyText->SetVisibility((bHasSlotIcon || bHasItemIcon) ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	if (QuantityTextBlock)
	{
		const int32 Quantity = ItemInstance ? ItemInstance->Quantity : 0;
		const bool bShowQuantity = Quantity > 0 && bHasItemIcon && IsCurrentItemConsumable();
		QuantityTextBlock->SetText(FText::AsNumber(FMath::Max(0, Quantity)));
		QuantityTextBlock->SetVisibility(bShowQuantity ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (SelectionBorderImage)
	{
		SelectionBorderImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		SelectionBorderImage->SetColorAndOpacity(bIsSelected ? SelectionBorderSelectedColor : SelectionBorderDefaultColor);
	}
}

void UEquipSlotWidget::CacheOptionalWidgets()
{
	if (!ItemImage)
	{
		ItemImage = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("ItemImage"),
			TEXT("EquippedItemImage")
		});
	}

	if (!IconImage)
	{
		IconImage = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("IconImage"),
			TEXT("SlotIconImage"),
			TEXT("EquipIconImage"),
			TEXT("SlotIcon"),
			TEXT("EquipIcon"),
			TEXT("Icon")
		});
	}
	if (!SelectionBorderImage)
	{
		SelectionBorderImage = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("SelectionBorderImage"),
			TEXT("SelectedBorderImage"),
			TEXT("HighlightBorderImage"),
			TEXT("SelectionHighlightImage")
		});
	}
	if (!QuantityTextBlock)
	{
		QuantityTextBlock = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("QuantityTextBlock"),
			TEXT("Txt_Quantity"),
			TEXT("Text_Quantity"),
			TEXT("QuantityText"),
			TEXT("ItemCountText")
		});
	}
}

void UEquipSlotWidget::ApplyButtonBackgroundStyle()
{
	if (!ItemButton)
	{
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

bool UEquipSlotWidget::CanAcceptDroppedItem(UItemInstance* DroppedItem) const
{
	const UItemDefinition* ItemDefinition = DroppedItem ? DroppedItem->ItemDefinition.Get() : nullptr;
	const FGameplayTag AcceptedTag = GetAcceptedEquipTypeTag();
	return ItemDefinition && ItemDefinition->IdTag.IsValid() && AcceptedTag.IsValid() && ItemDefinition->IdTag.MatchesTag(AcceptedTag);
}

bool UEquipSlotWidget::IsCurrentItemConsumable() const
{
	const UItemDefinition* ItemDefinition = ItemInstance ? ItemInstance->ItemDefinition.Get() : nullptr;
	const FGameplayTag ConsumableTypeTag = UProjectTagConfig::Get(this)->GetItemConsumableTypeTag();
	return ItemDefinition
		&& ItemDefinition->IsConsumableDefinition(ConsumableTypeTag);
}

UTexture2D* UEquipSlotWidget::GetCurrentIconTexture(const bool bForHover) const
{
	if (PandoraWeaponRequirementIconTexture)
	{
		return PandoraWeaponRequirementIconTexture.Get();
	}

	if (bForHover)
	{
		if (CurrentHoverIconTexture)
		{
			return CurrentHoverIconTexture.Get();
		}

		if (!ItemInstance && SlotHoverIconTexture)
		{
			return SlotHoverIconTexture.Get();
		}
	}

	return CurrentIconTexture ? CurrentIconTexture.Get() : SlotIconTexture.Get();
}

void UEquipSlotWidget::CacheDefaultButtonStyle()
{
	if (!ItemButton || bHasDefaultButtonStyle)
	{
		return;
	}

	DefaultButtonStyle = ItemButton->GetStyle();
	bHasDefaultButtonStyle = true;
}
