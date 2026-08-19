#include "Component/Item/InventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Definition/Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Pandora/PandoraLoadoutTypes.h"

namespace
{
	int32 GetReplicatedPandoraWeaponLoadoutIndex(const EEnum_Direction Direction)
	{
		switch (Direction)
		{
		case EEnum_Direction::Left: return 0;
		case EEnum_Direction::Up: return 1;
		case EEnum_Direction::Right: return 2;
		default: return INDEX_NONE;
		}
	}
}

void UInventoryComponent::InitializeReplicatedEntriesFromRuntimeItems()
{
	for (UItemInstance* ItemInstance : AllItemList.Items)
	{
		AddReplicatedItem(ItemInstance);
	}
}

void UInventoryComponent::RebuildRuntimeItemsFromReplicatedEntries()
{
	// =================================================================================================================
	AllItemList.Items.Reset();

	// =================================================================================================================
	for (const FReplicatedInventoryEntry& Entry : ReplicatedEntries.Entries)
	{
		if (!Entry.ItemId.IsValid() || !IsValid(Entry.ItemDefinition))
		{
			continue;
		}

		UItemInstance* ItemInstance = NewObject<UItemInstance>(this);
		ItemInstance->ItemId = Entry.ItemId;
		ItemInstance->ItemDefinition = Entry.ItemDefinition;
		ItemInstance->Quantity = Entry.Quantity;
		ItemInstance->SetUpgradeLevel(Entry.UpgradeLevel);
		AllItemList.Items.Add(ItemInstance);
	}

	RebuildFilteredItemMap();
}

void UInventoryComponent::RebuildFilteredItemMap()
{

Map_Type_ItemList.Reset();

	if (FilterTypeTags.IsEmpty())
	{

		return;
	}

	for (UItemInstance* ItemInstance : AllItemList.Items)
	{
		FilterItem(ItemInstance);
	}

}

void UInventoryComponent::HandleReplicatedEntryAddedOrChanged(const FReplicatedInventoryEntry& Entry)
{
	// =================================================================================================================
	if (!Entry.ItemId.IsValid() || !IsValid(Entry.ItemDefinition))
	{

		return;
	}

	// =================================================================================================================

	UItemInstance* ItemInstance = FindItemInstanceById(Entry.ItemId);
	const bool bWasNewItemInstance = !IsValid(ItemInstance);
	const UItemDefinition* PreviousItemDefinition = bWasNewItemInstance ? nullptr : ItemInstance->ItemDefinition.Get();
	if (!IsValid(ItemInstance))
	{
		ItemInstance = NewObject<UItemInstance>(this);
		AllItemList.Items.Add(ItemInstance);
	}

	// =================================================================================================================
	ItemInstance->ItemId = Entry.ItemId;
	ItemInstance->ItemDefinition = Entry.ItemDefinition;
	ItemInstance->Quantity = Entry.Quantity;
	ItemInstance->SetUpgradeLevel(Entry.UpgradeLevel);

	if (bWasNewItemInstance)
	{
		FilterItem(ItemInstance);
	}
	else if (PreviousItemDefinition != Entry.ItemDefinition)
	{
		RebuildFilteredItemMap();
	}

	OnInventoryChanged.Broadcast();
	RefreshPandoraWeaponLoadoutPresentationAssets();
}

void UInventoryComponent::HandleReplicatedEntryRemoved(FGuid ItemId)
{
	if (!ItemId.IsValid())
	{
		return;
	}

	// =================================================================================================================

	for (int32 Index = AllItemList.Items.Num() - 1; Index >= 0; --Index)
	{
		UItemInstance* ItemInstance = AllItemList.Items[Index];
		if (IsValid(ItemInstance) && ItemInstance->GetItemId() == ItemId)
		{
			AllItemList.Items.RemoveAt(Index);
			break;
		}
	}

	RebuildFilteredItemMap();
	ClearConsumableQuickSlotReferencesToItem(ItemId);
	RefreshPandoraWeaponLoadoutPresentationAssets();
	OnInventoryChanged.Broadcast();
}

