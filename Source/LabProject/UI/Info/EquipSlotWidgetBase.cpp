#include "UI/Info/EquipSlotWidgetBase.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "UI/HUD/PdHUD.h"
#include "UI/Info/InfoWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipSlotWidgetBase)

namespace
{
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

UInfoWidget* ResolveInfoWidgetFromEquipSlot(const UUserWidget* Widget)
{
	const APlayerController* PlayerController = Widget ? Widget->GetOwningPlayer() : nullptr;
	const APdHUD* Hud = PlayerController ? PlayerController->GetHUD<APdHUD>() : nullptr;
	return Hud ? Hud->GetInfoWidget() : nullptr;
}
}

void UEquipSlotWidgetBase::NativeConstruct()
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

void UEquipSlotWidgetBase::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplySlotVisual();
}

void UEquipSlotWidgetBase::NativeDestruct()
{
	if (ItemButton)
	{
		ItemButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleButtonClicked);
		ItemButton->OnHovered.RemoveDynamic(this, &ThisClass::HandleButtonHovered);
		ItemButton->OnUnhovered.RemoveDynamic(this, &ThisClass::HandleButtonUnhovered);
	}

	Super::NativeDestruct();
}

void UEquipSlotWidgetBase::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	if (UInfoWidget* InfoWidget = ResolveInfoWidgetFromEquipSlot(this); InfoWidget && !ShowSlotDetail(*InfoWidget))
	{
		InfoWidget->HideDetailWidgets();
	}
}

void UEquipSlotWidgetBase::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	if (UInfoWidget* InfoWidget = ResolveInfoWidgetFromEquipSlot(this))
	{
		InfoWidget->HideDetailWidgets();
	}

	Super::NativeOnMouseLeave(InMouseEvent);
}

void UEquipSlotWidgetBase::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	bIsAcceptedDragHovered = CanAcceptDragOperation(InOperation);
	ApplySlotVisual();
}

void UEquipSlotWidgetBase::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);

	bIsAcceptedDragHovered = false;
	ApplySlotVisual();
}

bool UEquipSlotWidgetBase::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	bIsAcceptedDragHovered = false;

	if (CanAcceptDragOperation(InOperation))
	{
		BroadcastAcceptedDrop(InOperation);
		ApplySlotVisual();
		return true;
	}

	ApplySlotVisual();
	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UEquipSlotWidgetBase::SetText(const FText& InText)
{
	SlotText = InText;
	ApplySlotVisual();
}

void UEquipSlotWidgetBase::SetIcon(UTexture2D* InIconTexture)
{
	SlotIconTexture = InIconTexture;

	if (!HasSlotContent())
	{
		ResetSlotIcons();
	}

	ApplySlotVisual();
}

void UEquipSlotWidgetBase::SetHoverIcon(UTexture2D* InIconTexture)
{
	SlotHoverIconTexture = InIconTexture;

	if (!HasSlotContent())
	{
		CurrentHoverIconTexture = SlotHoverIconTexture ? SlotHoverIconTexture.Get() : CurrentIconTexture.Get();
	}

	ApplySlotVisual();
}

void UEquipSlotWidgetBase::SetSelected(const bool bInSelected)
{
	if (bIsSelected == bInSelected)
	{
		return;
	}

	bIsSelected = bInSelected;
	ApplySlotVisual();
}

void UEquipSlotWidgetBase::SetResolvedEquipTypeTag(const FGameplayTag InResolvedEquipTypeTag)
{
	ResolvedEquipTypeTag = InResolvedEquipTypeTag;
}

FGameplayTag UEquipSlotWidgetBase::GetAcceptedEquipTypeTag() const
{
	return EquipTypeTag.IsValid() ? EquipTypeTag : ResolvedEquipTypeTag;
}

UTexture2D* UEquipSlotWidgetBase::GetCurrentIconTexture(const bool bForHover) const
{
	if (bForHover)
	{
		if (CurrentHoverIconTexture)
		{
			return CurrentHoverIconTexture.Get();
		}

		if (!HasSlotContent() && SlotHoverIconTexture)
		{
			return SlotHoverIconTexture.Get();
		}
	}

	return CurrentIconTexture ? CurrentIconTexture.Get() : SlotIconTexture.Get();
}

UTexture2D* UEquipSlotWidgetBase::GetDisplayIconTexture() const
{
	return GetCurrentIconTexture(bIsButtonHovered || bIsAcceptedDragHovered);
}

UTexture2D* UEquipSlotWidgetBase::GetRestIconTexture() const
{
	return GetCurrentIconTexture(bIsAcceptedDragHovered);
}

void UEquipSlotWidgetBase::ResetSlotIcons()
{
	CurrentIconTexture = SlotIconTexture;
	CurrentHoverIconTexture = SlotHoverIconTexture ? SlotHoverIconTexture.Get() : CurrentIconTexture.Get();
}

void UEquipSlotWidgetBase::ApplySlotText(const bool bHasAnyIcon)
{
	if (ApplyText)
	{
		ApplyText->SetText(SlotText);
		ApplyText->SetVisibility(bHasAnyIcon ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UEquipSlotWidgetBase::ApplySelectionBorder()
{
	if (SelectionBorderImage)
	{
		SelectionBorderImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		SelectionBorderImage->SetColorAndOpacity(bIsSelected ? SelectionBorderSelectedColor : SelectionBorderDefaultColor);
	}
}

FButtonStyle UEquipSlotWidgetBase::MakeSlotIconButtonStyle(FButtonStyle ButtonStyle, const float Opacity) const
{
	UTexture2D* RestIconTexture = GetRestIconTexture();
	UTexture2D* HoverIconTexture = GetCurrentIconTexture(true);
	ButtonStyle.SetNormal(MakeSlotIconBrush(ButtonStyle.Normal, RestIconTexture, FLinearColor(0.9f, 0.9f, 0.9f, Opacity)));
	ButtonStyle.SetHovered(MakeSlotIconBrush(ButtonStyle.Hovered, HoverIconTexture, FLinearColor(1.0f, 1.0f, 1.0f, Opacity)));
	ButtonStyle.SetPressed(MakeSlotIconBrush(ButtonStyle.Pressed, HoverIconTexture, FLinearColor(0.75f, 0.75f, 0.75f, Opacity)));
	ButtonStyle.SetDisabled(MakeSlotIconBrush(ButtonStyle.Disabled, RestIconTexture, FLinearColor(0.35f, 0.35f, 0.35f, Opacity)));
	return ButtonStyle;
}

FSlateBrush UEquipSlotWidgetBase::MakeSolidBrush(const FSlateBrush& SourceBrush, const FLinearColor& TintColor)
{
	FSlateBrush Brush = SourceBrush;
	Brush.SetResourceObject(nullptr);
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.TintColor = FSlateColor(TintColor);
	return Brush;
}

void UEquipSlotWidgetBase::HandleButtonClicked()
{
	BroadcastSlotClicked();
}

void UEquipSlotWidgetBase::HandleButtonHovered()
{
	bIsButtonHovered = true;
	ApplySlotVisual();
}

void UEquipSlotWidgetBase::HandleButtonUnhovered()
{
	bIsButtonHovered = false;
	ApplySlotVisual();
}

void UEquipSlotWidgetBase::CacheDefaultButtonStyle()
{
	if (!ItemButton || bHasDefaultButtonStyle)
	{
		return;
	}

	DefaultButtonStyle = ItemButton->GetStyle();
	bHasDefaultButtonStyle = true;
}
