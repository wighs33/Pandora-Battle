#include "UI/Widget/SkinEquipSlotWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdHUD.h"
#include "Skin/SkinDefinition.h"
#include "Skin/SkinInstance.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/SkinSlotDragDropOperation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinEquipSlotWidget)

DEFINE_LOG_CATEGORY_STATIC(LogSkinEquipSlotWidget, Log, All);

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

UInfoWidget* ResolveInfoWidgetFromSkinEquipSlot(const UUserWidget* Widget)
{
	const APlayerController* PlayerController = Widget ? Widget->GetOwningPlayer() : nullptr;
	const APdHUD* Hud = PlayerController ? PlayerController->GetHUD<APdHUD>() : nullptr;
	return Hud ? Hud->GetInfoWidget() : nullptr;
}
}

void USkinEquipSlotWidget::NativeConstruct()
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

void USkinEquipSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplySlotVisual();
}

void USkinEquipSlotWidget::NativeDestruct()
{
	if (ItemButton)
	{
		ItemButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleButtonClicked);
		ItemButton->OnHovered.RemoveDynamic(this, &ThisClass::HandleButtonHovered);
		ItemButton->OnUnhovered.RemoveDynamic(this, &ThisClass::HandleButtonUnhovered);
	}

	Super::NativeDestruct();
}

void USkinEquipSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	if (UInfoWidget* InfoWidget = ResolveInfoWidgetFromSkinEquipSlot(this))
	{
		if (SkinInstance)
		{
			InfoWidget->ShowSkinDetailAtWidget(SkinInstance, this, false);
		}
		else if (SkinDefinition)
		{
			InfoWidget->ShowSkinDefinitionDetailAtWidget(SkinDefinition, this, false);
		}
		else
		{
			InfoWidget->HideDetailWidgets();
		}
	}
}

void USkinEquipSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	if (UInfoWidget* InfoWidget = ResolveInfoWidgetFromSkinEquipSlot(this))
	{
		InfoWidget->HideDetailWidgets();
	}

	Super::NativeOnMouseLeave(InMouseEvent);
}

bool USkinEquipSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	bIsAcceptedDragHovered = false;

	if (USkinSlotDragDropOperation* SkinDragOperation = Cast<USkinSlotDragDropOperation>(InOperation))
	{
		USkinInstance* DroppedSkin = SkinDragOperation->GetSkinInstance();
		if (CanAcceptDroppedSkin(DroppedSkin))
		{
			UE_LOG(LogSkinEquipSlotWidget, Log, TEXT("[SkinSlotDragDrop] Skin dropped on skin equip slot: slot=%s skin=%s sourceSlot=%d"),
				*GetNameSafe(this),
				*GetNameSafe(DroppedSkin),
				SkinDragOperation->GetSourceSlotIndex());
			OnDroppedSkin_SkinEquipSlot.Broadcast(this, DroppedSkin);
			ApplySlotVisual();
			return true;
		}

		const USkinDefinition* DroppedDefinition = DroppedSkin ? DroppedSkin->SkinDefinition.Get() : nullptr;
		UE_LOG(LogSkinEquipSlotWidget, Warning, TEXT("[SkinSlotDragDrop] Skin drop rejected by skin equip slot type: slot=%s acceptedTag=%s skin=%s definition=%s idTag=%s"),
			*GetNameSafe(this),
			GetAcceptedEquipTypeTag().IsValid() ? *GetAcceptedEquipTypeTag().ToString() : TEXT("None"),
			*GetNameSafe(DroppedSkin),
			*GetNameSafe(DroppedDefinition),
			DroppedDefinition && DroppedDefinition->IdTag.IsValid() ? *DroppedDefinition->IdTag.ToString() : TEXT("None"));
	}

	ApplySlotVisual();
	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void USkinEquipSlotWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	bIsAcceptedDragHovered = false;
	if (USkinSlotDragDropOperation* SkinDragOperation = Cast<USkinSlotDragDropOperation>(InOperation))
	{
		bIsAcceptedDragHovered = CanAcceptDroppedSkin(SkinDragOperation->GetSkinInstance());
	}

	ApplySlotVisual();
}

void USkinEquipSlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);

	bIsAcceptedDragHovered = false;
	ApplySlotVisual();
}

void USkinEquipSlotWidget::BroadcastClickedSkinEquipSlot(USkinEquipSlotWidget* SkinEquipSlot)
{
	OnClicked_SkinEquipSlot.Broadcast(SkinEquipSlot ? SkinEquipSlot : this);
}

void USkinEquipSlotWidget::SetText(const FText& InText)
{
	SlotText = InText;
	ApplySlotVisual();
}

