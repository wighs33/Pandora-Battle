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
#include "Misc/ScopeExit.h"
#include "Pandora/PandoraLoadoutTypes.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(InventoryComponent)

DEFINE_LOG_CATEGORY(InventoryComponentLog);

void FReplicatedInventoryEntry::PostReplicatedAdd(const FReplicatedInventoryList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->HandleReplicatedEntryAddedOrChanged(*this);
	}
}

void FReplicatedInventoryEntry::PostReplicatedChange(const FReplicatedInventoryList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->HandleReplicatedEntryAddedOrChanged(*this);
	}
}

void FReplicatedInventoryEntry::PreReplicatedRemove(const FReplicatedInventoryList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->HandleReplicatedEntryRemoved(ItemId);
	}
}

UInventoryComponent::UInventoryComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// =================================================================================================================

	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	ReplicatedEntries.Owner = this;
	EnsureConsumableQuickSlotArray();
	EnsureWeaponLoadoutSlotCount();
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// =================================================================================================================

	ReplicatedEntries.Owner = this;
	EnsureConsumableQuickSlotArray();
	EnsureWeaponLoadoutSlotCount();

	UProjectTagConfig::Get(this)->GetItemFilterTypeTags(FilterTypeTags);
	// =================================================================================================================
	if (HasInventoryAuthority())
	{
		RebuildFilteredItemMap();
	}
	else
	{
		RebuildRuntimeItemsFromReplicatedEntries();
	}

	RefreshWeaponLoadoutPresentationAssets();
}

void UInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPendingItemLoads();
	ReplicatedEntries.Owner = nullptr;
	ReleaseWeaponLoadoutPresentationAssets();

	Super::EndPlay(EndPlayReason);
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	FDoRepLifetimeParams OwnerOnlyParams = Params;
	OwnerOnlyParams.Condition = COND_OwnerOnly;

	DOREPLIFETIME_WITH_PARAMS_FAST(UInventoryComponent, ReplicatedEntries, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UInventoryComponent, ConsumableQuickSlotItemIds, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UInventoryComponent, WeaponIdsByLoadoutSlot, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UInventoryComponent, EquippedItemSlots, OwnerOnlyParams);
}

// 기존 BP 지급 요청도 완료 통지를 지원하는 동일한 인벤토리 추가 경로를 사용한다.
void UInventoryComponent::AddItemsByPrimaryAssetIds(const TArray<FPrimaryAssetId>& ItemDefinitions)
{
	AddItemsByPrimaryAssetIdsWithCompletion(ItemDefinitions, {});
}

// 자산 로딩에 성공한 아이템만 지급하고 실제 추가 결과를 요청자에게 전달한다.
void UInventoryComponent::AddItemsByPrimaryAssetIdsWithCompletion(const TArray<FPrimaryAssetId>& ItemDefinitions, FOnPdItemsAdded OnComplete)
{
	if (!HasInventoryAuthority() || ItemDefinitions.IsEmpty())
	{
		OnComplete.ExecuteIfBound({});
		return;
	}
	CleanupCompletedItemLoadHandles();
	const uint64 RequestGeneration = ItemLoadGeneration;
	++PendingItemLoadRequestCount;
	TSharedPtr<FStreamableHandle> LoadHandle = UAssetManager::Get().LoadPrimaryAssets(
		ItemDefinitions, {}, FStreamableDelegate::CreateWeakLambda(this, [this, ItemDefinitions, RequestGeneration, OnComplete]()
		{
			if (RequestGeneration != ItemLoadGeneration)
			{
				return;
			}
			TArray<FPrimaryAssetId> AddedItems;
			if (HasInventoryAuthority())
			{
				++InventoryUpdateDepth;
				ON_SCOPE_EXIT { --InventoryUpdateDepth; FlushInventoryChanges(); };
				UAssetManager& AssetManager = UAssetManager::Get();
				const FGameplayTag ConsumableTypeTag = UProjectTagConfig::Get(this)->GetItemConsumableTypeTag();
				for (const FPrimaryAssetId& DefinitionId : ItemDefinitions)
				{
					const UItemDefinition* Definition = Cast<UItemDefinition>(AssetManager.GetPrimaryAssetObject(DefinitionId));
					if (!IsValid(Definition))
					{
						UE_LOG(InventoryComponentLog, Error, TEXT("Failed to resolve asynchronously loaded item definition '%s'."),
							*DefinitionId.ToString());
						continue;
					}
					if (Definition->IsConsumableDefinition(ConsumableTypeTag))
					{
						UItemInstance* Existing = FindFirstItemInstanceByDefinition(Definition);
						if (Existing && Existing->Quantity < MAX_int32)
						{
							if (SetReplicatedItemQuantityById(Existing->GetOrCreateItemId(), Existing->Quantity + 1))
							{
								AddedItems.Add(DefinitionId);
							}
							continue;
						}
					}
					// 가득 찬 소비 아이템 스택은 유지하고 새 스택으로 추가한다.
					UItemInstance* Item = NewObject<UItemInstance>(this);
					Item->ItemDefinition = Definition;
					Item->Quantity = 1;
					AddReplicatedItem(Item);
					AddedItems.Add(DefinitionId);
				}
			}
			CompletePendingItemLoadRequest(RequestGeneration);
			CleanupCompletedItemLoadHandles();
			if (RequestGeneration == ItemLoadGeneration)
			{
				OnComplete.ExecuteIfBound(AddedItems);
			}
		}));
	// 이미 로딩된 PrimaryAsset은 핸들 없이 완료 콜백만 예약될 수 있다.
	if (LoadHandle)
	{
		PendingItemLoadHandles.Add(LoadHandle);
	}
}

