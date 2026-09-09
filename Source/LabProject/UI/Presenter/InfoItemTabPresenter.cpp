#include "UI/Presenter/InfoItemTabPresenter.h"

#include "Components/TileView.h"
#include "Component/Item/InventoryComponent.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Definition/Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "UI/InfoLoadoutStore.h"
#include "UI/PandoraLoadoutUiModel.h"
#include "UI/Widget/EquipSlotWidget.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/InventorySlotViewData.h"
#include "UI/Widget/LeftEquipmentWidget.h"
#include "UI/Widget/RightInventoryWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InfoItemTabPresenter)

namespace
{
void AppendItemListAsObjects(const FItemList& ItemList, TArray<UObject*>& OutListItems)
{
	OutListItems.Reserve(OutListItems.Num() + ItemList.Items.Num());
	for (const TObjectPtr<UItemInstance>& Item : ItemList.Items)
	{
		if (UItemInstance* ItemInstance = Item.Get())
		{
			OutListItems.Add(ItemInstance);
		}
	}
}

FString GetItemDisplayNameForSort(const UItemInstance* ItemInstance)
{
	const UItemDefinition* ItemDefinition = IsValid(ItemInstance)
		? ItemInstance->ItemDefinition.Get()
		: nullptr;
	if (IsValid(ItemDefinition) && !ItemDefinition->DisplayName.IsEmpty())
	{
		return ItemDefinition->DisplayName.ToString();
	}

	return IsValid(ItemDefinition) ? ItemDefinition->GetName() : GetNameSafe(ItemInstance);
}

void SortItemObjectsByDisplayName(TArray<UObject*>& InOutItems)
{
	InOutItems.StableSort([](const UObject& LeftObject, const UObject& RightObject)
	{
		const UItemInstance* LeftItem = Cast<UItemInstance>(&LeftObject);
		const UItemInstance* RightItem = Cast<UItemInstance>(&RightObject);
		const FString LeftName = GetItemDisplayNameForSort(LeftItem);
		const FString RightName = GetItemDisplayNameForSort(RightItem);

		const int32 NameCompare = LeftName.Compare(RightName, ESearchCase::IgnoreCase);
		if (NameCompare != 0)
		{
			return NameCompare < 0;
		}

		const UObject* LeftTieObject = LeftItem && LeftItem->ItemDefinition
			? LeftItem->ItemDefinition.Get()
			: &LeftObject;
		const UObject* RightTieObject = RightItem && RightItem->ItemDefinition
			? RightItem->ItemDefinition.Get()
			: &RightObject;
		return GetNameSafe(LeftTieObject) < GetNameSafe(RightTieObject);
	});
}
}

void UInfoItemTabPresenter::BindInfoUi(UInfoWidget* InInfoWidget)
{
	if (GetInfoWidget() != InInfoWidget)
	{
		UnbindEvents();
	}

	Super::BindInfoUi(InInfoWidget);
	BindEvents();
}

void UInfoItemTabPresenter::Deinitialize()
{
	UnbindEvents();
	SelectedEquipSlot = nullptr;
	SelectedEquipTypeTag = FGameplayTag();
	CurrentItemFilterTag = FGameplayTag();
	bUseItemTypeFilter = false;
	bActive = false;
	LoadoutStore.Reset();
	ResetInventoryDisplaySlots();
	Super::Deinitialize();
}

void UInfoItemTabPresenter::SetLoadoutStore(UInfoLoadoutStore* InLoadoutStore)
{
	LoadoutStore = InLoadoutStore;
}

void UInfoItemTabPresenter::SetActive(const bool bInActive)
{
	bActive = bInActive;
	if (!bActive)
	{
		ClearInventoryTileItemClicked();
		SelectedEquipSlot = nullptr;
		SelectedEquipTypeTag = FGameplayTag();
	}
}

void UInfoItemTabPresenter::Activate()
{
	SetActive(true);
	bUseItemTypeFilter = false;
	CurrentItemFilterTag = FGameplayTag();
	BindEvents();

	UInfoWidget* InfoWidget = GetInfoWidget();
	if (!InfoWidget)
	{
		return;
	}

	if (URightInventoryWidget* RightInventoryWidget = InfoWidget->GetRightInventoryWidget())
	{
		RightInventoryWidget->ToggleActiveFiliterButtons(true);
	}
	if (ULeftEquipmentWidget* LeftEquipmentWidget = InfoWidget->GetLeftEquipmentWidget())
	{
		LeftEquipmentWidget->InitialzeEquipSlots();
	}

	RefreshEquipmentSlots();
	RefreshInventoryTileView();
}

