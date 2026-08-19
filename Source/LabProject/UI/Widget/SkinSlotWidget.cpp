#include "UI/Widget/SkinSlotWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "Mode/PdHUD.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "Skin/SkinInstance.h"
#include "UI/Widget/DragItemVisualWidget.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/SkinSlotDragDropOperation.h"
#include "UI/Widget/SkinSlotViewData.h"
#include "UI/WidgetLookup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinSlotWidget)

namespace
{
UInfoWidget* ResolveInfoWidgetFromSkinSlot(const UUserWidget* Widget)
{
	const APlayerController* PlayerController = Widget ? Widget->GetOwningPlayer() : nullptr;
	const APdHUD* Hud = PlayerController ? PlayerController->GetHUD<APdHUD>() : nullptr;
	return Hud ? Hud->GetInfoWidget() : nullptr;
}
}

void USkinSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplySelectionVisual();
}

void USkinSlotWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	if (USkinSlotViewData* SlotViewData = Cast<USkinSlotViewData>(ListItemObject))
	{
		SetSlotData(SlotViewData);
	}
	else if (USkinInstance* SkinInstance = Cast<USkinInstance>(ListItemObject))
	{
		SetData(SkinInstance);
	}
}

void USkinSlotWidget::NativeOnItemSelectionChanged(const bool bInIsSelected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(bInIsSelected);

	SetSelected(bInIsSelected);
}

void USkinSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	if (UInfoWidget* InfoWidget = ResolveInfoWidgetFromSkinSlot(this))
	{
		if (CachedData)
		{
			InfoWidget->ShowSkinDetailAtWidget(CachedData, this, true);
		}
		else
		{
			InfoWidget->HideDetailWidgets();
		}
	}
}

void USkinSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	if (UInfoWidget* InfoWidget = ResolveInfoWidgetFromSkinSlot(this))
	{
		InfoWidget->HideDetailWidgets();
	}

	Super::NativeOnMouseLeave(InMouseEvent);
}

FReply USkinSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (CachedData && InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void USkinSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if (!CachedData)
	{
		return;
	}

	USkinSlotDragDropOperation* DragOperation = NewObject<USkinSlotDragDropOperation>(this);
	if (!DragOperation)
	{
		return;
	}

	const int32 SourceSlotIndex = CachedSlotData ? CachedSlotData->GetSlotIndex() : INDEX_NONE;
	DragOperation->Initialize(SourceSlotIndex, CachedData, CachedSlotData);
	DragOperation->Pivot = EDragPivot::CenterCenter;

	const USkinDefinition* SkinDefinition = CachedData->SkinDefinition.Get();
	UTexture2D* IconTexture = SkinDefinition ? SkinDefinition->IconTexture.Get() : nullptr;
	if (IconTexture)
	{
		CacheOptionalWidgets();

		FVector2D EffectiveDragIconSize = IconImage ? IconImage->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
		if (EffectiveDragIconSize.X <= 1.0f || EffectiveDragIconSize.Y <= 1.0f)
		{
			EffectiveDragIconSize = InGeometry.GetLocalSize();
		}
		if (EffectiveDragIconSize.X <= 0.0f || EffectiveDragIconSize.Y <= 0.0f)
		{
			EffectiveDragIconSize = DragIconSize;
		}

		if (DragVisualWidgetClass)
		{
			UDragItemVisualWidget* DragVisualWidget = CreateWidget<UDragItemVisualWidget>(GetWorld(), DragVisualWidgetClass);
			if (DragVisualWidget)
			{
				DragVisualWidget->SetIconSize(EffectiveDragIconSize);
				DragVisualWidget->SetIconTexture(IconTexture);
				DragOperation->DefaultDragVisual = DragVisualWidget;
			}
		}

		if (!DragOperation->DefaultDragVisual)
		{
			if (USizeBox* DragSizeBox = NewObject<USizeBox>(DragOperation))
			{
				DragSizeBox->SetWidthOverride(EffectiveDragIconSize.X);
				DragSizeBox->SetHeightOverride(EffectiveDragIconSize.Y);

				UImage* DragVisual = NewObject<UImage>(DragSizeBox);
				if (DragVisual)
				{
					DragVisual->SetBrushFromTexture(IconTexture, false);
					DragVisual->SetDesiredSizeOverride(EffectiveDragIconSize);
					DragSizeBox->AddChild(DragVisual);
				}

				DragOperation->DefaultDragVisual = DragSizeBox;
			}
		}
	}

	OutOperation = DragOperation;
}