void USkinEquipSlotWidget::SetIcon(UTexture2D* InIconTexture)
{
	SlotIconTexture = InIconTexture;

	if (!SkinInstance)
	{
		CurrentIconTexture = SlotIconTexture;
		CurrentHoverIconTexture = SlotHoverIconTexture ? SlotHoverIconTexture.Get() : CurrentIconTexture.Get();
	}

	ApplySlotVisual();
}

void USkinEquipSlotWidget::SetHoverIcon(UTexture2D* InIconTexture)
{
	SlotHoverIconTexture = InIconTexture;

	if (!SkinInstance)
	{
		CurrentHoverIconTexture = SlotHoverIconTexture ? SlotHoverIconTexture.Get() : CurrentIconTexture.Get();
	}

	ApplySlotVisual();
}

void USkinEquipSlotWidget::SetData(USkinInstance* Target)
{
	SkinInstance = Target;
	SkinDefinition = IsValid(SkinInstance) ? SkinInstance->SkinDefinition.Get() : nullptr;
	UE_LOG(LogSkinEquipSlotWidget, Log, TEXT("[SkinSlotFlow] SkinEquipSlot SetData start: widget=%s equipType=%s skin=%s definition=%s idTag=%s slotIcon=%s slotHoverIcon=%s iconImage=%s itemButton=%s"),
		*GetNameSafe(this),
		EquipTypeTag.IsValid() ? *EquipTypeTag.ToString() : TEXT("None"),
		*GetNameSafe(SkinInstance),
		*GetNameSafe(SkinDefinition),
		SkinDefinition && SkinDefinition->IdTag.IsValid() ? *SkinDefinition->IdTag.ToString() : TEXT("None"),
		*GetNameSafe(SlotIconTexture),
		*GetNameSafe(SlotHoverIconTexture),
		*GetNameSafe(IconImage),
		*GetNameSafe(ItemButton));

	if (!SkinDefinition)
	{
		bUseSelectedEmptyIcon = false;
		SlotText = FText::GetEmpty();
		CurrentIconTexture = SlotIconTexture;
		CurrentHoverIconTexture = SlotHoverIconTexture ? SlotHoverIconTexture.Get() : CurrentIconTexture.Get();
		CurrentSkinIconTexture = nullptr;
		UE_LOG(LogSkinEquipSlotWidget, Log, TEXT("[SkinSlotFlow] SkinEquipSlot SetData empty/default: widget=%s currentIcon=%s currentHover=%s"),
			*GetNameSafe(this),
			*GetNameSafe(CurrentIconTexture),
			*GetNameSafe(CurrentHoverIconTexture));
		ApplySlotVisual();
		return;
	}

	bUseSelectedEmptyIcon = false;
	SlotText = SkinDefinition->DisplayName;
	CurrentIconTexture = SlotIconTexture;
	CurrentHoverIconTexture = SlotHoverIconTexture ? SlotHoverIconTexture.Get() : CurrentIconTexture.Get();
	CurrentSkinIconTexture = SkinDefinition->IconTexture;
	if (!CurrentSkinIconTexture)
	{
		UE_LOG(LogSkinEquipSlotWidget, Warning, TEXT("[SkinSlotFlow] SkinEquipSlot skin icon missing, using slot default: widget=%s definition=%s fallbackIcon=%s fallbackHover=%s"),
			*GetNameSafe(this),
			*GetNameSafe(SkinDefinition),
			*GetNameSafe(CurrentIconTexture),
			*GetNameSafe(CurrentHoverIconTexture));
	}
	else
	{
		UE_LOG(LogSkinEquipSlotWidget, Log, TEXT("[SkinSlotFlow] SkinEquipSlot skin icon loaded: widget=%s definition=%s icon=%s"),
			*GetNameSafe(this),
			*GetNameSafe(SkinDefinition),
			*GetNameSafe(CurrentSkinIconTexture));
	}
	ApplySlotVisual();
}