void UInfoItemTabPresenter::HandleInfoUiOpened()
{
	BindEvents();
	UInfoWidget* InfoWidget = GetInfoWidget();
	if (ULeftEquipmentWidget* LeftEquipmentWidget = InfoWidget ? InfoWidget->GetLeftEquipmentWidget() : nullptr)
	{
		LeftEquipmentWidget->InitialzeEquipSlots();
	}
	RefreshEquipmentSlots();

	if (URightInventoryWidget* RightInventoryWidget = InfoWidget ? InfoWidget->GetRightInventoryWidget() : nullptr)
	{
		RightInventoryWidget->ToggleActiveFiliterButtons(true);
	}
	RefreshInventoryTileView();
}

void UInfoItemTabPresenter::HandleInventoryChanged()
{
	RefreshEquipmentSlots();
	RefreshInventoryTileView();
}

void UInfoItemTabPresenter::HandleWeaponLoadoutChanged()
{
	RefreshEquipmentSlots();
	RefreshInventoryTileView();
}

void UInfoItemTabPresenter::HandlePandoraLoadoutChanged()
{
	RefreshEquipmentSlots();
}

void UInfoItemTabPresenter::HandlePresentationAssetsReady()
{
	RefreshEquipmentSlots();
	RefreshInventoryTileView();
}

void UInfoItemTabPresenter::ResetInventoryDisplaySlots()
{
	InventoryDisplaySlots.Reset();
	CachedInventoryViewSlots.Reset();
	bInventoryDisplaySlotsInitialized = false;
}

UItemInstance* UInfoItemTabPresenter::GetSelectedWeapon(const EEnum_Direction Direction) const
{
	const UInfoLoadoutStore* Store = LoadoutStore.Get();
	return Store ? Store->GetSelectedWeapon(Direction) : nullptr;
}

void UInfoItemTabPresenter::RefreshEquipmentSlots() const
{
	UInfoWidget* InfoWidget = GetInfoWidget();
	ULeftEquipmentWidget* LeftEquipmentWidget = InfoWidget ? InfoWidget->GetLeftEquipmentWidget() : nullptr;
	if (!LeftEquipmentWidget)
	{
		return;
	}

	LeftEquipmentWidget->SetWeaponSlotData(1, GetSelectedWeapon(EEnum_Direction::Left));
	LeftEquipmentWidget->SetWeaponSlotData(2, GetSelectedWeapon(EEnum_Direction::Up));
	LeftEquipmentWidget->SetWeaponSlotData(3, GetSelectedWeapon(EEnum_Direction::Right));

	const UInfoLoadoutStore* Store = LoadoutStore.Get();
	LeftEquipmentWidget->SetWeaponSlotPandoraRequirement(
		1,
		Store ? Store->GetSelectedPandoraDefinition(EEnum_Direction::Left) : nullptr);
	LeftEquipmentWidget->SetWeaponSlotPandoraRequirement(
		2,
		Store ? Store->GetSelectedPandoraDefinition(EEnum_Direction::Up) : nullptr);
	LeftEquipmentWidget->SetWeaponSlotPandoraRequirement(
		3,
		Store ? Store->GetSelectedPandoraDefinition(EEnum_Direction::Right) : nullptr);

	const UInventoryComponent* Inventory = Store ? Store->GetInventoryComponent() : nullptr;
	const UProjectTagConfig* TagConfig = UProjectTagConfig::Get(this);
	TArray<FGameplayTag> EquipmentSlotTags;
	TagConfig->GetItemEquipmentSlotTags(EquipmentSlotTags);
	for (const FGameplayTag& EquipmentSlotTag : EquipmentSlotTags)
	{
		LeftEquipmentWidget->SetEquipmentSlotData(
			EquipmentSlotTag,
			Inventory ? Inventory->GetEquipmentSlotItem(EquipmentSlotTag) : nullptr);
	}
	for (int32 SlotIndex = 0; SlotIndex < UInventoryComponent::ConsumableQuickSlotCount; ++SlotIndex)
	{
		LeftEquipmentWidget->SetConsumableQuickSlotData(
			SlotIndex + 1,
			Inventory ? Inventory->GetConsumableQuickSlotItem(SlotIndex) : nullptr);
	}
}

