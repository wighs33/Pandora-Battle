#include "UI/Widget/EquipSlotWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Mode/PdHUD.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/ItemSlotDragDropOperation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipSlotWidget)

DEFINE_LOG_CATEGORY_STATIC(LogEquipSlotWidget, Log, All);

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

	UE_LOG(LogEquipSlotWidget, Log, TEXT("[InventoryFilter] EquipSlot NativeConstruct: widget=%s button=%s nth=%d"),
		*GetNameSafe(this),
		*GetNameSafe(ItemButton),
		Nth);

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

	UE_LOG(LogEquipSlotWidget, Log, TEXT("[EquipSlotIconDebug] NativePreConstruct: widget=%s nth=%d slotIcon=%s slotHoverIcon=%s boundIconImage=%s boundItemImage=%s"),
		*GetNameSafe(this),
		Nth,
		*GetNameSafe(SlotIconTexture),
		*GetNameSafe(SlotHoverIconTexture),
		*GetNameSafe(IconImage),
		*GetNameSafe(ItemImage));
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
	UE_LOG(LogEquipSlotWidget, Log, TEXT("[InventoryFilter] EquipSlot broadcast clicked: widget=%s itemSlot=%s nth=%d text=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ItemSlot ? ItemSlot : this),
		(ItemSlot ? ItemSlot : this)->GetNth(),
		*(ItemSlot ? ItemSlot : this)->GetSlotText().ToString());
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
			UE_LOG(LogEquipSlotWidget, Log, TEXT("[EquipSlotDragDrop] Item dropped on equip slot: slot=%s nth=%d item=%s sourceSlot=%d"),
				*GetNameSafe(this),
				Nth,
				*GetNameSafe(DroppedItem),
				ItemDragOperation->GetSourceSlotIndex());
			OnDroppedItem_EquipSlot.Broadcast(this, DroppedItem);
			ApplySlotVisual();
			return true;
		}

		UE_LOG(LogEquipSlotWidget, Warning, TEXT("[EquipSlotDragDrop] Item drop rejected by equip slot type: slot=%s nth=%d acceptedTag=%s item=%s"),
			*GetNameSafe(this),
			Nth,
			GetAcceptedEquipTypeTag().IsValid() ? *GetAcceptedEquipTypeTag().ToString() : TEXT("None"),
			*GetNameSafe(DroppedItem));
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
	UE_LOG(LogEquipSlotWidget, Log, TEXT("[EquipSlotIconDebug] SetIcon: widget=%s nth=%d slotIcon=%s item=%s"),
		*GetNameSafe(this),
		Nth,
		*GetNameSafe(SlotIconTexture),
		*GetNameSafe(ItemInstance));

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
	UE_LOG(LogEquipSlotWidget, Log, TEXT("[EquipSlotIconDebug] SetHoverIcon: widget=%s nth=%d slotHoverIcon=%s item=%s"),
		*GetNameSafe(this),
		Nth,
		*GetNameSafe(SlotHoverIconTexture),
		*GetNameSafe(ItemInstance));

	if (!ItemInstance)
	{
		CurrentHoverIconTexture = SlotHoverIconTexture ? SlotHoverIconTexture.Get() : CurrentIconTexture.Get();
	}

	ApplySlotVisual();
}