void UInventoryComponent::AddReplicatedItem(UItemInstance* ItemInstance)
{
	// =================================================================================================================
	if (!HasInventoryAuthority())
	{

		return;
	}

	if (!IsValid(ItemInstance) || !IsValid(ItemInstance->ItemDefinition))
	{

		return;
	}

	// =================================================================================================================

	ItemInstance->EnsureItemId();
	AllItemList.Items.AddUnique(ItemInstance);

	// =================================================================================================================

	if (FReplicatedInventoryEntry* ExistingEntry = FindReplicatedEntryById(ItemInstance->GetItemId()))
	{
		ExistingEntry->ItemDefinition = ItemInstance->ItemDefinition;
		ExistingEntry->Quantity = ItemInstance->Quantity;
		ExistingEntry->UpgradeLevel = ItemInstance->GetUpgradeLevel();
		ReplicatedEntries.MarkEntryDirty(*ExistingEntry);
		MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, ReplicatedEntries, this);
	}
	else
	{
		FReplicatedInventoryEntry& NewEntry = ReplicatedEntries.Entries.AddDefaulted_GetRef();
		NewEntry.ItemId = ItemInstance->GetItemId();
		NewEntry.ItemDefinition = ItemInstance->ItemDefinition;
		NewEntry.Quantity = ItemInstance->Quantity;
		NewEntry.UpgradeLevel = ItemInstance->GetUpgradeLevel();
		ReplicatedEntries.MarkEntryDirty(NewEntry);
		MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, ReplicatedEntries, this);
	}

	RebuildFilteredItemMap();
	OnInventoryChanged.Broadcast();
}

bool UInventoryComponent::RemoveReplicatedItemById(FGuid ItemId)
{
	if (!ItemId.IsValid())
	{
		return false;
	}

	// =================================================================================================================

	const int32 EntryIndex = FindReplicatedEntryIndexById(ItemId);
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	// =================================================================================================================

	for (int32 Index = AllItemList.Items.Num() - 1; Index >= 0; --Index)
	{
		UItemInstance* ItemInstance = AllItemList.Items[Index];
		if (IsValid(ItemInstance) && ItemInstance->GetItemId() == ItemId)
		{
			AllItemList.Items.RemoveAt(Index);
			break;
		}
	}

	// =================================================================================================================

	ReplicatedEntries.Entries.RemoveAt(EntryIndex);
	ReplicatedEntries.MarkArrayDirty();
	MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, ReplicatedEntries, this);
	ClearConsumableQuickSlotReferencesToItem(ItemId);
	ClearEquipmentSlotReferencesToItem(ItemId);
	ClearPandoraWeaponLoadoutReferencesToItem(ItemId);
	RebuildFilteredItemMap();
	OnInventoryChanged.Broadcast();
	return true;
}

bool UInventoryComponent::SetReplicatedItemQuantityById(FGuid ItemId, int32 NewQuantity)
{
	if (!ItemId.IsValid())
	{
		return false;
	}

	// =================================================================================================================

	if (NewQuantity <= 0)
	{
		return RemoveReplicatedItemById(ItemId);
	}

	// =================================================================================================================

	FReplicatedInventoryEntry* Entry = FindReplicatedEntryById(ItemId);
	UItemInstance* ItemInstance = FindItemInstanceById(ItemId);
	if (!Entry || !IsValid(ItemInstance))
	{
		return false;
	}

	// =================================================================================================================
	ItemInstance->Quantity = NewQuantity;
	Entry->Quantity = NewQuantity;
	ReplicatedEntries.MarkEntryDirty(*Entry);
	MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, ReplicatedEntries, this);
	RebuildFilteredItemMap();
	OnInventoryChanged.Broadcast();
	return true;
}