void UInfoItemTabPresenter::HandleItemSlotClicked(UObject* Item)
{
	UInventorySlotViewData* SlotViewData = Cast<UInventorySlotViewData>(Item);
	UItemInstance* ItemInstance = Cast<UItemInstance>(Item);
	if (!ItemInstance && SlotViewData)
	{
		ItemInstance = SlotViewData->GetItemInstance();
	}

	const UItemDefinition* ItemDefinition = IsValid(ItemInstance)
		? ItemInstance->ItemDefinition.Get()
		: nullptr;
	if (!GetController()
		|| !ItemDefinition
		|| !ItemDefinition->IdTag.IsValid()
		|| !SelectedEquipSlot
		|| !SelectedEquipTypeTag.IsValid()
		|| !ItemDefinition->IdTag.MatchesTag(SelectedEquipTypeTag))
	{
		return;
	}

	UInfoLoadoutStore* Store = LoadoutStore.Get();
	const bool bConsumableSlot = GetConsumableItemTypeTag().IsValid()
		&& SelectedEquipTypeTag.MatchesTag(GetConsumableItemTypeTag());
	if (bConsumableSlot)
	{
		if (Store
			&& Store->RequestSetConsumableQuickSlot(
				SelectedEquipSlot->GetNth() - 1,
				ItemInstance))
		{
			ClearInventoryTileItemClicked();
		}
		return;
	}

	const bool bWeaponSlot = GetWeaponItemTypeTag().IsValid()
		&& SelectedEquipTypeTag.MatchesTag(GetWeaponItemTypeTag());
	if (bWeaponSlot)
	{
		const EEnum_Direction Direction =
			FPandoraLoadoutUiModel::GetDirectionFromSelectSlotNumber(SelectedEquipSlot->GetNth());
		if (!Store || !Store->RequestSetWeaponLoadoutSlot(Direction, ItemInstance))
		{
			return;
		}

		ClearInventoryTileItemClicked();
		return;
	}

	const bool bEquipmentSlot = GetEquipmentItemTypeTag().IsValid()
		&& SelectedEquipTypeTag.MatchesTag(GetEquipmentItemTypeTag());
	if (bEquipmentSlot)
	{
		if (!Store
			|| !Store->RequestSetEquipmentSlot(SelectedEquipTypeTag, ItemInstance))
		{
			return;
		}

		ClearInventoryTileItemClicked();
		return;
	}

	SelectedEquipSlot->SetData(ItemInstance);
	ClearInventoryTileItemClicked();
	RefreshInventoryTileView();
}

void UInfoItemTabPresenter::HandleItemEquipSlotClicked(
	FGameplayTag EquipTypeTag,
	UEquipSlotWidget* InSelectedEquipSlot,
	const bool bIsSelectedAnyButton)
{
	static_cast<void>(bIsSelectedAnyButton);
	if (!GetController())
	{
		return;
	}

	ClearInventoryTileItemClicked();
	SelectedEquipSlot = nullptr;
	SelectedEquipTypeTag = FGameplayTag();
	if (!InSelectedEquipSlot || !EquipTypeTag.IsValid())
	{
		return;
	}

	if (InSelectedEquipSlot->HasEquippedItem())
	{
		ClearEquipmentSlot(InSelectedEquipSlot, EquipTypeTag);
		return;
	}

	SelectedEquipSlot = InSelectedEquipSlot;
	SelectedEquipTypeTag = EquipTypeTag;
	HandleItemFilterTypeClicked(EquipTypeTag);
	BindInventoryTileItemClicked();
}

void UInfoItemTabPresenter::HandleItemEquipSlotDropped(
	const FGameplayTag EquipTypeTag,
	UEquipSlotWidget* TargetEquipSlot,
	UItemInstance* ItemInstance)
{
	SelectedEquipSlot = TargetEquipSlot;
	SelectedEquipTypeTag = EquipTypeTag;
	HandleItemSlotClicked(ItemInstance);
}