void UInventoryComponent::SetItemQuantityByPrimaryAssetId(
	const FPrimaryAssetId ItemDefinitionId,
	const int32 Quantity)
{
	SetItemQuantityByPrimaryAssetIdInternal(ItemDefinitionId, Quantity, INDEX_NONE);
}

void UInventoryComponent::SetConsumableItemQuantityAndQuickSlotByPrimaryAssetId(
	const FPrimaryAssetId ItemDefinitionId,
	const int32 Quantity,
	const int32 SlotIndex)
{
	if (!IsValidConsumableQuickSlotIndex(SlotIndex))
	{
		return;
	}

	SetItemQuantityByPrimaryAssetIdInternal(ItemDefinitionId, Quantity, SlotIndex);
}

void UInventoryComponent::SetItemQuantityByPrimaryAssetIdInternal(
	const FPrimaryAssetId ItemDefinitionId,
	const int32 Quantity,
	const int32 ConsumableQuickSlotIndex)
{
	if (!HasInventoryAuthority() || !ItemDefinitionId.IsValid())
	{
		return;
	}

	CleanupCompletedItemLoadHandles();

	const TArray<FPrimaryAssetId> ItemDefinitionIds = { ItemDefinitionId };
	const uint64 RequestGeneration = ItemLoadGeneration;
	++PendingItemLoadRequestCount;
	UAssetManager& AssetManager = UAssetManager::Get();
	TSharedPtr<FStreamableHandle> LoadHandle = AssetManager.LoadPrimaryAssets(
		ItemDefinitionIds,
		{},
		FStreamableDelegate::CreateWeakLambda(
			this,
			[this, ItemDefinitionId, Quantity, ConsumableQuickSlotIndex, RequestGeneration]()
		{
			if (RequestGeneration != ItemLoadGeneration)
			{
				return;
			}

			if (!HasInventoryAuthority())
			{
				CompletePendingItemLoadRequest(RequestGeneration);
				CleanupCompletedItemLoadHandles();
				return;
			}

			const UItemDefinition* ItemDefinition = Cast<UItemDefinition>(
				UAssetManager::Get().GetPrimaryAssetObject(ItemDefinitionId));
			if (!IsValid(ItemDefinition))
			{
				UE_LOG(
					InventoryComponentLog,
					Error,
					TEXT("Failed to resolve asynchronously loaded item definition '%s'."),
					*ItemDefinitionId.ToString());
				CompletePendingItemLoadRequest(RequestGeneration);
				CleanupCompletedItemLoadHandles();
				return;
			}

			++InventoryUpdateDepth;
			ON_SCOPE_EXIT { --InventoryUpdateDepth; FlushInventoryChanges(); };
			UItemInstance* ExistingItem = FindFirstItemInstanceByDefinition(ItemDefinition);
			if (Quantity <= 0)
			{
				if (ExistingItem)
				{
					RemoveReplicatedItemById(ExistingItem->GetOrCreateItemId());
				}
				if (IsValidConsumableQuickSlotIndex(ConsumableQuickSlotIndex))
				{
					SetConsumableQuickSlotItemId(ConsumableQuickSlotIndex, FGuid());
				}
				CompletePendingItemLoadRequest(RequestGeneration);
				CleanupCompletedItemLoadHandles();
				return;
			}

			UItemInstance* ItemToAssign = ExistingItem;
			if (ExistingItem)
			{
				SetReplicatedItemQuantityById(ExistingItem->GetOrCreateItemId(), Quantity);
			}
			else
			{
				UItemInstance* NewItemInstance = NewObject<UItemInstance>(this);
				NewItemInstance->ItemDefinition = ItemDefinition;
				NewItemInstance->Quantity = Quantity;
				AddReplicatedItem(NewItemInstance);
				ItemToAssign = NewItemInstance;
			}

			if (IsValidConsumableQuickSlotIndex(ConsumableQuickSlotIndex)
				&& IsConsumableItem(ItemToAssign))
			{
				SetConsumableQuickSlotItemId(
					ConsumableQuickSlotIndex,
					ItemToAssign->GetOrCreateItemId());
			}

			CompletePendingItemLoadRequest(RequestGeneration);
			CleanupCompletedItemLoadHandles();
		}));

	if (LoadHandle.IsValid())
	{
		PendingItemLoadHandles.Add(LoadHandle);
	}
}

