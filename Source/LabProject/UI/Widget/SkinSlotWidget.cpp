#include "UI/Widget/SkinSlotWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "Mode/PdHUD.h"
#include "Skin/SkinDefinition.h"
#include "Skin/SkinInstance.h"
#include "UI/Widget/DragItemVisualWidget.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/SkinSlotDragDropOperation.h"
#include "UI/Widget/SkinSlotViewData.h"

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
		const FVector2D EffectiveDragIconSize(
			FMath::Min(DragIconSize.X, 56.0f),
			FMath::Min(DragIconSize.Y, 56.0f));

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
			UImage* DragVisual = NewObject<UImage>(DragOperation);
			DragVisual->SetBrushFromTexture(IconTexture, true);
			DragVisual->SetDesiredSizeOverride(EffectiveDragIconSize);
			DragOperation->DefaultDragVisual = DragVisual;
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
	const FText DisplayName = SkinDefinition ? SkinDefinition->DisplayName : FText::GetEmpty();
	UTexture2D* IconTexture = SkinDefinition ? SkinDefinition->IconTexture.Get() : nullptr;

	CacheOptionalWidgets();

	if (IconImage)
	{
		IconImage->SetBrushFromTexture(IconTexture, false);
		IconImage->SetVisibility(IconTexture ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (TextBlock)
	{
		TextBlock->SetText(DisplayName);
		TextBlock->SetVisibility(IconTexture ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
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
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("IconImage")));
	}
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("SkinIconImage")));
	}
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("SlotIconImage")));
	}
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("SkinIcon")));
	}
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("SlotIcon")));
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