void UInfoItemTabPresenter::HandleItemDroppedToCharacter(UItemInstance* ItemInstance)
{
	UInfoWidget* InfoWidget = GetInfoWidget();
	ULeftEquipmentWidget* LeftEquipmentWidget = InfoWidget ? InfoWidget->GetLeftEquipmentWidget() : nullptr;
	UEquipSlotWidget* TargetSlot = LeftEquipmentWidget
		? LeftEquipmentWidget->FindFirstCompatibleEquipSlot(ItemInstance)
		: nullptr;
	if (!TargetSlot)
	{
		return;
	}

	SelectedEquipSlot = TargetSlot;
	SelectedEquipTypeTag = TargetSlot->GetAcceptedEquipTypeTag();
	HandleItemSlotClicked(ItemInstance);
}

void UInfoItemTabPresenter::HandleInventorySlotDropped(
	const int32 SourceSlotIndex,
	const int32 TargetSlotIndex,
	UItemInstance* SourceItem)
{
	if (!IsValid(SourceItem) || SourceSlotIndex == TargetSlotIndex)
	{
		return;
	}

	SourceItem->EnsureItemId();
	const FGuid SourceItemId = SourceItem->GetItemId();
	const int32 SourceDisplayIndex = FindInventoryDisplaySlotIndexByItemId(SourceItemId);
	if (!SourceItemId.IsValid() || SourceDisplayIndex == INDEX_NONE)
	{
		return;
	}

	int32 TargetDisplayIndex = INDEX_NONE;
	UItemInstance* TargetItem = CachedInventoryViewSlots.IsValidIndex(TargetSlotIndex)
		? CachedInventoryViewSlots[TargetSlotIndex].Get()
		: nullptr;
	if (bUseItemTypeFilter)
	{
		if (!IsValid(TargetItem))
		{
			return;
		}
		TargetItem->EnsureItemId();
		TargetDisplayIndex = FindInventoryDisplaySlotIndexByItemId(TargetItem->GetItemId());
	}
	else
	{
		TargetDisplayIndex = TargetSlotIndex;
	}

	if (TargetDisplayIndex == INDEX_NONE)
	{
		return;
	}

	const int32 RequiredSlotCount = FMath::Max(SourceDisplayIndex, TargetDisplayIndex) + 1;
	if (InventoryDisplaySlots.Num() < RequiredSlotCount)
	{
		InventoryDisplaySlots.SetNum(RequiredSlotCount);
	}

	UItemInstance* PreviousTargetItem = InventoryDisplaySlots[TargetDisplayIndex].Get();
	InventoryDisplaySlots[TargetDisplayIndex] = SourceItem;
	InventoryDisplaySlots[SourceDisplayIndex] = PreviousTargetItem;
	RefreshInventoryTileView();
}

void UInfoItemTabPresenter::HandleItemFilterTypeClicked(const FGameplayTag TypeTag)
{
	if (!TypeTag.IsValid())
	{
		bUseItemTypeFilter = false;
		CurrentItemFilterTag = FGameplayTag();
	}
	else
	{
		bUseItemTypeFilter = true;
		CurrentItemFilterTag = TypeTag;
	}
	RefreshInventoryTileView();
}

void UInfoItemTabPresenter::HandleItemFilterAllClicked()
{
	bUseItemTypeFilter = false;
	CurrentItemFilterTag = FGameplayTag();
	RefreshInventoryTileView();
}