void UEquipSlotWidget::SetData(UItemInstance* Target)
{
	ItemInstance = Target;
	const UItemDefinition* ItemDefinition = ItemInstance ? ItemInstance->ItemDefinition.Get() : nullptr;
	UE_LOG(LogEquipSlotWidget, Log, TEXT("[EquipSlotFlow] EquipSlot SetData start: widget=%s nth=%d equipType=%s item=%s definition=%s idTag=%s slotIcon=%s slotHoverIcon=%s iconImage=%s itemButton=%s"),
		*GetNameSafe(this),
		Nth,
		EquipTypeTag.IsValid() ? *EquipTypeTag.ToString() : TEXT("None"),
		*GetNameSafe(ItemInstance),
		*GetNameSafe(ItemDefinition),
		ItemDefinition && ItemDefinition->IdTag.IsValid() ? *ItemDefinition->IdTag.ToString() : TEXT("None"),
		*GetNameSafe(SlotIconTexture),
		*GetNameSafe(SlotHoverIconTexture),
		*GetNameSafe(IconImage),
		*GetNameSafe(ItemButton));

	if (!ItemInstance || !ItemDefinition)
	{
		bUseSelectedEmptyIcon = false;
		SlotText = FText::GetEmpty();
		CurrentIconTexture = SlotIconTexture;
		CurrentHoverIconTexture = SlotHoverIconTexture ? SlotHoverIconTexture.Get() : CurrentIconTexture.Get();
		CurrentItemIconTexture = nullptr;
		UE_LOG(LogEquipSlotWidget, Log, TEXT("[EquipSlotFlow] EquipSlot SetData empty/default: widget=%s nth=%d currentIcon=%s currentHover=%s"),
			*GetNameSafe(this),
			Nth,
			*GetNameSafe(CurrentIconTexture),
			*GetNameSafe(CurrentHoverIconTexture));
		ApplySlotVisual();
		return;
	}

	bUseSelectedEmptyIcon = false;
	SlotText = ItemDefinition->DisplayName;
	CurrentIconTexture = SlotIconTexture;
	CurrentHoverIconTexture = SlotHoverIconTexture ? SlotHoverIconTexture.Get() : CurrentIconTexture.Get();
	CurrentItemIconTexture = ItemDefinition->IconTexture.LoadSynchronous();
	if (!CurrentItemIconTexture)
	{
		UE_LOG(LogEquipSlotWidget, Warning, TEXT("[EquipSlotFlow] EquipSlot item icon missing, using slot default: widget=%s nth=%d definition=%s iconPath=%s fallbackIcon=%s fallbackHover=%s"),
			*GetNameSafe(this),
			Nth,
			*GetNameSafe(ItemDefinition),
			*ItemDefinition->IconTexture.ToSoftObjectPath().ToString(),
			*GetNameSafe(CurrentIconTexture),
			*GetNameSafe(CurrentHoverIconTexture));
	}
	else
	{
		UE_LOG(LogEquipSlotWidget, Log, TEXT("[EquipSlotFlow] EquipSlot item icon loaded: widget=%s nth=%d definition=%s icon=%s iconPath=%s"),
			*GetNameSafe(this),
			Nth,
			*GetNameSafe(ItemDefinition),
			*GetNameSafe(CurrentItemIconTexture),
			*ItemDefinition->IconTexture.ToSoftObjectPath().ToString());
	}
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
	UE_LOG(LogEquipSlotWidget, Log, TEXT("[InventoryFilter] EquipSlot button clicked: widget=%s nth=%d text=%s button=%s"),
		*GetNameSafe(this),
		Nth,
		*SlotText.ToString(),
		*GetNameSafe(ItemButton));
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

	UE_LOG(LogEquipSlotWidget, Log, TEXT("[EquipSlotIconDebug] ApplySlotVisual: widget=%s nth=%d item=%s iconImage=%s itemImage=%s slotIcon=%s slotHoverIcon=%s currentIcon=%s currentHover=%s itemIcon=%s displayIcon=%s hovered=%s selectedEmpty=%s dragHovered=%s"),
		*GetNameSafe(this),
		Nth,
		*GetNameSafe(ItemInstance),
		*GetNameSafe(IconImage),
		*GetNameSafe(ItemImage),
		*GetNameSafe(SlotIconTexture),
		*GetNameSafe(SlotHoverIconTexture),
		*GetNameSafe(CurrentIconTexture),
		*GetNameSafe(CurrentHoverIconTexture),
		*GetNameSafe(CurrentItemIconTexture),
		*GetNameSafe(DisplayIconTexture),
		bIsButtonHovered ? TEXT("true") : TEXT("false"),
		bUseSelectedEmptyIcon ? TEXT("true") : TEXT("false"),
		bIsAcceptedDragHovered ? TEXT("true") : TEXT("false"));

	if (IconImage)
	{
		UE_LOG(LogEquipSlotWidget, Log, TEXT("[EquipSlotFlow] EquipSlot visual applied via IconImage: widget=%s nth=%d displayIcon=%s normalIcon=%s hoverIcon=%s hasIcon=%s text=%s"),
			*GetNameSafe(this),
			Nth,
			*GetNameSafe(DisplayIconTexture),
			*GetNameSafe(NormalIconTexture),
			*GetNameSafe(HoverIconTexture),
			bHasSlotIcon ? TEXT("true") : TEXT("false"),
			*SlotText.ToString());
		IconImage->SetBrushFromTexture(DisplayIconTexture, false);
		IconImage->SetVisibility(bHasSlotIcon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		UE_LOG(LogEquipSlotWidget, Log, TEXT("[EquipSlotIconDebug] IconImage result: widget=%s nth=%d iconImage=%s texture=%s visibility=%s"),
			*GetNameSafe(this),
			Nth,
			*GetNameSafe(IconImage),
			*GetNameSafe(DisplayIconTexture),
			VisibilityToString(IconImage->GetVisibility()));
	}
	else if (bHasSlotIcon && ItemButton && !ItemInstance)
	{
		UTexture2D* RestIconTexture = (bUseSelectedEmptyIcon || bIsAcceptedDragHovered) ? HoverIconTexture : NormalIconTexture;
		FButtonStyle ButtonStyle = bHasDefaultButtonStyle ? DefaultButtonStyle : ItemButton->GetStyle();
		ButtonStyle.SetNormal(MakeSlotIconBrush(ButtonStyle.Normal, RestIconTexture, FLinearColor(0.9f, 0.9f, 0.9f, 1.0f)));
		ButtonStyle.SetHovered(MakeSlotIconBrush(ButtonStyle.Hovered, HoverIconTexture, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));
		ButtonStyle.SetPressed(MakeSlotIconBrush(ButtonStyle.Pressed, HoverIconTexture, FLinearColor(0.75f, 0.75f, 0.75f, 1.0f)));
		ButtonStyle.SetDisabled(MakeSlotIconBrush(ButtonStyle.Disabled, RestIconTexture, FLinearColor(0.35f, 0.35f, 0.35f, 1.0f)));
		ItemButton->SetStyle(ButtonStyle);
		UE_LOG(LogEquipSlotWidget, Log, TEXT("[EquipSlotIconDebug] Empty slot visual applied via ButtonStyle: widget=%s nth=%d restIcon=%s normalIcon=%s hoverIcon=%s selectedEmpty=%s dragHovered=%s"),
			*GetNameSafe(this),
			Nth,
			*GetNameSafe(RestIconTexture),
			*GetNameSafe(NormalIconTexture),
			*GetNameSafe(HoverIconTexture),
			bUseSelectedEmptyIcon ? TEXT("true") : TEXT("false"),
			bIsAcceptedDragHovered ? TEXT("true") : TEXT("false"));
	}
	else if (bHasSlotIcon && !ItemInstance)
	{
		UE_LOG(LogEquipSlotWidget, Warning, TEXT("[EquipSlotIconDebug] Slot icon texture is set but no slot icon Image is bound: widget=%s nth=%d texture=%s. Add/rename an Image to IconImage, SlotIconImage, EquipIconImage, SlotIcon, EquipIcon, or Icon."),
			*GetNameSafe(this),
			Nth,
			*GetNameSafe(DisplayIconTexture));
	}

	if (ItemImage)
	{
		ItemImage->SetBrushFromTexture(CurrentItemIconTexture, false);
		ItemImage->SetVisibility(bHasItemIcon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		UE_LOG(LogEquipSlotWidget, Log, TEXT("[EquipSlotIconDebug] ItemImage result: widget=%s nth=%d itemImage=%s texture=%s visibility=%s"),
			*GetNameSafe(this),
			Nth,
			*GetNameSafe(ItemImage),
			*GetNameSafe(CurrentItemIconTexture),
			VisibilityToString(ItemImage->GetVisibility()));
	}
	else if (bHasItemIcon)
	{
		UE_LOG(LogEquipSlotWidget, Warning, TEXT("[EquipSlotIcon] Item texture is set but ItemImage widget is not bound: widget=%s nth=%d icon=%s"),
			*GetNameSafe(this),
			Nth,
			*GetNameSafe(CurrentItemIconTexture));
	}

	if (ApplyText)
	{
		ApplyText->SetText(SlotText);
		ApplyText->SetVisibility((bHasSlotIcon || bHasItemIcon) ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
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
		ItemImage = Cast<UImage>(GetWidgetFromName(TEXT("ItemImage")));
	}
	if (!ItemImage)
	{
		ItemImage = Cast<UImage>(GetWidgetFromName(TEXT("EquippedItemImage")));
	}

	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("IconImage")));
	}
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("SlotIconImage")));
	}
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("EquipIconImage")));
	}
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("SlotIcon")));
	}
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("EquipIcon")));
	}
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("Icon")));
	}
	if (!SelectionBorderImage)
	{
		SelectionBorderImage = Cast<UImage>(GetWidgetFromName(TEXT("SelectionBorderImage")));
	}
	if (!SelectionBorderImage)
	{
		SelectionBorderImage = Cast<UImage>(GetWidgetFromName(TEXT("SelectedBorderImage")));
	}
	if (!SelectionBorderImage)
	{
		SelectionBorderImage = Cast<UImage>(GetWidgetFromName(TEXT("HighlightBorderImage")));
	}
	if (!SelectionBorderImage)
	{
		SelectionBorderImage = Cast<UImage>(GetWidgetFromName(TEXT("SelectionHighlightImage")));
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
		if (bHasDefaultButtonStyle)
		{
			ItemButton->SetStyle(DefaultButtonStyle);
		}
		return;
	}

	FButtonStyle ButtonStyle = bHasDefaultButtonStyle ? DefaultButtonStyle : ItemButton->GetStyle();
	ButtonStyle.SetNormal(MakeSolidBrush(ButtonStyle.Normal, ButtonNormalColor));
	ButtonStyle.SetHovered(MakeSolidBrush(ButtonStyle.Hovered, ButtonHoverColor));
	ButtonStyle.SetPressed(MakeSolidBrush(ButtonStyle.Pressed, ButtonPressedColor));
	ButtonStyle.SetDisabled(MakeSolidBrush(ButtonStyle.Disabled, FLinearColor(ButtonNormalColor.R, ButtonNormalColor.G, ButtonNormalColor.B, 0.35f)));
	ItemButton->SetStyle(ButtonStyle);
}

bool UEquipSlotWidget::CanAcceptDroppedItem(UItemInstance* DroppedItem) const
{
	const UItemDefinition* ItemDefinition = DroppedItem ? DroppedItem->ItemDefinition.Get() : nullptr;
	const FGameplayTag AcceptedTag = GetAcceptedEquipTypeTag();
	return ItemDefinition && ItemDefinition->IdTag.IsValid() && AcceptedTag.IsValid() && ItemDefinition->IdTag.MatchesTag(AcceptedTag);
}

UTexture2D* UEquipSlotWidget::GetCurrentIconTexture(const bool bForHover) const
{
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