void UInventoryComponent::ClearAllItems()
{
	if (!HasInventoryAuthority())
	{
		return;
	}

	// Cancel requests that have not completed and invalidate callbacks that may
	// already be queued for execution.
	CancelPendingItemLoads();

	const bool bHadItems = !AllItemList.Items.IsEmpty() || !ReplicatedEntries.Entries.IsEmpty();
	AllItemList.Items.Reset();
	Map_Type_ItemList.Reset();
	ReplicatedEntries.Entries.Reset();
	ReplicatedEntries.MarkArrayDirty();
	MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, ReplicatedEntries, this);

	EnsureConsumableQuickSlotArray();
	bool bHadQuickSlotReferences = false;
	for (FGuid& QuickSlotItemId : ConsumableQuickSlotItemIds)
	{
		if (QuickSlotItemId.IsValid())
		{
			QuickSlotItemId = FGuid();
			bHadQuickSlotReferences = true;
		}
	}

	if (bHadQuickSlotReferences)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, ConsumableQuickSlotItemIds, this);
	}

	const bool bHadEquippedItemReferences = !EquippedItemSlots.IsEmpty();
	if (bHadEquippedItemReferences)
	{
		EquippedItemSlots.Reset();
		MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, EquippedItemSlots, this);
	}

	EnsureWeaponLoadoutSlotCount();
	bool bHadWeaponLoadoutReferences = false;
	for (FGuid& WeaponItemId : WeaponIdsByLoadoutSlot)
	{
		if (WeaponItemId.IsValid())
		{
			WeaponItemId.Invalidate();
			bHadWeaponLoadoutReferences = true;
		}
	}
	if (bHadWeaponLoadoutReferences)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, WeaponIdsByLoadoutSlot, this);
		RefreshWeaponLoadoutPresentationAssets();
		OnWeaponLoadoutChanged.Broadcast();
	}

	if (bHadEquippedItemReferences)
	{
		OnEquipmentSlotsChanged.Broadcast();
	}
	if (bHadItems
		|| bHadQuickSlotReferences
		|| bHadEquippedItemReferences
		|| bHadWeaponLoadoutReferences)
	{
		NotifyInventoryChanged();
	}
}

void UInventoryComponent::FilterItem(UItemInstance* ItemInstance)
{
	// =================================================================================================================
	const UItemDefinition* ItemDefinition = IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
	if (!ItemDefinition)
	{

		return;
	}

	if (!ItemDefinition->IdTag.IsValid())
	{

		return;
	}

	if (FilterTypeTags.IsEmpty())
	{

		return;
	}

	// =================================================================================================================

	for (const FGameplayTag& TypeTag : FilterTypeTags)
	{
		if (ItemDefinition->IdTag.MatchesTag(TypeTag))
		{
			Map_Type_ItemList.FindOrAdd(TypeTag).Items.AddUnique(ItemInstance);

		}
	}

}

FGuid UInventoryComponent::GetOrCreateItemId(UItemInstance* ItemInstance)
{
	return IsValid(ItemInstance) ? ItemInstance->GetOrCreateItemId() : FGuid();
}