void UInfoItemTabPresenter::RefreshInventoryTileView()
{
	if (!bActive)
	{
		return;
	}

	UInfoWidget* InfoWidget = GetInfoWidget();
	URightInventoryWidget* RightInventoryWidget = InfoWidget
		? InfoWidget->GetRightInventoryWidget()
		: nullptr;
	if (!RightInventoryWidget || !InfoWidget->IsInViewport())
	{
		return;
	}

	TArray<UObject*> AllInventoryItems;
	TArray<UObject*> CurrentItemList;
	const UInfoLoadoutStore* Store = LoadoutStore.Get();
	const UInventoryComponent* Inventory = Store ? Store->GetInventoryComponent() : nullptr;
	if (Inventory)
	{
		AppendItemListAsObjects(Inventory->GetAllItems(), AllInventoryItems);
		const bool bCanUseTypeFilter = bUseItemTypeFilter
			&& CurrentItemFilterTag.IsValid()
			&& !Inventory->GetFilteredItemMap().IsEmpty();
		if (bCanUseTypeFilter)
		{
			for (UObject* ItemObject : AllInventoryItems)
			{
				UItemInstance* ItemInstance = Cast<UItemInstance>(ItemObject);
				const UItemDefinition* ItemDefinition = IsValid(ItemInstance)
					? ItemInstance->ItemDefinition.Get()
					: nullptr;
				if (ItemDefinition && ItemDefinition->IdTag.MatchesTag(CurrentItemFilterTag))
				{
					CurrentItemList.Add(ItemInstance);
				}
			}
		}
		else
		{
			CurrentItemList = AllInventoryItems;
		}
	}

	ReconcileInventoryDisplaySlots(AllInventoryItems);
	if (!bUseItemTypeFilter)
	{
		CurrentItemList = AllInventoryItems;
	}

	TSet<FGuid> AssignedItemIds;
	CollectAssignedItemIds(AssignedItemIds);

	TArray<UObject*> ViewSlotItems;
	BuildInventoryViewSlots(CurrentItemList, ViewSlotItems);
	RightInventoryWidget->SetAssignedItemIds(AssignedItemIds);
	RightInventoryWidget->SetTileView(ViewSlotItems);
}

void UInfoItemTabPresenter::BindEvents()
{
	UInfoWidget* InfoWidget = GetInfoWidget();
	if (!InfoWidget)
	{
		return;
	}

	InfoWidget->OnDroppedItemToCharacterPanel.RemoveDynamic(
		this,
		&ThisClass::HandleItemDroppedToCharacter);
	InfoWidget->OnDroppedItemToCharacterPanel.AddUniqueDynamic(
		this,
		&ThisClass::HandleItemDroppedToCharacter);

	if (ULeftEquipmentWidget* LeftEquipmentWidget = InfoWidget->GetLeftEquipmentWidget())
	{
		LeftEquipmentWidget->OnClicked_EquipTypeSlot.RemoveDynamic(
			this,
			&ThisClass::HandleItemEquipSlotClicked);
		LeftEquipmentWidget->OnClicked_EquipTypeSlot.AddUniqueDynamic(
			this,
			&ThisClass::HandleItemEquipSlotClicked);
		LeftEquipmentWidget->OnDroppedItem_EquipTypeSlot.RemoveDynamic(
			this,
			&ThisClass::HandleItemEquipSlotDropped);
		LeftEquipmentWidget->OnDroppedItem_EquipTypeSlot.AddUniqueDynamic(
			this,
			&ThisClass::HandleItemEquipSlotDropped);
	}

	if (URightInventoryWidget* RightInventoryWidget = InfoWidget->GetRightInventoryWidget())
	{
		RightInventoryWidget->OnClicked_FilterAllButton.RemoveDynamic(
			this,
			&ThisClass::HandleItemFilterAllClicked);
		RightInventoryWidget->OnClicked_FilterAllButton.AddUniqueDynamic(
			this,
			&ThisClass::HandleItemFilterAllClicked);
		RightInventoryWidget->OnClicked_FilterTypeButton.RemoveDynamic(
			this,
			&ThisClass::HandleItemFilterTypeClicked);
		RightInventoryWidget->OnClicked_FilterTypeButton.AddUniqueDynamic(
			this,
			&ThisClass::HandleItemFilterTypeClicked);
		RightInventoryWidget->OnDropped_InventorySlot.RemoveDynamic(
			this,
			&ThisClass::HandleInventorySlotDropped);
		RightInventoryWidget->OnDropped_InventorySlot.AddUniqueDynamic(
			this,
			&ThisClass::HandleInventorySlotDropped);
	}
}