void USkinEquipSlotWidget::SetSkinDefinition(const USkinDefinition* Target)
{
	SkinInstance = nullptr;
	SkinDefinition = Target;
	UE_LOG(LogSkinEquipSlotWidget, Log, TEXT("[SkinSlotFlow] SkinEquipSlot SetSkinDefinition start: widget=%s equipType=%s definition=%s idTag=%s slotIcon=%s slotHoverIcon=%s"),
		*GetNameSafe(this),
		EquipTypeTag.IsValid() ? *EquipTypeTag.ToString() : TEXT("None"),
		*GetNameSafe(SkinDefinition),
		SkinDefinition && SkinDefinition->IdTag.IsValid() ? *SkinDefinition->IdTag.ToString() : TEXT("None"),
		*GetNameSafe(SlotIconTexture),
		*GetNameSafe(SlotHoverIconTexture));

	if (!SkinDefinition)
	{
		bUseSelectedEmptyIcon = false;
		SlotText = FText::GetEmpty();
		CurrentIconTexture = SlotIconTexture;
		CurrentHoverIconTexture = SlotHoverIconTexture ? SlotHoverIconTexture.Get() : CurrentIconTexture.Get();
		CurrentSkinIconTexture = nullptr;
		UE_LOG(LogSkinEquipSlotWidget, Log, TEXT("[SkinSlotFlow] SkinEquipSlot SetSkinDefinition empty/default: widget=%s currentIcon=%s currentHover=%s"),
			*GetNameSafe(this),
			*GetNameSafe(CurrentIconTexture),
			*GetNameSafe(CurrentHoverIconTexture));
		ApplySlotVisual();
		return;
	}

	bUseSelectedEmptyIcon = false;
	SlotText = SkinDefinition->DisplayName;
	CurrentIconTexture = SlotIconTexture;
	CurrentHoverIconTexture = SlotHoverIconTexture ? SlotHoverIconTexture.Get() : CurrentIconTexture.Get();
	CurrentSkinIconTexture = SkinDefinition->IconTexture;
	ApplySlotVisual();
}

void USkinEquipSlotWidget::SetSelected(const bool bInSelected)
{
	if (bIsSelected == bInSelected)
	{
		return;
	}

	bIsSelected = bInSelected;
	ApplySlotVisual();
}

void USkinEquipSlotWidget::SetResolvedEquipTypeTag(const FGameplayTag InResolvedEquipTypeTag)
{
	ResolvedEquipTypeTag = InResolvedEquipTypeTag;
}

FGameplayTag USkinEquipSlotWidget::GetAcceptedEquipTypeTag() const
{
	return EquipTypeTag.IsValid() ? EquipTypeTag : ResolvedEquipTypeTag;
}

void USkinEquipSlotWidget::HandleButtonClicked()
{
	BroadcastClickedSkinEquipSlot(this);
}

void USkinEquipSlotWidget::HandleButtonHovered()
{
	bIsButtonHovered = true;
	ApplySlotVisual();
}

void USkinEquipSlotWidget::HandleButtonUnhovered()
{
	bIsButtonHovered = false;
	ApplySlotVisual();
}