UItemInstance* UInventoryComponent::FindItemInstanceById(FGuid ItemId) const
{
	if (!ItemId.IsValid())
	{
		return nullptr;
	}

	for (UItemInstance* ItemInstance : AllItemList.Items)
	{
		if (IsValid(ItemInstance) && ItemInstance->GetItemId() == ItemId)
		{
			return ItemInstance;
		}
	}

	return nullptr;
}

bool UInventoryComponent::SetConsumableQuickSlot(const int32 SlotIndex, UItemInstance* ItemInstance)
{
	if (!IsValidConsumableQuickSlotIndex(SlotIndex) || !IsValid(ItemInstance))
	{
		return false;
	}

	if (!IsConsumableItem(ItemInstance))
	{
		return false;
	}

	const FGuid ItemId = ItemInstance->GetOrCreateItemId();
	if (!ItemId.IsValid() || !FindItemInstanceById(ItemId))
	{
		return false;
	}

	if (!HasInventoryAuthority())
	{
		ServerSetConsumableQuickSlot(SlotIndex, ItemId);
		return true;
	}

	return SetConsumableQuickSlotItemId(SlotIndex, ItemId);
}

bool UInventoryComponent::ClearConsumableQuickSlot(const int32 SlotIndex)
{
	if (!IsValidConsumableQuickSlotIndex(SlotIndex))
	{
		return false;
	}

	if (!HasInventoryAuthority())
	{
		ServerClearConsumableQuickSlot(SlotIndex);
		return true;
	}

	return SetConsumableQuickSlotItemId(SlotIndex, FGuid());
}

bool UInventoryComponent::UseConsumableQuickSlot(const int32 SlotIndex)
{
	if (!IsValidConsumableQuickSlotIndex(SlotIndex))
	{
		return false;
	}

	if (!HasInventoryAuthority())
	{
		ServerUseConsumableQuickSlot(SlotIndex);
		return true;
	}

	EnsureConsumableQuickSlotArray();
	const FGuid ItemId = ConsumableQuickSlotItemIds[SlotIndex];
	UItemInstance* ItemInstance = FindItemInstanceById(ItemId);
	const UItemDefinition* ItemDefinition = IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
	if (!IsValid(ItemInstance) || !ItemDefinition)
	{

		SetConsumableQuickSlotItemId(SlotIndex, FGuid());
		return false;
	}

	if (!IsConsumableItem(ItemInstance))
	{

		return false;
	}

	const int32 QuantityToConsume = ItemDefinition->GetSafeQuantityToConsume();
	if (ItemInstance->Quantity < QuantityToConsume)
	{

		return false;
	}

	if (!ApplyConsumableItemEffect(ItemInstance))
	{

		return false;
	}

	const int32 NewQuantity = ItemInstance->Quantity - QuantityToConsume;
	return SetReplicatedItemQuantityById(ItemId, NewQuantity);
}

UItemInstance* UInventoryComponent::GetConsumableQuickSlotItem(const int32 SlotIndex) const
{
	if (!IsValidConsumableQuickSlotIndex(SlotIndex) || !ConsumableQuickSlotItemIds.IsValidIndex(SlotIndex))
	{
		return nullptr;
	}

	return FindItemInstanceById(ConsumableQuickSlotItemIds[SlotIndex]);
}

bool UInventoryComponent::SetEquipmentSlot(
	const FGameplayTag SlotTag,
	UItemInstance* ItemInstance)
{
	const FGameplayTag ResolvedSlotTag = ResolveEquipmentSlotTag(SlotTag);
	const UItemDefinition* ItemDefinition = IsValid(ItemInstance)
		? ItemInstance->ItemDefinition.Get()
		: nullptr;
	if (!ResolvedSlotTag.IsValid()
		|| !ItemDefinition
		|| !ItemDefinition->IdTag.IsValid()
		|| !ItemDefinition->IdTag.MatchesTag(ResolvedSlotTag))
	{
		return false;
	}

	const FGuid ItemId = ItemInstance->GetOrCreateItemId();
	if (!ItemId.IsValid() || FindItemInstanceById(ItemId) != ItemInstance)
	{
		return false;
	}

	if (!HasInventoryAuthority())
	{
		ServerSetEquipmentSlot(ResolvedSlotTag, ItemId);
		return true;
	}

	return SetEquipmentSlotItemId(ResolvedSlotTag, ItemId);
}

