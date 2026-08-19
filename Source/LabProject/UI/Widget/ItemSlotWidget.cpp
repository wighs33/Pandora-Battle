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
#include "Definition/UI/WidgetClassDefinition.h"
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
	SetSelected(false);

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

	if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		if (CachedSlotData)
		{
			if (UInfoWidget* InfoWidget = ResolveInfoWidgetFromSlot(this))
			{
				if (URightInventoryWidget* RightInventoryWidget = InfoWidget->GetRightInventoryWidget())
				{
					RightInventoryWidget->SelectInventorySlot(CachedSlotData);
				}
			}
		}

		if (CachedData)
		{
			return UWidgetBlueprintLibrary::DetectDragIfPressed(
				InMouseEvent,
				this,
				EKeys::LeftMouseButton).NativeReply;
		}
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
	if (RequestMergeDraggedItem(InOperation))
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
	bDuplicateWeaponOrEquipment = false;

	ApplyItemVisual(CachedViewData);
}

void UItemSlotWidget::SetSlotData(UInventorySlotViewData* Target)
{
	CachedSlotData = Target;
	CachedData = Target ? Target->GetItemInstance() : nullptr;
	CachedViewData = Target ? Target->GetViewData() : FItemViewData();
	bDuplicateWeaponOrEquipment =
		Target && Target->IsDuplicateWeaponOrEquipment();

	ApplyItemVisual(CachedViewData);
}

void UItemSlotWidget::ApplyItemVisual(const FItemViewData& ViewData)
{
	CacheOptionalWidgets();
	const bool bAssigned = CachedSlotData && CachedSlotData->IsAssigned();

	if (IconImage)
	{
		IconImage->SetBrushResourceObject(ViewData.IconResource);
		IconImage->SetVisibility(ViewData.IconResource ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Img_Back)
	{
		Img_Back->SetColorAndOpacity(
			ViewData.HasContent()
				? ResolveBackgroundColor(bAssigned)
				: DefaultBackgroundColor);
	}

	if (Txt_Upgradeable)
	{
		Txt_Upgradeable->SetVisibility(
			bDuplicateWeaponOrEquipment && ViewData.HasContent()
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
	}

	if (Txt_Assigned)
	{
		Txt_Assigned->SetVisibility(
			bAssigned && ViewData.HasContent()
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
	}

	if (Txt_Quantity)
	{
		const bool bShowQuantity = IsCachedItemConsumable() && ViewData.Quantity > 0 && ViewData.HasContent();
		Txt_Quantity->SetText(FText::AsNumber(FMath::Max(0, ViewData.Quantity)));
		Txt_Quantity->SetVisibility(bShowQuantity ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Txt_Upgrade)
	{
		const int32 UpgradeLevel = FMath::Max(ViewData.UpgradeLevel, 0);
		Txt_Upgrade->SetText(FText::Format(
			NSLOCTEXT("ItemSlotWidget", "UpgradeLevelFormat", "+{0}"),
			FText::AsNumber(UpgradeLevel)));
		Txt_Upgrade->SetVisibility(
			UpgradeLevel > 0 && ViewData.HasContent()
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
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
	if (!Img_Back)
	{
		Img_Back = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("Img_Back"),
			TEXT("BackgroundImage"),
			TEXT("ItemBackgroundImage"),
			TEXT("SlotBackgroundImage")
		});
	}

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

	if (!Txt_Quantity)
	{
		Txt_Quantity = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("Txt_Quantity"),
			TEXT("Text_Quantity"),
			TEXT("QuantityText"),
			TEXT("ItemCountText")
		});
	}

	if (Img_Back && !bDefaultBackgroundColorCached)
	{
		DefaultBackgroundColor = Img_Back->GetColorAndOpacity();
		bDefaultBackgroundColorCached = true;
	}

	if (!Txt_Assigned)
	{
		Txt_Assigned = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("Txt_Assigned"),
			TEXT("AssignedText"),
			TEXT("AssignedTextBlock")
		});
	}

	if (!Txt_Upgrade)
	{
		Txt_Upgrade = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("Txt_Upgrade"),
			TEXT("UpgradeText"),
			TEXT("UpgradeTextBlock")
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

FLinearColor UItemSlotWidget::ResolveBackgroundColor(const bool bAssigned) const
{
	const UWidgetClassDefinition* WidgetDefinition =
		UWidgetClassDefinition::ResolveWidgetClassDefinition(this);
	const FInventoryWidgetSettings DefaultSettings;
	const FInventoryWidgetSettings& Settings = WidgetDefinition
		? WidgetDefinition->GetInventoryWidgetSettings()
		: DefaultSettings;

	if (bAssigned)
	{
		return Settings.AssignedItemBackgroundColor;
	}
	if (bDuplicateWeaponOrEquipment)
	{
		return Settings.UpgradeableItemBackgroundColor;
	}
	return DefaultBackgroundColor;
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

bool UItemSlotWidget::IsItemUpgradeable(const UItemInstance* ItemInstance) const
{
	const UItemDefinition* ItemDefinition = IsValid(ItemInstance)
		? ItemInstance->ItemDefinition.Get()
		: nullptr;
	if (!ItemDefinition)
	{
		return false;
	}

	const UProjectTagConfig* TagConfig = UProjectTagConfig::Get(this);
	return ItemDefinition->IsWeaponDefinition(TagConfig->GetItemWeaponTypeTag())
		|| ItemDefinition->MatchesItemType(TagConfig->GetItemEquipmentTypeTag());
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
	return InventoryComponent->SplitConsumableStack(ItemId);
}

bool UItemSlotWidget::RequestMergeDraggedItem(UDragDropOperation* InOperation) const
{
	const UItemSlotDragDropOperation* ItemDragOperation = Cast<UItemSlotDragDropOperation>(InOperation);
	UItemInstance* SourceItem = ItemDragOperation ? ItemDragOperation->GetItemInstance() : nullptr;
	if (!SourceItem || !CachedData || SourceItem == CachedData)
	{
		return false;
	}

	const UItemDefinition* SourceDefinition = SourceItem->ItemDefinition.Get();
	const UItemDefinition* TargetDefinition = CachedData->ItemDefinition.Get();
	if (!SourceDefinition || SourceDefinition != TargetDefinition)
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
	if (IsItemConsumable(SourceItem) && IsCachedItemConsumable())
	{
		return InventoryComponent->MergeConsumableStacks(SourceItemId, TargetItemId);
	}

	if (IsItemUpgradeable(SourceItem) && IsItemUpgradeable(CachedData))
	{
		return InventoryComponent->MergeUpgradeableItems(SourceItemId, TargetItemId);
	}

	return false;
}