void UInfoItemTabPresenter::UnbindEvents()
{
	ClearInventoryTileItemClicked();
	UInfoWidget* InfoWidget = GetInfoWidget();
	if (!InfoWidget)
	{
		return;
	}

	InfoWidget->OnDroppedItemToCharacterPanel.RemoveDynamic(
		this,
		&ThisClass::HandleItemDroppedToCharacter);
	if (ULeftEquipmentWidget* LeftEquipmentWidget = InfoWidget->GetLeftEquipmentWidget())
	{
		LeftEquipmentWidget->OnClicked_EquipTypeSlot.RemoveDynamic(
			this,
			&ThisClass::HandleItemEquipSlotClicked);
		LeftEquipmentWidget->OnDroppedItem_EquipTypeSlot.RemoveDynamic(
			this,
			&ThisClass::HandleItemEquipSlotDropped);
	}
	if (URightInventoryWidget* RightInventoryWidget = InfoWidget->GetRightInventoryWidget())
	{
		RightInventoryWidget->OnClicked_FilterAllButton.RemoveDynamic(
			this,
			&ThisClass::HandleItemFilterAllClicked);
		RightInventoryWidget->OnClicked_FilterTypeButton.RemoveDynamic(
			this,
			&ThisClass::HandleItemFilterTypeClicked);
		RightInventoryWidget->OnDropped_InventorySlot.RemoveDynamic(
			this,
			&ThisClass::HandleInventorySlotDropped);
	}
}

void UInfoItemTabPresenter::BindInventoryTileItemClicked()
{
	UInfoWidget* InfoWidget = GetInfoWidget();
	URightInventoryWidget* RightInventoryWidget = InfoWidget
		? InfoWidget->GetRightInventoryWidget()
		: nullptr;
	if (UTileView* TileView = RightInventoryWidget ? RightInventoryWidget->GetTileView() : nullptr)
	{
		TileView->OnItemClicked().RemoveAll(this);
		TileView->OnItemClicked().AddUObject(this, &ThisClass::HandleItemSlotClicked);
	}
}

void UInfoItemTabPresenter::ClearInventoryTileItemClicked() const
{
	UInfoWidget* InfoWidget = GetInfoWidget();
	URightInventoryWidget* RightInventoryWidget = InfoWidget
		? InfoWidget->GetRightInventoryWidget()
		: nullptr;
	if (UTileView* TileView = RightInventoryWidget ? RightInventoryWidget->GetTileView() : nullptr)
	{
		TileView->OnItemClicked().RemoveAll(this);
	}
}

void UInfoItemTabPresenter::ClearEquipmentSlot(
	UEquipSlotWidget* TargetEquipSlot,
	FGameplayTag EquipTypeTag)
{
	if (!TargetEquipSlot)
	{
		return;
	}
	if (!EquipTypeTag.IsValid())
	{
		EquipTypeTag = TargetEquipSlot->GetAcceptedEquipTypeTag();
	}

	UInfoLoadoutStore* Store = LoadoutStore.Get();
	if (GetConsumableItemTypeTag().IsValid()
		&& EquipTypeTag.MatchesTag(GetConsumableItemTypeTag()))
	{
		if (!Store || !Store->RequestClearConsumableQuickSlot(TargetEquipSlot->GetNth() - 1))
		{
			return;
		}
		SelectedEquipSlot = nullptr;
		SelectedEquipTypeTag = FGameplayTag();
		return;
	}

	if (GetWeaponItemTypeTag().IsValid() && EquipTypeTag.MatchesTag(GetWeaponItemTypeTag()))
	{
		const EEnum_Direction Direction =
			FPandoraLoadoutUiModel::GetDirectionFromSelectSlotNumber(TargetEquipSlot->GetNth());
		if (!Store || !Store->RequestClearWeaponLoadoutSlot(Direction))
		{
			return;
		}
		SelectedEquipSlot = nullptr;
		SelectedEquipTypeTag = FGameplayTag();
		return;
	}

	if (GetEquipmentItemTypeTag().IsValid()
		&& EquipTypeTag.MatchesTag(GetEquipmentItemTypeTag()))
	{
		if (!Store || !Store->RequestClearEquipmentSlot(EquipTypeTag))
		{
			return;
		}
		SelectedEquipSlot = nullptr;
		SelectedEquipTypeTag = FGameplayTag();
		return;
	}

	TargetEquipSlot->SetData(nullptr);
	if (SelectedEquipSlot == TargetEquipSlot)
	{
		SelectedEquipSlot = nullptr;
		SelectedEquipTypeTag = FGameplayTag();
	}
	RefreshInventoryTileView();
}