bool UInventoryComponent::ClearEquipmentSlot(const FGameplayTag SlotTag)
{
	const FGameplayTag ResolvedSlotTag = ResolveEquipmentSlotTag(SlotTag);
	if (!ResolvedSlotTag.IsValid())
	{
		return false;
	}

	if (!HasInventoryAuthority())
	{
		ServerSetEquipmentSlot(ResolvedSlotTag, FGuid());
		return true;
	}

	return SetEquipmentSlotItemId(ResolvedSlotTag, FGuid());
}

FGuid UInventoryComponent::GetEquipmentSlotItemId(const FGameplayTag SlotTag) const
{
	const int32 SlotIndex = FindEquipmentSlotIndex(SlotTag);
	return EquippedItemSlots.IsValidIndex(SlotIndex)
		? EquippedItemSlots[SlotIndex].ItemId
		: FGuid();
}

UItemInstance* UInventoryComponent::GetEquipmentSlotItem(const FGameplayTag SlotTag) const
{
	return FindItemInstanceById(GetEquipmentSlotItemId(SlotTag));
}

bool UInventoryComponent::AssignWeaponToLoadoutSlot(
	const EEnum_Direction Direction,
	UItemInstance* WeaponInstance)
{
	if ((PandoraLoadout::GetLoadoutNumberFromDirection(Direction) - 1) == INDEX_NONE || !IsWeaponItem(WeaponInstance))
	{
		return false;
	}

	const FGuid ItemId = WeaponInstance->GetOrCreateItemId();
	if (!ItemId.IsValid() || FindItemInstanceById(ItemId) != WeaponInstance)
	{
		return false;
	}

	if (!HasInventoryAuthority())
	{
		ServerSetWeaponIdForLoadoutSlot(Direction, ItemId);
		return true;
	}

	return SetWeaponIdForLoadoutSlot(Direction, ItemId);
}

bool UInventoryComponent::ClearWeaponFromLoadoutSlot(const EEnum_Direction Direction)
{
	if ((PandoraLoadout::GetLoadoutNumberFromDirection(Direction) - 1) == INDEX_NONE)
	{
		return false;
	}

	if (!HasInventoryAuthority())
	{
		ServerSetWeaponIdForLoadoutSlot(Direction, FGuid());
		return true;
	}

	return SetWeaponIdForLoadoutSlot(Direction, FGuid());
}

FGuid UInventoryComponent::GetWeaponIdForLoadoutSlot(const EEnum_Direction Direction) const
{
	const int32 SlotIndex = (PandoraLoadout::GetLoadoutNumberFromDirection(Direction) - 1);
	return WeaponIdsByLoadoutSlot.IsValidIndex(SlotIndex)
		? WeaponIdsByLoadoutSlot[SlotIndex]
		: FGuid();
}

UItemInstance* UInventoryComponent::FindWeaponForLoadoutSlot(const EEnum_Direction Direction) const
{
	return FindItemInstanceById(GetWeaponIdForLoadoutSlot(Direction));
}

bool UInventoryComponent::SplitConsumableStack(const FGuid ItemId)
{
	if (!ItemId.IsValid())
	{

		return false;
	}

	if (!HasInventoryAuthority())
	{
		ServerSplitConsumableStack(ItemId);
		return true;
	}

	UItemInstance* SourceItem = FindItemInstanceById(ItemId);
	if (!IsValid(SourceItem) || !IsValid(SourceItem->ItemDefinition))
	{

		return false;
	}

	if (!IsConsumableItem(SourceItem))
	{

		return false;
	}

	if (SourceItem->Quantity < 2)
	{

		return false;
	}

	++InventoryUpdateDepth;
	ON_SCOPE_EXIT { --InventoryUpdateDepth; FlushInventoryChanges(); };
	const int32 NewStackQuantity = SourceItem->Quantity / 2;
	const int32 RemainingQuantity = SourceItem->Quantity - NewStackQuantity;
	const UItemDefinition* ItemDefinition = SourceItem->ItemDefinition.Get();

	if (!SetReplicatedItemQuantityById(ItemId, RemainingQuantity))
	{

		return false;
	}

	UItemInstance* NewItem = NewObject<UItemInstance>(this);
	NewItem->ItemDefinition = ItemDefinition;
	NewItem->Quantity = NewStackQuantity;
	AddReplicatedItem(NewItem);

	return true;
}