void USkinEquipSlotWidget::ApplySlotVisual()
{
	CacheOptionalWidgets();
	ApplyButtonBackgroundStyle();

	UTexture2D* NormalIconTexture = GetCurrentIconTexture(false);
	UTexture2D* HoverIconTexture = GetCurrentIconTexture(true);
	UTexture2D* DisplayIconTexture = (bIsButtonHovered || bUseSelectedEmptyIcon || bIsAcceptedDragHovered) ? HoverIconTexture : NormalIconTexture;
	const bool bHasIcon = DisplayIconTexture != nullptr;
	const bool bHasEquippedSkin = SkinDefinition != nullptr;
	const bool bHasSkinIcon = CurrentSkinIconTexture != nullptr;

	UImage* EquippedSkinImage = bHasEquippedSkin ? (SkinImage ? SkinImage.Get() : IconImage.Get()) : nullptr;

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
		else if (bHasSkinIcon)
		{
			UE_LOG(LogSkinEquipSlotWidget, Warning, TEXT("[SkinSlotFlow] Equipped skin icon is set but no SkinImage/IconImage widget is bound: widget=%s icon=%s"),
				*GetNameSafe(this),
				*GetNameSafe(CurrentSkinIconTexture));
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
			UE_LOG(LogSkinEquipSlotWidget, Log, TEXT("[SkinSlotFlow] SkinEquipSlot visual applied via IconImage: widget=%s displayIcon=%s normalIcon=%s hoverIcon=%s hasIcon=%s text=%s"),
				*GetNameSafe(this),
				*GetNameSafe(DisplayIconTexture),
				*GetNameSafe(NormalIconTexture),
				*GetNameSafe(HoverIconTexture),
				bHasIcon ? TEXT("true") : TEXT("false"),
				*SlotText.ToString());
			IconImage->SetBrushFromTexture(DisplayIconTexture, false);
			IconImage->SetVisibility(bHasIcon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
		else if (bHasIcon && ItemButton)
		{
			UE_LOG(LogSkinEquipSlotWidget, Log, TEXT("[SkinSlotFlow] SkinEquipSlot visual applied via ButtonStyle: widget=%s restIcon=%s displayIcon=%s normalIcon=%s hoverIcon=%s text=%s"),
				*GetNameSafe(this),
				*GetNameSafe(bUseSelectedEmptyIcon ? HoverIconTexture : NormalIconTexture),
				*GetNameSafe(DisplayIconTexture),
				*GetNameSafe(NormalIconTexture),
				*GetNameSafe(HoverIconTexture),
				*SlotText.ToString());
			UTexture2D* RestIconTexture = (bUseSelectedEmptyIcon || bIsAcceptedDragHovered) ? HoverIconTexture : NormalIconTexture;
			FButtonStyle ButtonStyle = ItemButton->GetStyle();
			ButtonStyle.SetNormal(MakeSlotIconBrush(ButtonStyle.Normal, RestIconTexture, FLinearColor(0.9f, 0.9f, 0.9f, 1.0f)));
			ButtonStyle.SetHovered(MakeSlotIconBrush(ButtonStyle.Hovered, HoverIconTexture, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));
			ButtonStyle.SetPressed(MakeSlotIconBrush(ButtonStyle.Pressed, HoverIconTexture, FLinearColor(0.75f, 0.75f, 0.75f, 1.0f)));
			ButtonStyle.SetDisabled(MakeSlotIconBrush(ButtonStyle.Disabled, RestIconTexture, FLinearColor(0.35f, 0.35f, 0.35f, 1.0f)));
			ItemButton->SetStyle(ButtonStyle);
		}
		else if (bHasIcon)
		{
			UE_LOG(LogSkinEquipSlotWidget, Warning, TEXT("[SkinEquipSlotIcon] Icon texture is set but IconImage widget is not bound: widget=%s icon=%s"),
				*GetNameSafe(this),
				*GetNameSafe(DisplayIconTexture));
		}
		else if (ItemButton && bHasDefaultButtonStyle)
		{
			UE_LOG(LogSkinEquipSlotWidget, Log, TEXT("[SkinSlotFlow] SkinEquipSlot visual restored default button style: widget=%s text=%s"),
				*GetNameSafe(this),
				*SlotText.ToString());
			ItemButton->SetStyle(DefaultButtonStyle);
		}
	}

	if (SkinImage && (!EquippedSkinImage || SkinImage.Get() != EquippedSkinImage))
	{
		SkinImage->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (ApplyText)
	{
		ApplyText->SetText(SlotText);
		ApplyText->SetVisibility((bHasIcon || bHasSkinIcon) ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	if (SelectionBorderImage)
	{
		SelectionBorderImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		SelectionBorderImage->SetColorAndOpacity(bIsSelected ? SelectionBorderSelectedColor : SelectionBorderDefaultColor);
	}
}

void USkinEquipSlotWidget::CacheOptionalWidgets()
{
	if (!SkinImage)
	{
		SkinImage = Cast<UImage>(GetWidgetFromName(TEXT("SkinImage")));
	}
	if (!SkinImage)
	{
		SkinImage = Cast<UImage>(GetWidgetFromName(TEXT("EquippedSkinImage")));
	}
	if (!SkinImage)
	{
		SkinImage = Cast<UImage>(GetWidgetFromName(TEXT("ItemImage")));
	}
	if (!SkinImage)
	{
		SkinImage = Cast<UImage>(GetWidgetFromName(TEXT("EquippedItemImage")));
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
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("SkinIconImage")));
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
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("SkinIcon")));
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

void USkinEquipSlotWidget::ApplyButtonBackgroundStyle()
{
	if (!ItemButton)
	{
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

bool USkinEquipSlotWidget::CanAcceptDroppedSkin(USkinInstance* DroppedSkin) const
{
	const USkinDefinition* DroppedDefinition = DroppedSkin ? DroppedSkin->SkinDefinition.Get() : nullptr;
	const FGameplayTag AcceptedTag = GetAcceptedEquipTypeTag();
	return DroppedDefinition && DroppedDefinition->IdTag.IsValid() && AcceptedTag.IsValid() && DroppedDefinition->IdTag.MatchesTag(AcceptedTag);
}

UTexture2D* USkinEquipSlotWidget::GetCurrentIconTexture(const bool bForHover) const
{
	if (bForHover)
	{
		if (CurrentHoverIconTexture)
		{
			return CurrentHoverIconTexture.Get();
		}

		if (!SkinDefinition && SlotHoverIconTexture)
		{
			return SlotHoverIconTexture.Get();
		}
	}

	return CurrentIconTexture ? CurrentIconTexture.Get() : SlotIconTexture.Get();
}

void USkinEquipSlotWidget::CacheDefaultButtonStyle()
{
	if (!ItemButton || bHasDefaultButtonStyle)
	{
		return;
	}

	DefaultButtonStyle = ItemButton->GetStyle();
	bHasDefaultButtonStyle = true;
}
