#include "UI/Widget/ItemSlotWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Component/Item/InventoryComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Mode/PdHUD.h"
#include "Mode/PdPlayerState.h"
#include "UI/Widget/InventorySlotViewData.h"
#include "UI/Widget/DragItemVisualWidget.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/ItemSlotDragDropOperation.h"
#include "UI/Widget/RightInventoryWidget.h"
#include "UI/WidgetLookup.h"

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
	if (CachedData && InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
	{
		RequestSplitCachedStack();
		return FReply::Handled();
	}

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
				DragVisualWidget->SetQuantity(IsCachedItemConsumable() ? CachedViewData.Quantity : 0);
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

bool UItemSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (RequestMergeDraggedStack(InOperation))
	{
		return true;
	}

	if (const UItemSlotDragDropOperation* ItemDragOperation = Cast<UItemSlotDragDropOperation>(InOperation))
	{
		const int32 SourceSlotIndex = ItemDragOperation->GetSourceSlotIndex();
		const int32 TargetSlotIndex = CachedSlotData ? CachedSlotData->GetSlotIndex() : INDEX_NONE;
		if (SourceSlotIndex != INDEX_NONE && TargetSlotIndex != INDEX_NONE)
		{
			if (UInfoWidget* InfoWidget = ResolveInfoWidgetFromSlot(this))
			{
				if (URightInventoryWidget* RightInventoryWidget = InfoWidget->GetRightInventoryWidget())
				{
					RightInventoryWidget->BroadcastDroppedInventorySlot(SourceSlotIndex, TargetSlotIndex, ItemDragOperation->GetItemInstance());
					return true;
				}
			}
		}
	}

	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UItemSlotWidget::SetData(UItemInstance* Target)
{
	CachedData = Target;
	CachedSlotData = nullptr;
	CachedViewData = FItemViewDataBuilder::FromItemInstance(Target);

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

	if (QuantityTextBlock)
	{
		const bool bShowQuantity = IsCachedItemConsumable() && ViewData.Quantity > 0 && ViewData.HasContent();
		QuantityTextBlock->SetText(FText::AsNumber(FMath::Max(0, ViewData.Quantity)));
		QuantityTextBlock->SetVisibility(bShowQuantity ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
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
		IconImage = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("IconImage"),
			TEXT("ItemIconImage"),
			TEXT("SlotIconImage"),
			TEXT("ItemIcon"),
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

bool UItemSlotWidget::IsItemConsumable(const UItemInstance* ItemInstance) const
{
	const UItemDefinition* ItemDefinition = IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
	const FGameplayTag ConsumableTypeTag = UProjectTagConfig::Get(this)->GetItemConsumableTypeTag();
	return ItemDefinition
		&& ItemDefinition->IsConsumableDefinition(ConsumableTypeTag);
}

bool UItemSlotWidget::IsCachedItemConsumable() const
{
	return IsItemConsumable(CachedData);
}

UInventoryComponent* UItemSlotWidget::ResolveOwningInventoryComponent() const
{
	const APlayerController* PlayerController = GetOwningPlayer();
	const APdPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<APdPlayerState>() : nullptr;
	return PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
}

bool UItemSlotWidget::RequestSplitCachedStack() const
{
	if (!CachedData)
	{
		return false;
	}

	if (!IsCachedItemConsumable() || CachedData->Quantity < 2)
	{

		return false;
	}

	UInventoryComponent* InventoryComponent = ResolveOwningInventoryComponent();
	if (!InventoryComponent)
	{

		return false;
	}

	const FGuid ItemId = CachedData->GetItemId();
	const bool bRequested = InventoryComponent->SplitConsumableStack(ItemId);

	return bRequested;
}

bool UItemSlotWidget::RequestMergeDraggedStack(UDragDropOperation* InOperation) const
{
	const UItemSlotDragDropOperation* ItemDragOperation = Cast<UItemSlotDragDropOperation>(InOperation);
	UItemInstance* SourceItem = ItemDragOperation ? ItemDragOperation->GetItemInstance() : nullptr;
	if (!SourceItem || !CachedData || SourceItem == CachedData)
	{
		return false;
	}

	const UItemDefinition* SourceDefinition = SourceItem->ItemDefinition.Get();
	const UItemDefinition* TargetDefinition = CachedData->ItemDefinition.Get();
	if (!SourceDefinition || SourceDefinition != TargetDefinition || !IsItemConsumable(SourceItem) || !IsCachedItemConsumable())
	{
		return false;
	}

	UInventoryComponent* InventoryComponent = ResolveOwningInventoryComponent();
	if (!InventoryComponent)
	{

		return false;
	}

	const FGuid SourceItemId = SourceItem->GetItemId();
	const FGuid TargetItemId = CachedData->GetItemId();
	const bool bRequested = InventoryComponent->MergeConsumableStacks(SourceItemId, TargetItemId);

	return bRequested;
}