bool UInventoryComponent::MergeConsumableStacks(const FGuid SourceItemId, const FGuid TargetItemId)
{
	if (!SourceItemId.IsValid() || !TargetItemId.IsValid() || SourceItemId == TargetItemId)
	{

		return false;
	}

	if (!HasInventoryAuthority())
	{
		ServerMergeConsumableStacks(SourceItemId, TargetItemId);
		return true;
	}

	UItemInstance* SourceItem = FindItemInstanceById(SourceItemId);
	UItemInstance* TargetItem = FindItemInstanceById(TargetItemId);
	const UItemDefinition* SourceDefinition = IsValid(SourceItem) ? SourceItem->ItemDefinition.Get() : nullptr;
	const UItemDefinition* TargetDefinition = IsValid(TargetItem) ? TargetItem->ItemDefinition.Get() : nullptr;

	if (!SourceDefinition || !TargetDefinition)
	{

		return false;
	}

	if (SourceDefinition != TargetDefinition || !IsConsumableItem(SourceItem) || !IsConsumableItem(TargetItem))
	{

		return false;
	}

	const int32 SourceQuantity = FMath::Max(0, SourceItem->Quantity);
	const int32 TargetQuantity = FMath::Max(0, TargetItem->Quantity);
	const int64 CombinedQuantity = static_cast<int64>(SourceQuantity) + TargetQuantity;
	if (CombinedQuantity <= 0 || CombinedQuantity > MAX_int32)
	{
		return false;
	}

	++InventoryUpdateDepth;
	ON_SCOPE_EXIT { --InventoryUpdateDepth; FlushInventoryChanges(); };
	if (!SetReplicatedItemQuantityById(TargetItemId, static_cast<int32>(CombinedQuantity)))
	{

		return false;
	}

	return RemoveReplicatedItemById(SourceItemId);
}

bool UInventoryComponent::MergeUpgradeableItems(
	const FGuid SourceItemId,
	const FGuid TargetItemId)
{
	if (!SourceItemId.IsValid()
		|| !TargetItemId.IsValid()
		|| SourceItemId == TargetItemId)
	{
		return false;
	}

	if (!HasInventoryAuthority())
	{
		ServerMergeUpgradeableItems(SourceItemId, TargetItemId);
		return true;
	}

	UItemInstance* SourceItem = FindItemInstanceById(SourceItemId);
	UItemInstance* TargetItem = FindItemInstanceById(TargetItemId);
	const UItemDefinition* SourceDefinition = IsValid(SourceItem)
		? SourceItem->ItemDefinition.Get()
		: nullptr;
	const UItemDefinition* TargetDefinition = IsValid(TargetItem)
		? TargetItem->ItemDefinition.Get()
		: nullptr;
	if (!SourceDefinition
		|| SourceDefinition != TargetDefinition
		|| !IsUpgradeableItem(SourceItem)
		|| !IsUpgradeableItem(TargetItem))
	{
		return false;
	}

	const auto IsItemAssigned = [this](const FGuid ItemId)
	{
		return WeaponIdsByLoadoutSlot.Contains(ItemId)
			|| EquippedItemSlots.ContainsByPredicate(
			[ItemId](const FEquippedItemSlot& EquippedItemSlot)
		{
			return EquippedItemSlot.ItemId == ItemId;
		});
	};

	const bool bSourceIsAssigned = IsItemAssigned(SourceItemId);
	const bool bTargetIsAssigned = IsItemAssigned(TargetItemId);
	if (bSourceIsAssigned && bTargetIsAssigned)
	{
		// Never consume an item that another equipment/loadout slot still owns.
		return false;
	}

	// Dropping in either direction upgrades the assigned item and consumes the
	// unassigned duplicate, so its slot reference and active stats stay intact.
	const FGuid ConsumedItemId = bSourceIsAssigned ? TargetItemId : SourceItemId;
	const FGuid UpgradedItemId = bSourceIsAssigned ? SourceItemId : TargetItemId;
	UItemInstance* ConsumedItem = bSourceIsAssigned ? TargetItem : SourceItem;
	UItemInstance* UpgradedItem = bSourceIsAssigned ? SourceItem : TargetItem;

	const int32 SourceEntryIndex = FindReplicatedEntryIndexById(ConsumedItemId);
	FReplicatedInventoryEntry* TargetEntry = FindReplicatedEntryById(UpgradedItemId);
	if (SourceEntryIndex == INDEX_NONE || !TargetEntry)
	{
		return false;
	}

	const int64 MergedUpgradeLevel =
		static_cast<int64>(ConsumedItem->GetUpgradeLevel())
		+ static_cast<int64>(UpgradedItem->GetUpgradeLevel())
		+ 1;
	const int32 NewUpgradeLevel = static_cast<int32>(FMath::Min<int64>(
		MergedUpgradeLevel,
		static_cast<int64>(MAX_int32)));
	UpgradedItem->SetUpgradeLevel(NewUpgradeLevel);
	TargetEntry->UpgradeLevel = NewUpgradeLevel;
	ReplicatedEntries.MarkEntryDirty(*TargetEntry);

	return RemoveReplicatedItemById(ConsumedItemId);
}