void UInfoItemTabPresenter::ReconcileInventoryDisplaySlots(const TArray<UObject*>& InventoryItems)
{
	TMap<FGuid, UItemInstance*> CurrentItemsById;
	CurrentItemsById.Reserve(InventoryItems.Num());
	for (UObject* ItemObject : InventoryItems)
	{
		UItemInstance* ItemInstance = Cast<UItemInstance>(ItemObject);
		if (!IsValid(ItemInstance))
		{
			continue;
		}
		ItemInstance->EnsureItemId();
		if (const FGuid ItemId = ItemInstance->GetItemId(); ItemId.IsValid())
		{
			CurrentItemsById.Add(ItemId, ItemInstance);
		}
	}

	if (!bInventoryDisplaySlotsInitialized)
	{
		TArray<UObject*> SortedItems = InventoryItems;
		SortItemObjectsByDisplayName(SortedItems);
		InventoryDisplaySlots.Reset(SortedItems.Num());
		for (UObject* SortedObject : SortedItems)
		{
			if (UItemInstance* ItemInstance = Cast<UItemInstance>(SortedObject))
			{
				InventoryDisplaySlots.Add(ItemInstance);
			}
		}
		bInventoryDisplaySlotsInitialized = true;
		return;
	}

	TSet<FGuid> ExistingSlotIds;
	for (TObjectPtr<UItemInstance>& SlotItem : InventoryDisplaySlots)
	{
		UItemInstance* ItemInstance = SlotItem.Get();
		if (!IsValid(ItemInstance))
		{
			SlotItem = nullptr;
			continue;
		}

		ItemInstance->EnsureItemId();
		const FGuid ItemId = ItemInstance->GetItemId();
		if (!ItemId.IsValid() || !CurrentItemsById.Contains(ItemId))
		{
			SlotItem = nullptr;
			continue;
		}
		ExistingSlotIds.Add(ItemId);
	}

	TArray<UObject*> NewItems;
	for (UObject* ItemObject : InventoryItems)
	{
		UItemInstance* ItemInstance = Cast<UItemInstance>(ItemObject);
		if (!IsValid(ItemInstance))
		{
			continue;
		}
		ItemInstance->EnsureItemId();
		const FGuid ItemId = ItemInstance->GetItemId();
		if (ItemId.IsValid() && !ExistingSlotIds.Contains(ItemId))
		{
			NewItems.Add(ItemInstance);
		}
	}
	SortItemObjectsByDisplayName(NewItems);

	for (UObject* NewItemObject : NewItems)
	{
		UItemInstance* NewItem = Cast<UItemInstance>(NewItemObject);
		if (!IsValid(NewItem))
		{
			continue;
		}

		const int32 EmptySlotIndex = InventoryDisplaySlots.IndexOfByPredicate(
			[](const TObjectPtr<UItemInstance>& SlotItem)
			{
				return !SlotItem.Get();
			});
		if (EmptySlotIndex == INDEX_NONE)
		{
			InventoryDisplaySlots.Add(NewItem);
		}
		else
		{
			InventoryDisplaySlots[EmptySlotIndex] = NewItem;
		}
	}
}