int32 UInventoryComponent::FindReplicatedEntryIndexById(FGuid ItemId) const
{
	if (!ItemId.IsValid())
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < ReplicatedEntries.Entries.Num(); ++Index)
	{
		if (ReplicatedEntries.Entries[Index].ItemId == ItemId)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

FReplicatedInventoryEntry* UInventoryComponent::FindReplicatedEntryById(FGuid ItemId)
{
	const int32 EntryIndex = FindReplicatedEntryIndexById(ItemId);
	return EntryIndex != INDEX_NONE ? &ReplicatedEntries.Entries[EntryIndex] : nullptr;
}

const FReplicatedInventoryEntry* UInventoryComponent::FindReplicatedEntryById(FGuid ItemId) const
{
	const int32 EntryIndex = FindReplicatedEntryIndexById(ItemId);
	return EntryIndex != INDEX_NONE ? &ReplicatedEntries.Entries[EntryIndex] : nullptr;
}

void UInventoryComponent::OnRep_ConsumableQuickSlotItemIds()
{
	EnsureConsumableQuickSlotArray();
	OnInventoryChanged.Broadcast();
}

void UInventoryComponent::OnRep_PandoraWeaponLoadoutItemIds()
{
	EnsurePandoraWeaponLoadoutArray();
	RefreshPandoraWeaponLoadoutPresentationAssets();
	OnPandoraWeaponLoadoutChanged.Broadcast();
}

void UInventoryComponent::OnRep_EquippedItemSlots()
{
	OnEquipmentSlotsChanged.Broadcast();
	OnInventoryChanged.Broadcast();
}

void UInventoryComponent::ServerSetConsumableQuickSlot_Implementation(const int32 SlotIndex, const FGuid ItemId)
{
	SetConsumableQuickSlotItemId(SlotIndex, ItemId);
}

void UInventoryComponent::ServerClearConsumableQuickSlot_Implementation(const int32 SlotIndex)
{
	SetConsumableQuickSlotItemId(SlotIndex, FGuid());
}

void UInventoryComponent::ServerUseConsumableQuickSlot_Implementation(const int32 SlotIndex)
{
	UseConsumableQuickSlot(SlotIndex);
}

void UInventoryComponent::ServerSetPandoraWeaponLoadoutSlot_Implementation(
	const EEnum_Direction Direction,
	const FGuid ItemId)
{
	SetPandoraWeaponLoadoutItemId(Direction, ItemId);
}

void UInventoryComponent::ServerSetEquipmentSlot_Implementation(
	const FGameplayTag SlotTag,
	const FGuid ItemId)
{
	SetEquipmentSlotItemId(SlotTag, ItemId);
}

void UInventoryComponent::ServerSplitConsumableStack_Implementation(const FGuid ItemId)
{
	SplitConsumableStack(ItemId);
}

void UInventoryComponent::ServerMergeConsumableStacks_Implementation(const FGuid SourceItemId, const FGuid TargetItemId)
{
	MergeConsumableStacks(SourceItemId, TargetItemId);
}

void UInventoryComponent::ServerMergeUpgradeableItems_Implementation(
	const FGuid SourceItemId,
	const FGuid TargetItemId)
{
	MergeUpgradeableItems(SourceItemId, TargetItemId);
}

void UInventoryComponent::EnsureConsumableQuickSlotArray()
{
	if (ConsumableQuickSlotItemIds.Num() != ConsumableQuickSlotCount)
	{
		ConsumableQuickSlotItemIds.SetNum(ConsumableQuickSlotCount);
	}
}

void UInventoryComponent::EnsurePandoraWeaponLoadoutArray()
{
	if (PandoraWeaponLoadoutItemIds.Num() != PandoraWeaponLoadoutSlotCount)
	{
		PandoraWeaponLoadoutItemIds.SetNum(PandoraWeaponLoadoutSlotCount);
	}
}

bool UInventoryComponent::IsValidConsumableQuickSlotIndex(const int32 SlotIndex) const
{
	return SlotIndex >= 0 && SlotIndex < ConsumableQuickSlotCount;
}

bool UInventoryComponent::SetConsumableQuickSlotItemId(const int32 SlotIndex, const FGuid ItemId)
{
	if (!IsValidConsumableQuickSlotIndex(SlotIndex))
	{
		return false;
	}

	EnsureConsumableQuickSlotArray();
	if (ItemId.IsValid())
	{
		UItemInstance* ItemInstance = FindItemInstanceById(ItemId);
		if (!IsConsumableItem(ItemInstance))
		{

			return false;
		}
	}

	if (ConsumableQuickSlotItemIds[SlotIndex] == ItemId)
	{
		return true;
	}

	ConsumableQuickSlotItemIds[SlotIndex] = ItemId;
	if (HasInventoryAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, ConsumableQuickSlotItemIds, this);
	}

	OnInventoryChanged.Broadcast();
	return true;
}

bool UInventoryComponent::ClearConsumableQuickSlotReferencesToItem(const FGuid ItemId)
{
	if (!ItemId.IsValid())
	{
		return false;
	}

	EnsureConsumableQuickSlotArray();
	bool bChanged = false;
	for (FGuid& QuickSlotItemId : ConsumableQuickSlotItemIds)
	{
		if (QuickSlotItemId == ItemId)
		{
			QuickSlotItemId = FGuid();
			bChanged = true;
		}
	}

	if (bChanged && HasInventoryAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, ConsumableQuickSlotItemIds, this);
	}
	return bChanged;
}