void USkinSlotWidget::SetData(USkinInstance* Target)
{
	CachedData = Target;
	CachedSlotData = nullptr;

	ApplySkinVisual(CachedData);
}

void USkinSlotWidget::SetSlotData(USkinSlotViewData* Target)
{
	CachedSlotData = Target;
	CachedData = Target ? Target->GetSkinInstance() : nullptr;

	ApplySkinVisual(CachedData);
}

void USkinSlotWidget::ApplySkinVisual(USkinInstance* Target)
{
	const USkinDefinition* SkinDefinition = Target ? Target->SkinDefinition.Get() : nullptr;
	UTexture2D* IconTexture = SkinDefinition ? SkinDefinition->IconTexture.Get() : nullptr;
	const bool bAssigned = CachedSlotData && CachedSlotData->IsAssigned();

	CacheOptionalWidgets();

	if (IconImage)
	{
		IconImage->SetBrushFromTexture(IconTexture, false);
		IconImage->SetVisibility(IconTexture ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Txt_Assigned)
	{
		Txt_Assigned->SetVisibility(
			bAssigned && SkinDefinition
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
	}

	if (Img_Back)
	{
		Img_Back->SetColorAndOpacity(
			bAssigned && SkinDefinition
				? ResolveAssignedBackgroundColor()
				: DefaultBackgroundColor);
	}

	ApplySelectionVisual();
}

void USkinSlotWidget::SetSelected(const bool bInSelected)
{
	if (bIsSelected == bInSelected)
	{
		return;
	}

	bIsSelected = bInSelected;
	ApplySelectionVisual();
}

void USkinSlotWidget::CacheOptionalWidgets()
{
	if (!Txt_Assigned)
	{
		Txt_Assigned = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("Txt_Assigned"),
			TEXT("AssignedText"),
			TEXT("AssignedTextBlock")
		});
	}

	if (!Img_Back)
	{
		Img_Back = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("Img_Back"),
			TEXT("BackgroundImage"),
			TEXT("SkinBackgroundImage"),
			TEXT("SlotBackgroundImage")
		});
	}

	if (!IconImage)
	{
		IconImage = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("IconImage"),
			TEXT("SkinIconImage"),
			TEXT("SlotIconImage"),
			TEXT("SkinIcon"),
			TEXT("SlotIcon"),
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

	if (Img_Back && !bDefaultBackgroundColorCached)
	{
		DefaultBackgroundColor = Img_Back->GetColorAndOpacity();
		bDefaultBackgroundColorCached = true;
	}
}

void USkinSlotWidget::ApplySelectionVisual()
{
	CacheOptionalWidgets();

	if (!SelectionBorderImage)
	{
		return;
	}

	SelectionBorderImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SelectionBorderImage->SetColorAndOpacity(bIsSelected ? SelectionBorderSelectedColor : SelectionBorderDefaultColor);
}

FLinearColor USkinSlotWidget::ResolveAssignedBackgroundColor() const
{
	const UWidgetClassDefinition* WidgetDefinition =
		UWidgetClassDefinition::ResolveWidgetClassDefinition(this);
	const FInventoryWidgetSettings DefaultSettings;
	return WidgetDefinition
		? WidgetDefinition->GetInventoryWidgetSettings().AssignedItemBackgroundColor
		: DefaultSettings.AssignedItemBackgroundColor;
}
