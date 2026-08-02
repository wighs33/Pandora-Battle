#include "UI/Widget/SkinEquipSlotWidget.h"

#include "Common/LabGameplayTags.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdHUD.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Skin/SkinInstance.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/SkinSlotDragDropOperation.h"
#include "UI/WidgetLookup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinEquipSlotWidget)

namespace
{
FSlateBrush MakeSkinSolidBrush(const FSlateBrush& SourceBrush, const FLinearColor& TintColor)
{
	FSlateBrush Brush = SourceBrush;
	Brush.SetResourceObject(nullptr);
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.TintColor = FSlateColor(TintColor);
	return Brush;
}

FSlateBrush MakeSkinSlotIconBrush(const FSlateBrush& SourceBrush, UTexture2D* IconTexture, const FLinearColor& TintColor)
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

			OnDroppedSkin_SkinEquipSlot.Broadcast(this, DroppedSkin);
			ApplySlotVisual();
			return true;
		}

		const USkinDefinition* DroppedDefinition = DroppedSkin ? DroppedSkin->SkinDefinition.Get() : nullptr;

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


	if (!SkinDefinition)
	{
		bUseSelectedEmptyIcon = false;
		SlotText = FText::GetEmpty();
		CurrentIconTexture = SlotIconTexture;
		CurrentHoverIconTexture = SlotHoverIconTexture ? SlotHoverIconTexture.Get() : CurrentIconTexture.Get();
		CurrentSkinIconTexture = nullptr;

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

void USkinEquipSlotWidget::SetSkinDefinition(const USkinDefinition* Target)
{
	SkinInstance = nullptr;
	SkinDefinition = Target;


	if (!SkinDefinition)
	{
		bUseSelectedEmptyIcon = false;
		SlotText = FText::GetEmpty();
		CurrentIconTexture = SlotIconTexture;
		CurrentHoverIconTexture = SlotHoverIconTexture ? SlotHoverIconTexture.Get() : CurrentIconTexture.Get();
		CurrentSkinIconTexture = nullptr;

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
	if (ResolvedEquipTypeTag.MatchesTag(LabGameplayTags::Skin_Gesture))
	{
		return ResolvedEquipTypeTag;
	}

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

			UTexture2D* RestIconTexture = (bUseSelectedEmptyIcon || bIsAcceptedDragHovered) ? HoverIconTexture : NormalIconTexture;
			FButtonStyle ButtonStyle = ItemButton->GetStyle();
			ButtonStyle.SetNormal(MakeSkinSlotIconBrush(ButtonStyle.Normal, RestIconTexture, FLinearColor(0.9f, 0.9f, 0.9f, 1.0f)));
			ButtonStyle.SetHovered(MakeSkinSlotIconBrush(ButtonStyle.Hovered, HoverIconTexture, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));
			ButtonStyle.SetPressed(MakeSkinSlotIconBrush(ButtonStyle.Pressed, HoverIconTexture, FLinearColor(0.75f, 0.75f, 0.75f, 1.0f)));
			ButtonStyle.SetDisabled(MakeSkinSlotIconBrush(ButtonStyle.Disabled, RestIconTexture, FLinearColor(0.35f, 0.35f, 0.35f, 1.0f)));
			ItemButton->SetStyle(ButtonStyle);
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
		SkinImage = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("SkinImage"),
			TEXT("EquippedSkinImage"),
			TEXT("ItemImage"),
			TEXT("EquippedItemImage")
		});
	}

	if (!IconImage)
	{
		IconImage = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("IconImage"),
			TEXT("SlotIconImage"),
			TEXT("SkinIconImage"),
			TEXT("EquipIconImage"),
			TEXT("SlotIcon"),
			TEXT("SkinIcon"),
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
	ButtonStyle.SetNormal(MakeSkinSolidBrush(ButtonStyle.Normal, NormalColor));
	ButtonStyle.SetHovered(MakeSkinSolidBrush(ButtonStyle.Hovered, ButtonHoverColor));
	ButtonStyle.SetPressed(MakeSkinSolidBrush(ButtonStyle.Pressed, ButtonPressedColor));
	ButtonStyle.SetDisabled(MakeSkinSolidBrush(ButtonStyle.Disabled, FLinearColor(ButtonNormalColor.R, ButtonNormalColor.G, ButtonNormalColor.B, 0.35f)));
	ItemButton->SetStyle(ButtonStyle);
}

bool USkinEquipSlotWidget::CanAcceptDroppedSkin(USkinInstance* DroppedSkin) const
{
	const USkinDefinition* DroppedDefinition = DroppedSkin ? DroppedSkin->SkinDefinition.Get() : nullptr;
	const FGameplayTag AcceptedTag = GetAcceptedEquipTypeTag();
	const FGameplayTag RequiredSkinTag = AcceptedTag.MatchesTag(LabGameplayTags::Skin_Gesture)
		? LabGameplayTags::Skin_Gesture
		: AcceptedTag;
	return DroppedDefinition && DroppedDefinition->IdTag.IsValid() && RequiredSkinTag.IsValid() && DroppedDefinition->IdTag.MatchesTag(RequiredSkinTag);
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