void UInventoryComponent::ApplyProjectTagConfig(const UProjectTagConfig* ProjectTagConfig)
{
	const UProjectTagConfig* EffectiveConfig = ProjectTagConfig ? ProjectTagConfig : UProjectTagConfig::GetDefaultConfig();
	EffectiveConfig->GetItemFilterTypeTags(FilterTypeTags);

	RebuildFilteredItemMap();
	NotifyInventoryChanged();
}

bool UInventoryComponent::HasInventoryAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

bool UInventoryComponent::IsConsumableItem(const UItemInstance* ItemInstance) const
{
	const UItemDefinition* ItemDefinition = IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
	const FGameplayTag ConsumableTypeTag = UProjectTagConfig::Get(this)->GetItemConsumableTypeTag();
	return ItemDefinition
		&& ItemDefinition->IsConsumableDefinition(ConsumableTypeTag);
}

bool UInventoryComponent::IsWeaponItem(const UItemInstance* ItemInstance) const
{
	const UItemDefinition* ItemDefinition = IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
	const FGameplayTag WeaponTypeTag = UProjectTagConfig::Get(this)->GetItemWeaponTypeTag();
	return ItemDefinition
		&& WeaponTypeTag.IsValid()
		&& ItemDefinition->IdTag.IsValid()
		&& ItemDefinition->IdTag.MatchesTag(WeaponTypeTag);
}

bool UInventoryComponent::IsUpgradeableItem(const UItemInstance* ItemInstance) const
{
	const UItemDefinition* ItemDefinition = IsValid(ItemInstance)
		? ItemInstance->ItemDefinition.Get()
		: nullptr;
	if (!ItemDefinition)
	{
		return false;
	}

	const UProjectTagConfig* TagConfig = UProjectTagConfig::Get(this);
	const FGameplayTag WeaponTypeTag = TagConfig->GetItemWeaponTypeTag();
	const FGameplayTag EquipmentTypeTag = TagConfig->GetItemEquipmentTypeTag();
	return ItemDefinition->IsWeaponDefinition(WeaponTypeTag)
		|| ItemDefinition->MatchesItemType(EquipmentTypeTag);
}

bool UInventoryComponent::ApplyConsumableItemEffect(const UItemInstance* ItemInstance) const
{
	const UItemDefinition* ItemDefinition = IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
	if (!ItemDefinition || !ItemDefinition->ConsumeGameplayEffectClass)
	{

		return false;
	}

	const APdPlayerState* PlayerState = Cast<APdPlayerState>(GetOwner());
	UAbilitySystemComponent* AbilitySystemComponent = PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent)
	{
		if (const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(GetOwner()))
		{
			AbilitySystemComponent = AbilitySystemInterface->GetAbilitySystemComponent();
		}
	}
	if (!AbilitySystemComponent)
	{

		return false;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(ItemDefinition);

	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(ItemDefinition->ConsumeGameplayEffectClass, 1.0f, EffectContext);
	if (!SpecHandle.IsValid())
	{

		return false;
	}

	for (const TPair<FGameplayTag, float>& Pair : ItemDefinition->Map_Consume_Magnitude)
	{
		if (Pair.Key.IsValid() && !FMath::IsNearlyZero(Pair.Value))
		{
			SpecHandle.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
		}
	}

	const FActiveGameplayEffectHandle AppliedHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return AppliedHandle.WasSuccessfullyApplied();
}

UItemInstance* UInventoryComponent::FindFirstItemInstanceByDefinition(const UItemDefinition* ItemDefinition) const
{
	if (!ItemDefinition)
	{
		return nullptr;
	}

	for (UItemInstance* ItemInstance : AllItemList.Items)
	{
		if (IsValid(ItemInstance) && ItemInstance->ItemDefinition.Get() == ItemDefinition)
		{
			return ItemInstance;
		}
	}

	return nullptr;
}
