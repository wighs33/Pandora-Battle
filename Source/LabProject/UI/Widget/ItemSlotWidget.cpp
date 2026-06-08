#include "UI/Widget/ItemSlotWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Item/ItemInstance.h"
#include "Mode/PdHUD.h"
#include "UI/Widget/InventorySlotViewData.h"
#include "UI/Widget/DragItemVisualWidget.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/ItemSlotDragDropOperation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ItemSlotWidget)

namespace
{
UInfoWidget* ResolveInfoWidgetFromSlot(const UUserWidget* Widget)
{
	const APlayerController* PlayerController = Widget ? Widget->GetOwningPlayer() : nullptr;
	const APdHUD* Hud = PlayerController ? PlayerController->GetHUD<APdHUD>() : nullptr;
	return Hud ? Hud->GetInfoWidget() : nullptr;
}
}

void UItemSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplySelectionVisual();
}

void UItemSlotWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	if (UInventorySlotViewData* SlotViewData = Cast<UInventorySlotViewData>(ListItemObject))
	{
		SetSlotData(SlotViewData);
	}
	else if (UItemInstance* ItemInstance = Cast<UItemInstance>(ListItemObject))
	{
		SetData(ItemInstance);
	}
}

void UItemSlotWidget::NativeOnItemSelectionChanged(const bool bInIsSelected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(bInIsSelected);

	SetSelected(bInIsSelected);
}

void UItemSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	if (UInfoWidget* InfoWidget = ResolveInfoWidgetFromSlot(this))
	{
		if (CachedData)
		{
			InfoWidget->ShowItemDetailAtWidget(CachedData, this, true);
		}
		else
		{
			InfoWidget->HideDetailWidgets();
		}
	}
}

void UItemSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	if (UInfoWidget* InfoWidget = ResolveInfoWidgetFromSlot(this))
	{
		InfoWidget->HideDetailWidgets();
	}

	Super::NativeOnMouseLeave(InMouseEvent);
}

FReply UItemSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (CachedData && InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UItemSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if (!CachedData)
	{
		return;
	}

	UItemSlotDragDropOperation* DragOperation = NewObject<UItemSlotDragDropOperation>(this);
	if (!DragOperation)
	{
		return;
	}

	const int32 SourceSlotIndex = CachedSlotData ? CachedSlotData->GetSlotIndex() : INDEX_NONE;
	DragOperation->Initialize(SourceSlotIndex, CachedData, CachedSlotData);
	DragOperation->Pivot = EDragPivot::CenterCenter;

	UTexture2D* IconTexture = Cast<UTexture2D>(CachedViewData.IconResource);
	if (IconTexture)
	{
		if (DragVisualWidgetClass)
		{
			UDragItemVisualWidget* DragVisualWidget = CreateWidget<UDragItemVisualWidget>(GetWorld(), DragVisualWidgetClass);
			if (DragVisualWidget)
			{
				DragVisualWidget->SetIconSize(DragIconSize);
				DragVisualWidget->SetIconTexture(IconTexture);
				DragOperation->DefaultDragVisual = DragVisualWidget;
			}
		}

		if (!DragOperation->DefaultDragVisual)
		{
			UImage* DragVisual = NewObject<UImage>(DragOperation);
			DragVisual->SetBrushFromTexture(IconTexture, true);
			DragVisual->SetDesiredSizeOverride(DragIconSize);
			DragOperation->DefaultDragVisual = DragVisual;
		}
	}

	OutOperation = DragOperation;
}

void UItemSlotWidget::SetData(UItemInstance* Target)
{
	CachedData = Target;
	CachedSlotData = nullptr;
	CachedViewData = FPdItemViewDataBuilder::FromItemInstance(Target);

	ApplyItemVisual(CachedViewData);
}

void UItemSlotWidget::SetSlotData(UInventorySlotViewData* Target)
{
	CachedSlotData = Target;
	CachedData = Target ? Target->GetItemInstance() : nullptr;
	CachedViewData = Target ? Target->GetViewData() : FPdItemViewData();

	ApplyItemVisual(CachedViewData);
}

void UItemSlotWidget::ApplyItemVisual(const FPdItemViewData& ViewData)
{
	CacheOptionalWidgets();

	if (IconImage)
	{
		IconImage->SetBrushResourceObject(ViewData.IconResource);
		IconImage->SetVisibility(ViewData.IconResource ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (TextBlock)
	{
		TextBlock->SetText(ViewData.DisplayName);
		TextBlock->SetVisibility(ViewData.IconResource ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	ApplySelectionVisual();
}

void UItemSlotWidget::SetSelected(const bool bInSelected)
{
	if (bIsSelected == bInSelected)
	{
		return;
	}

	bIsSelected = bInSelected;
	ApplySelectionVisual();
}

void UItemSlotWidget::CacheOptionalWidgets()
{
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("IconImage")));
	}
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("ItemIconImage")));
	}
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("SlotIconImage")));
	}
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("ItemIcon")));
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

void UItemSlotWidget::ApplySelectionVisual()
{
	CacheOptionalWidgets();

	if (!SelectionBorderImage)
	{
		return;
	}

	SelectionBorderImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SelectionBorderImage->SetColorAndOpacity(bIsSelected ? SelectionBorderSelectedColor : SelectionBorderDefaultColor);
}