void UInfoItemTabPresenter::BuildInventoryViewSlots(
	const TArray<UObject*>& SourceItems,
	TArray<UObject*>& OutViewItems)
{
	OutViewItems.Reset();
	CachedInventoryViewSlots.Reset();

	TSet<FGuid> SourceItemIds;
	SourceItemIds.Reserve(SourceItems.Num());
	for (UObject* SourceObject : SourceItems)
	{
		UItemInstance* ItemInstance = Cast<UItemInstance>(SourceObject);
		if (!IsValid(ItemInstance))
		{
			continue;
		}
		ItemInstance->EnsureItemId();
		if (const FGuid ItemId = ItemInstance->GetItemId(); ItemId.IsValid())
		{
			SourceItemIds.Add(ItemId);
		}
	}

	TSet<FGuid> AddedItemIds;
	for (const TObjectPtr<UItemInstance>& SlotItemPtr : InventoryDisplaySlots)
	{
		UItemInstance* SlotItem = SlotItemPtr.Get();
		UItemInstance* VisibleItem = nullptr;
		if (IsValid(SlotItem))
		{
			SlotItem->EnsureItemId();
			const FGuid SlotItemId = SlotItem->GetItemId();
			if (SlotItemId.IsValid() && SourceItemIds.Contains(SlotItemId))
			{
				VisibleItem = SlotItem;
				AddedItemIds.Add(SlotItemId);
			}
		}

		if (bUseItemTypeFilter)
		{
			if (VisibleItem)
			{
				OutViewItems.Add(VisibleItem);
				CachedInventoryViewSlots.Add(VisibleItem);
			}
		}
		else
		{
			OutViewItems.Add(VisibleItem);
			CachedInventoryViewSlots.Add(VisibleItem);
		}
	}

	for (UObject* SourceObject : SourceItems)
	{
		UItemInstance* ItemInstance = Cast<UItemInstance>(SourceObject);
		if (!IsValid(ItemInstance))
		{
			continue;
		}
		ItemInstance->EnsureItemId();
		const FGuid ItemId = ItemInstance->GetItemId();
		if (ItemId.IsValid() && !AddedItemIds.Contains(ItemId))
		{
			OutViewItems.Add(ItemInstance);
			CachedInventoryViewSlots.Add(ItemInstance);
			AddedItemIds.Add(ItemId);
		}
	}
}

int32 UInfoItemTabPresenter::FindInventoryDisplaySlotIndexByItemId(const FGuid ItemId) const
{
	if (!ItemId.IsValid())
	{
		return INDEX_NONE;
	}
	for (int32 SlotIndex = 0; SlotIndex < InventoryDisplaySlots.Num(); ++SlotIndex)
	{
		const UItemInstance* ItemInstance = InventoryDisplaySlots[SlotIndex].Get();
		if (IsValid(ItemInstance) && ItemInstance->GetItemId() == ItemId)
		{
			return SlotIndex;
		}
	}
	return INDEX_NONE;
}

void UInfoItemTabPresenter::CollectAssignedItemIds(TSet<FGuid>& OutAssignedItemIds) const
{
	OutAssignedItemIds.Reset();
	const UInfoLoadoutStore* Store = LoadoutStore.Get();
	const UInventoryComponent* Inventory = Store ? Store->GetInventoryComponent() : nullptr;

	if (Inventory)
	{
		const UProjectTagConfig* TagConfig = UProjectTagConfig::Get(this);
		TArray<FGameplayTag> EquipmentSlotTags;
		TagConfig->GetItemEquipmentSlotTags(EquipmentSlotTags);
		for (const FGameplayTag& EquipmentSlotTag : EquipmentSlotTags)
		{
			const FGuid EquippedItemId =
				Inventory->GetEquipmentSlotItemId(EquipmentSlotTag);
			if (EquippedItemId.IsValid())
			{
				OutAssignedItemIds.Add(EquippedItemId);
			}
		}
		for (int32 SlotIndex = 0; SlotIndex < UInventoryComponent::ConsumableQuickSlotCount; ++SlotIndex)
		{
			const UItemInstance* QuickSlotItem = Inventory->GetConsumableQuickSlotItem(SlotIndex);
			if (IsValid(QuickSlotItem) && QuickSlotItem->GetItemId().IsValid())
			{
				OutAssignedItemIds.Add(QuickSlotItem->GetItemId());
			}
		}
		for (const EEnum_Direction Direction :
			{ EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right })
		{
			const FGuid WeaponItemId = Inventory->GetWeaponIdForLoadoutSlot(Direction);
			if (WeaponItemId.IsValid())
			{
				OutAssignedItemIds.Add(WeaponItemId);
			}
		}
	}
}

FGameplayTag UInfoItemTabPresenter::GetEquipmentItemTypeTag() const
{
	return UProjectTagConfig::Get(this)->GetItemEquipmentTypeTag();
}

FGameplayTag UInfoItemTabPresenter::GetWeaponItemTypeTag() const
{
	return UProjectTagConfig::Get(this)->GetItemWeaponTypeTag();
}

FGameplayTag UInfoItemTabPresenter::GetConsumableItemTypeTag() const
{
	return UProjectTagConfig::Get(this)->GetItemConsumableTypeTag();
}
