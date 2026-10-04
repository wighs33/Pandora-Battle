#include "UI/Info/Item/RightInventoryWidget.h"

#include "Components/TextBlock.h"
#include "Components/TileView.h"
#include "Definition/Common/ProjectTagDefinition.h"
#include "Definition/Item/ItemDefinition.h"
#include "UI/Core/WidgetClassDefinition.h"
#include "Item/ItemInstance.h"
#include "UI/Info/Item/InventorySlotViewData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RightInventoryWidget)

void URightInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (TileView)
	{
		TileView->SetSelectionMode(ESelectionMode::Single);
	}
	UpdateCombineMessage(false);
}

void URightInventoryWidget::ApplyWidgetDefinitionSettings()
{
	const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this);
	const FInventoryWidgetSettings* Settings = WidgetDefinition ? &WidgetDefinition->GetInventoryWidgetSettings() : nullptr;
	if (Settings)
	{
		InventorySlotCount = FMath::Max(Settings->GameInventoryItemCountLimit, 0);
	}

	AddTypeFilter(WeaponButton, Settings ? Settings->WeaponTypeTag : FGameplayTag(), &UProjectTagDefinition::GetItemWeaponTypeTag);
	AddTypeFilter(EquipmentButton, Settings ? Settings->EquipmentTypeTag : FGameplayTag(), &UProjectTagDefinition::GetItemEquipmentTypeTag);
	AddTypeFilter(ConsumableButton, Settings ? Settings->ConsumableTypeTag : FGameplayTag(), &UProjectTagDefinition::GetItemConsumableTypeTag);
	AddTypeFilter(ValuableButton, Settings ? Settings->ValuableTypeTag : FGameplayTag(), &UProjectTagDefinition::GetItemValuableTypeTag);
}

void URightInventoryWidget::SetAssignedItemIds(const TSet<FGuid>& InAssignedItemIds)
{
	AssignedItemIds = InAssignedItemIds;
}

void URightInventoryWidget::SelectInventorySlot(UInventorySlotViewData* SlotViewData)
{
	if (!TileView || !SlotViewData)
	{
		return;
	}

	// 인벤토리 칸은 드래그를 감지하므로, 아이템이 든 칸은 TileView의 기본 선택 처리보다 먼저
	// 마우스 누름을 가져갈 수 있다. 마지막으로 누른 칸만 강조되도록 선택을 직접 지운다.
	TileView->SetSelectionMode(ESelectionMode::Single);
	TileView->ClearSelection();
	TileView->SetSelectedItem(SlotViewData);
}

void URightInventoryWidget::SetInventorySlotCount(const int32 InInventorySlotCount)
{
	InventorySlotCount = FMath::Max(InInventorySlotCount, 0);
}

void URightInventoryWidget::BroadcastDroppedInventorySlot(const int32 SourceSlotIndex, const int32 TargetSlotIndex, UItemInstance* SourceItem)
{
	OnDropped_InventorySlot.Broadcast(SourceSlotIndex, TargetSlotIndex, SourceItem);
}

// 검색 중에는 맞는 아이템만, 아니면 빈 칸을 포함해 슬롯 수만큼 보여 준다.
void URightInventoryWidget::RebuildTileView()
{
	if (!TileView)
	{
		return;
	}

	TileView->ClearListItems();
	CachedSlotViewData.Reset();

	TMap<const UItemDefinition*, int32> DuplicateCandidateCounts;
	TSet<const UItemDefinition*> DefinitionsWithUnassignedItems;
	for (const TObjectPtr<UObject>& ListItem : CachedSourceListItems)
	{
		const UItemInstance* ItemInstance = Cast<UItemInstance>(ListItem.Get());
		const UItemDefinition* ItemDefinition = ItemInstance ? ItemInstance->ItemDefinition.Get() : nullptr;
		if (IsDuplicateHighlightCandidate(ItemDefinition))
		{
			++DuplicateCandidateCounts.FindOrAdd(ItemDefinition);
			if (!AssignedItemIds.Contains(ItemInstance->GetItemId()))
			{
				DefinitionsWithUnassignedItems.Add(ItemDefinition);
			}
		}
	}

	bool bHasCombinableItems = false;
	for (const TPair<const UItemDefinition*, int32>& DuplicateCandidate : DuplicateCandidateCounts)
	{
		// 이미 배치된 아이템 두 개를 합치는 요청은 인벤토리 컴포넌트가 거절한다.
		if (DuplicateCandidate.Value > 1 && DefinitionsWithUnassignedItems.Contains(DuplicateCandidate.Key))
		{
			bHasCombinableItems = true;
			break;
		}
	}
	UpdateCombineMessage(bHasCombinableItems);

	const bool bUseSearch = IsSearching();
	const int32 SlotCountToDisplay = bUseSearch ? CachedSourceListItems.Num() : FMath::Max(InventorySlotCount, CachedSourceListItems.Num());
	CachedSlotViewData.Reserve(SlotCountToDisplay);

	for (int32 SlotIndex = 0; SlotIndex < SlotCountToDisplay; ++SlotIndex)
	{
		UItemInstance* ItemInstance = CachedSourceListItems.IsValidIndex(SlotIndex) ? Cast<UItemInstance>(CachedSourceListItems[SlotIndex].Get()) : nullptr;
		const UItemDefinition* ItemDefinition = IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
		if (bUseSearch && !MatchesSearch(ItemDefinition, ItemDefinition ? ItemDefinition->DisplayName : FText::GetEmpty(), ActiveSearchText))
		{
			continue;
		}

		const FGuid ItemId = ItemInstance ? ItemInstance->GetItemId() : FGuid();
		const int32* DuplicateCount = ItemDefinition ? DuplicateCandidateCounts.Find(ItemDefinition) : nullptr;
		UInventorySlotViewData* SlotViewData = NewObject<UInventorySlotViewData>(this);
		SlotViewData->Initialize(SlotIndex, ItemInstance,
			DuplicateCount && *DuplicateCount > 1 && DefinitionsWithUnassignedItems.Contains(ItemDefinition),
			ItemId.IsValid() && AssignedItemIds.Contains(ItemId));
		CachedSlotViewData.Add(SlotViewData);
		TileView->AddItem(SlotViewData);
	}
}

void URightInventoryWidget::UpdateCombineMessage(const bool bHasCombinableItems) const
{
	if (!Txt_Message)
	{
		return;
	}

	Txt_Message->SetText(MenuTextOrFallback(TEXT("Info.Combine"), NSLOCTEXT("RightInventoryWidget",
		"CombineDuplicateItemsMessage", "Combine duplicate weapons or equipment to upgrade them.")));
	Txt_Message->SetVisibility(
		bHasCombinableItems
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
}

bool URightInventoryWidget::IsDuplicateHighlightCandidate(const UItemDefinition* ItemDefinition) const
{
	return ItemDefinition && (ItemDefinition->IsWeaponDefinition(GetTypeFilterTag(WeaponFilterIndex))
		|| ItemDefinition->MatchesItemType(GetTypeFilterTag(EquipmentFilterIndex)));
}