FGameplayTag UInventoryComponent::ResolveEquipmentSlotTag(
	const FGameplayTag SlotTag) const
{
	if (!SlotTag.IsValid())
	{
		return FGameplayTag();
	}

	const UProjectTagConfig* TagConfig = UProjectTagConfig::Get(this);
	TArray<FGameplayTag> EquipmentSlotTags;
	TagConfig->GetItemEquipmentSlotTags(EquipmentSlotTags);

	for (const FGameplayTag& EquipmentSlotTag : EquipmentSlotTags)
	{
		if (EquipmentSlotTag.IsValid() && SlotTag == EquipmentSlotTag)
		{
			return EquipmentSlotTag;
		}
	}
	return FGameplayTag();
}

int32 UInventoryComponent::FindEquipmentSlotIndex(const FGameplayTag SlotTag) const
{
	const FGameplayTag ResolvedSlotTag = ResolveEquipmentSlotTag(SlotTag);
	if (!ResolvedSlotTag.IsValid())
	{
		return INDEX_NONE;
	}

	for (int32 SlotIndex = 0; SlotIndex < EquippedItemSlots.Num(); ++SlotIndex)
	{
		if (EquippedItemSlots[SlotIndex].SlotTag == ResolvedSlotTag)
		{
			return SlotIndex;
		}
	}
	return INDEX_NONE;
}

bool UInventoryComponent::SetEquipmentSlotItemId(
	const FGameplayTag SlotTag,
	const FGuid ItemId)
{
	if (!HasInventoryAuthority())
	{
		return false;
	}

	const FGameplayTag ResolvedSlotTag = ResolveEquipmentSlotTag(SlotTag);
	if (!ResolvedSlotTag.IsValid())
	{
		return false;
	}

	if (ItemId.IsValid())
	{
		const UItemInstance* ItemInstance = FindItemInstanceById(ItemId);
		const UItemDefinition* ItemDefinition = IsValid(ItemInstance)
			? ItemInstance->ItemDefinition.Get()
			: nullptr;
		if (!ItemDefinition
			|| !ItemDefinition->IdTag.IsValid()
			|| !ItemDefinition->IdTag.MatchesTag(ResolvedSlotTag))
		{
			return false;
		}
	}

	bool bChanged = false;
	for (int32 ExistingIndex = EquippedItemSlots.Num() - 1;
		ExistingIndex >= 0;
		--ExistingIndex)
	{
		if (EquippedItemSlots[ExistingIndex].ItemId == ItemId
			&& ItemId.IsValid()
			&& EquippedItemSlots[ExistingIndex].SlotTag != ResolvedSlotTag)
		{
			EquippedItemSlots.RemoveAt(ExistingIndex);
			bChanged = true;
		}
	}

	const int32 ExistingSlotIndex = FindEquipmentSlotIndex(ResolvedSlotTag);
	if (!ItemId.IsValid())
	{
		if (ExistingSlotIndex != INDEX_NONE)
		{
			EquippedItemSlots.RemoveAt(ExistingSlotIndex);
			bChanged = true;
		}
	}
	else if (ExistingSlotIndex == INDEX_NONE)
	{
		FEquippedItemSlot& NewSlot = EquippedItemSlots.AddDefaulted_GetRef();
		NewSlot.SlotTag = ResolvedSlotTag;
		NewSlot.ItemId = ItemId;
		bChanged = true;
	}
	else if (EquippedItemSlots[ExistingSlotIndex].ItemId != ItemId)
	{
		EquippedItemSlots[ExistingSlotIndex].ItemId = ItemId;
		bChanged = true;
	}

	if (!bChanged)
	{
		return true;
	}

	MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, EquippedItemSlots, this);
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
	OnEquipmentSlotsChanged.Broadcast();
	OnInventoryChanged.Broadcast();
	return true;
}

bool UInventoryComponent::ClearEquipmentSlotReferencesToItem(
	const FGuid ItemId)
{
	if (!HasInventoryAuthority() || !ItemId.IsValid())
	{
		return false;
	}

	const int32 RemovedCount = EquippedItemSlots.RemoveAll(
		[ItemId](const FEquippedItemSlot& EquippedItemSlot)
		{
			return EquippedItemSlot.ItemId == ItemId;
		});
	if (RemovedCount <= 0)
	{
		return false;
	}

	MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, EquippedItemSlots, this);
	OnEquipmentSlotsChanged.Broadcast();
	return true;
}

bool UInventoryComponent::SetPandoraWeaponLoadoutItemId(
	const EEnum_Direction Direction,
	const FGuid ItemId)
{
	if (!HasInventoryAuthority())
	{
		return false;
	}

	const int32 SlotIndex = GetReplicatedPandoraWeaponLoadoutIndex(Direction);
	if (SlotIndex == INDEX_NONE)
	{
		return false;
	}

	EnsurePandoraWeaponLoadoutArray();
	APdPlayerState* PlayerState = Cast<APdPlayerState>(GetOwner());
	const EEnum_Direction SelectedDirection = PlayerState
		? PandoraLoadout::GetDirectionFromLoadoutNumber(
			PlayerState->GetSelectedWeaponPandoraLoadoutNumber())
		: EEnum_Direction::Center;
	const FGuid PreviousSelectedWeaponId =
		PandoraLoadout::IsLoadoutDirection(SelectedDirection)
			? GetPandoraWeaponLoadoutItemId(SelectedDirection)
			: FGuid();
	if (ItemId.IsValid())
	{
		UItemInstance* WeaponInstance = FindItemInstanceById(ItemId);
		if (!IsWeaponItem(WeaponInstance))
		{
			return false;
		}
	}

	bool bChanged = false;
	if (ItemId.IsValid())
	{
		for (int32 ExistingIndex = 0; ExistingIndex < PandoraWeaponLoadoutItemIds.Num(); ++ExistingIndex)
		{
			if (ExistingIndex != SlotIndex && PandoraWeaponLoadoutItemIds[ExistingIndex] == ItemId)
			{
				PandoraWeaponLoadoutItemIds[ExistingIndex].Invalidate();
				bChanged = true;
			}
		}
	}

	if (PandoraWeaponLoadoutItemIds[SlotIndex] != ItemId)
	{
		PandoraWeaponLoadoutItemIds[SlotIndex] = ItemId;
		bChanged = true;
	}

	if (!bChanged)
	{
		RefreshPandoraWeaponLoadoutPresentationAssets();
		if (PlayerState && SelectedDirection == Direction)
		{
			PlayerState->ApplySelectedWeaponPandoraLoadout();
		}
		return true;
	}

	MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, PandoraWeaponLoadoutItemIds, this);
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
	RefreshPandoraWeaponLoadoutPresentationAssets();
	OnPandoraWeaponLoadoutChanged.Broadcast();
	const bool bSelectedWeaponChanged =
		PandoraLoadout::IsLoadoutDirection(SelectedDirection)
		&& PreviousSelectedWeaponId
			!= GetPandoraWeaponLoadoutItemId(SelectedDirection);
	if (PlayerState
		&& (SelectedDirection == Direction || bSelectedWeaponChanged))
	{
		PlayerState->ApplySelectedWeaponPandoraLoadout();
	}
	return true;
}

bool UInventoryComponent::ClearPandoraWeaponLoadoutReferencesToItem(const FGuid ItemId)
{
	if (!HasInventoryAuthority() || !ItemId.IsValid())
	{
		return false;
	}

	EnsurePandoraWeaponLoadoutArray();
	APdPlayerState* PlayerState = Cast<APdPlayerState>(GetOwner());
	const EEnum_Direction SelectedDirection = PlayerState
		? PandoraLoadout::GetDirectionFromLoadoutNumber(
			PlayerState->GetSelectedWeaponPandoraLoadoutNumber())
		: EEnum_Direction::Center;
	const bool bClearsSelectedWeapon =
		PandoraLoadout::IsLoadoutDirection(SelectedDirection)
		&& GetPandoraWeaponLoadoutItemId(SelectedDirection) == ItemId;
	bool bChanged = false;
	for (FGuid& WeaponItemId : PandoraWeaponLoadoutItemIds)
	{
		if (WeaponItemId == ItemId)
		{
			WeaponItemId.Invalidate();
			bChanged = true;
		}
	}

	if (!bChanged)
	{
		return false;
	}

	MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, PandoraWeaponLoadoutItemIds, this);
	RefreshPandoraWeaponLoadoutPresentationAssets();
	OnPandoraWeaponLoadoutChanged.Broadcast();
	if (PlayerState && bClearsSelectedWeapon)
	{
		PlayerState->ApplySelectedWeaponPandoraLoadout();
	}
	return true;
}
