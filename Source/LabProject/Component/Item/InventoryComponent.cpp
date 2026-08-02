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
#include UE_INLINE_GENERATED_CPP_BY_NAME(InventoryComponent)

DEFINE_LOG_CATEGORY(InventoryComponentLog);

namespace
{
	int32 GetPandoraWeaponLoadoutIndex(const EEnum_Direction Direction)
	{
		switch (Direction)
		{
		case EEnum_Direction::Left:
			return 0;
		case EEnum_Direction::Up:
			return 1;
		case EEnum_Direction::Right:
			return 2;
		default:
			return INDEX_NONE;
		}
	}
}

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
	EnsurePandoraWeaponLoadoutArray();
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// =================================================================================================================

	ReplicatedEntries.Owner = this;
	EnsureConsumableQuickSlotArray();
	EnsurePandoraWeaponLoadoutArray();

	UProjectTagConfig::Get(this)->GetItemFilterTypeTags(FilterTypeTags);
	// =================================================================================================================
	if (HasInventoryAuthority())
	{
		InitializeReplicatedEntriesFromRuntimeItems();
		RebuildFilteredItemMap();
	}
	else
	{
		RebuildRuntimeItemsFromReplicatedEntries();
	}

	RefreshPandoraWeaponLoadoutPresentationAssets();
}

void UInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPendingItemLoads();
	ReleasePandoraWeaponLoadoutPresentationAssets();

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
	DOREPLIFETIME_WITH_PARAMS_FAST(UInventoryComponent, PandoraWeaponLoadoutItemIds, OwnerOnlyParams);
}

void UInventoryComponent::AddItemsByPrimaryAssetIds(const TArray<FPrimaryAssetId>& ItemDefinitions)
{
	// =================================================================================================================
	if (!HasInventoryAuthority())
	{

		return;
	}

	// =================================================================================================================

	if (ItemDefinitions.IsEmpty())
	{
		RebuildFilteredItemMap();
		return;
	}

	// =================================================================================================================

	CleanupCompletedItemLoadHandles();

	const uint64 RequestGeneration = ItemLoadGeneration;
	TrackPendingItemDefinitionRequests(ItemDefinitions);
	++PendingItemLoadRequestCount;
	UAssetManager& AssetManager = UAssetManager::Get();
	TSharedPtr<FStreamableHandle> LoadHandle = AssetManager.LoadPrimaryAssets(
		ItemDefinitions,
		{},
		FStreamableDelegate::CreateWeakLambda(
			this,
			[this, ItemDefinitions, RequestGeneration]()
		{
			if (RequestGeneration != ItemLoadGeneration)
			{
				return;
			}

			if (!HasInventoryAuthority())
			{
				ReleasePendingItemDefinitionRequests(ItemDefinitions);
				CompletePendingItemLoadRequest(RequestGeneration);
				CleanupCompletedItemLoadHandles();
				return;
			}

			UAssetManager& LoadedAssetManager = UAssetManager::Get();

			// =================================================================================================================

			for (const FPrimaryAssetId& ItemDefinitionId : ItemDefinitions)
			{
				const UItemDefinition* ItemDefinition = Cast<UItemDefinition>(LoadedAssetManager.GetPrimaryAssetObject(ItemDefinitionId));
				if (!IsValid(ItemDefinition))
				{
					UE_LOG(
						InventoryComponentLog,
						Error,
						TEXT("Failed to resolve asynchronously loaded item definition '%s'."),
						*ItemDefinitionId.ToString());
					continue;
				}

				const FGameplayTag ConsumableTypeTag = UProjectTagConfig::Get(this)->GetItemConsumableTypeTag();
				if (ItemDefinition->IsConsumableDefinition(ConsumableTypeTag))
				{
					if (UItemInstance* ExistingConsumable = FindFirstItemInstanceByDefinition(ItemDefinition))
					{
						SetReplicatedItemQuantityById(ExistingConsumable->GetOrCreateItemId(), ExistingConsumable->Quantity + 1);

						continue;
					}
				}

				UItemInstance* NewItemInstance = NewObject<UItemInstance>(this);
				NewItemInstance->ItemDefinition = ItemDefinition;
				NewItemInstance->Quantity = 1;
				AddReplicatedItem(NewItemInstance);
			}

			ReleasePendingItemDefinitionRequests(ItemDefinitions);
			CompletePendingItemLoadRequest(RequestGeneration);
			CleanupCompletedItemLoadHandles();
		}));

	if (LoadHandle.IsValid())
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
	TrackPendingItemDefinitionRequests(ItemDefinitionIds);
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
				ReleasePendingItemDefinitionRequests({ ItemDefinitionId });
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
				ReleasePendingItemDefinitionRequests({ ItemDefinitionId });
				CompletePendingItemLoadRequest(RequestGeneration);
				CleanupCompletedItemLoadHandles();
				return;
			}

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
				ReleasePendingItemDefinitionRequests({ ItemDefinitionId });
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

			ReleasePendingItemDefinitionRequests({ ItemDefinitionId });
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

	EnsurePandoraWeaponLoadoutArray();
	bool bHadPandoraWeaponLoadoutReferences = false;
	for (FGuid& WeaponItemId : PandoraWeaponLoadoutItemIds)
	{
		if (WeaponItemId.IsValid())
		{
			WeaponItemId.Invalidate();
			bHadPandoraWeaponLoadoutReferences = true;
		}
	}
	if (bHadPandoraWeaponLoadoutReferences)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, PandoraWeaponLoadoutItemIds, this);
		RefreshPandoraWeaponLoadoutPresentationAssets();
		OnPandoraWeaponLoadoutChanged.Broadcast();
	}

	if (bHadItems || bHadQuickSlotReferences || bHadPandoraWeaponLoadoutReferences)
	{
		OnInventoryChanged.Broadcast();
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

	bool bMatchedAnyType = false;
	for (const FGameplayTag& TypeTag : FilterTypeTags)
	{
		if (ItemDefinition->IdTag.MatchesTag(TypeTag))
		{
			AddValueToMap(TypeTag, ItemInstance);
			bMatchedAnyType = true;

		}
	}

}

void UInventoryComponent::AddValueToMap(FGameplayTag TypeTag, UItemInstance* ItemInstance)
{
	if (!IsValid(ItemInstance))
	{
		return;
	}

	FItemList& ItemList = Map_Type_ItemList.FindOrAdd(TypeTag);
	ItemList.Items.AddUnique(ItemInstance);

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

void UInventoryComponent::GetOwnedOrPendingItemDefinitionIds(
	TSet<FPrimaryAssetId>& OutItemDefinitionIds) const
{
	OutItemDefinitionIds.Reset();
	for (const UItemInstance* ItemInstance : AllItemList.Items)
	{
		const UItemDefinition* ItemDefinition = IsValid(ItemInstance)
			? ItemInstance->ItemDefinition.Get()
			: nullptr;
		if (IsValid(ItemDefinition))
		{
			const FPrimaryAssetId ItemDefinitionId =
				ItemDefinition->GetPrimaryAssetId();
			if (ItemDefinitionId.IsValid())
			{
				OutItemDefinitionIds.Add(ItemDefinitionId);
			}
		}
	}

	for (const FReplicatedInventoryEntry& Entry : ReplicatedEntries.Entries)
	{
		if (IsValid(Entry.ItemDefinition))
		{
			const FPrimaryAssetId ItemDefinitionId =
				Entry.ItemDefinition->GetPrimaryAssetId();
			if (ItemDefinitionId.IsValid())
			{
				OutItemDefinitionIds.Add(ItemDefinitionId);
			}
		}
	}

	for (const TPair<FPrimaryAssetId, int32>& PendingRequest :
		PendingItemDefinitionRequestCounts)
	{
		if (PendingRequest.Key.IsValid() && PendingRequest.Value > 0)
		{
			OutItemDefinitionIds.Add(PendingRequest.Key);
		}
	}
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
	const bool bQuantityUpdated = SetReplicatedItemQuantityById(ItemId, NewQuantity);

	return bQuantityUpdated;
}

UItemInstance* UInventoryComponent::GetConsumableQuickSlotItem(const int32 SlotIndex) const
{
	if (!IsValidConsumableQuickSlotIndex(SlotIndex) || !ConsumableQuickSlotItemIds.IsValidIndex(SlotIndex))
	{
		return nullptr;
	}

	return FindItemInstanceById(ConsumableQuickSlotItemIds[SlotIndex]);
}

bool UInventoryComponent::SetPandoraWeaponLoadoutSlot(
	const EEnum_Direction Direction,
	UItemInstance* WeaponInstance)
{
	if (GetPandoraWeaponLoadoutIndex(Direction) == INDEX_NONE || !IsWeaponItem(WeaponInstance))
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
		ServerSetPandoraWeaponLoadoutSlot(Direction, ItemId);
		return true;
	}

	return SetPandoraWeaponLoadoutItemId(Direction, ItemId);
}

bool UInventoryComponent::ClearPandoraWeaponLoadoutSlot(const EEnum_Direction Direction)
{
	if (GetPandoraWeaponLoadoutIndex(Direction) == INDEX_NONE)
	{
		return false;
	}

	if (!HasInventoryAuthority())
	{
		ServerSetPandoraWeaponLoadoutSlot(Direction, FGuid());
		return true;
	}

	return SetPandoraWeaponLoadoutItemId(Direction, FGuid());
}

FGuid UInventoryComponent::GetPandoraWeaponLoadoutItemId(const EEnum_Direction Direction) const
{
	const int32 SlotIndex = GetPandoraWeaponLoadoutIndex(Direction);
	return PandoraWeaponLoadoutItemIds.IsValidIndex(SlotIndex)
		? PandoraWeaponLoadoutItemIds[SlotIndex]
		: FGuid();
}

UItemInstance* UInventoryComponent::GetPandoraWeaponLoadoutItem(const EEnum_Direction Direction) const
{
	return FindItemInstanceById(GetPandoraWeaponLoadoutItemId(Direction));
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
	const int32 MergedQuantity = SourceQuantity + TargetQuantity;
	if (MergedQuantity <= 0)
	{
		return false;
	}

	if (!SetReplicatedItemQuantityById(TargetItemId, MergedQuantity))
	{

		return false;
	}

	const bool bRemovedSource = RemoveReplicatedItemById(SourceItemId);

	return bRemovedSource;
}

void UInventoryComponent::ApplyProjectTagConfig(const UProjectTagConfig* ProjectTagConfig)
{
	const UProjectTagConfig* EffectiveConfig = ProjectTagConfig ? ProjectTagConfig : UProjectTagConfig::GetDefaultConfig();
	EffectiveConfig->GetItemFilterTypeTags(FilterTypeTags);

	RebuildFilteredItemMap();
	OnInventoryChanged.Broadcast();
}

bool UInventoryComponent::HasInventoryAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

bool UInventoryComponent::HasPendingItemLoads()
{
	CleanupCompletedItemLoadHandles();
	return PendingItemLoadRequestCount > 0
		|| !PendingItemLoadHandles.IsEmpty();
}

void UInventoryComponent::CleanupCompletedItemLoadHandles()
{
	PendingItemLoadHandles.RemoveAll(
		[](const TSharedPtr<FStreamableHandle>& PendingHandle)
		{
			return !PendingHandle.IsValid() || PendingHandle->HasLoadCompleted();
		});
}

void UInventoryComponent::CompletePendingItemLoadRequest(
	const uint64 RequestGeneration)
{
	if (RequestGeneration != ItemLoadGeneration)
	{
		return;
	}

	if (ensure(PendingItemLoadRequestCount > 0))
	{
		--PendingItemLoadRequestCount;
	}
}

void UInventoryComponent::CancelPendingItemLoads()
{
	// CancelHandle cannot retract a completion delegate that is already queued.
	// Advancing the generation makes every callback from the old batch a no-op.
	++ItemLoadGeneration;
	PendingItemLoadRequestCount = 0;
	PendingItemDefinitionRequestCounts.Reset();

	for (const TSharedPtr<FStreamableHandle>& PendingHandle : PendingItemLoadHandles)
	{
		if (PendingHandle.IsValid() && !PendingHandle->HasLoadCompleted())
		{
			PendingHandle->CancelHandle();
		}
	}

	PendingItemLoadHandles.Reset();
}

void UInventoryComponent::TrackPendingItemDefinitionRequests(
	const TArray<FPrimaryAssetId>& ItemDefinitionIds)
{
	for (const FPrimaryAssetId& ItemDefinitionId : ItemDefinitionIds)
	{
		if (ItemDefinitionId.IsValid())
		{
			++PendingItemDefinitionRequestCounts.FindOrAdd(ItemDefinitionId);
		}
	}
}

void UInventoryComponent::ReleasePendingItemDefinitionRequests(
	const TArray<FPrimaryAssetId>& ItemDefinitionIds)
{
	for (const FPrimaryAssetId& ItemDefinitionId : ItemDefinitionIds)
	{
		int32* RequestCount =
			PendingItemDefinitionRequestCounts.Find(ItemDefinitionId);
		if (!RequestCount)
		{
			continue;
		}

		if (*RequestCount <= 1)
		{
			PendingItemDefinitionRequestCounts.Remove(ItemDefinitionId);
		}
		else
		{
			--(*RequestCount);
		}
	}
}

void UInventoryComponent::RefreshPandoraWeaponLoadoutPresentationAssets()
{
	EnsurePandoraWeaponLoadoutArray();

	TMap<FPrimaryAssetId, const UItemDefinition*> DesiredItemDefinitions;
	for (const FGuid& WeaponItemId : PandoraWeaponLoadoutItemIds)
	{
		const UItemInstance* WeaponInstance = FindItemInstanceById(WeaponItemId);
		const UItemDefinition* ItemDefinition =
			IsValid(WeaponInstance) ? WeaponInstance->ItemDefinition.Get() : nullptr;
		if (!IsValid(ItemDefinition))
		{
			continue;
		}

		const FPrimaryAssetId AssetId = ItemDefinition->GetPrimaryAssetId();
		if (AssetId.IsValid())
		{
			DesiredItemDefinitions.Add(AssetId, ItemDefinition);
		}
	}

	for (auto HandleIt = PandoraWeaponPresentationLoadHandles.CreateIterator(); HandleIt; ++HandleIt)
	{
		if (DesiredItemDefinitions.Contains(HandleIt.Key()))
		{
			continue;
		}

		for (const TSharedPtr<FStreamableHandle>& LoadHandle : HandleIt.Value())
		{
			if (LoadHandle.IsValid())
			{
				LoadHandle->ReleaseHandle();
			}
		}
		HandleIt.RemoveCurrent();
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	const FName PresentationBundleName = UItemDefinition::GetWeaponPresentationBundleName();
	for (const TPair<FPrimaryAssetId, const UItemDefinition*>& DesiredPair
		: DesiredItemDefinitions)
	{
		const FPrimaryAssetId& AssetId = DesiredPair.Key;
		if (PandoraWeaponPresentationLoadHandles.Contains(AssetId))
		{
			continue;
		}

		// Resolve the serialized bundle to concrete soft paths, then own a
		// streamable handle per inventory. UAssetManager bundle state is global
		// and LoadPrimaryAsset legitimately returns no handle when that state is
		// already active; treating that no-op as a failure produced the
		// DA_Greatsword error and also made per-player release semantics unclear.
		TArray<FSoftObjectPath> PresentationAssetPaths;
		const FAssetBundleEntry BundleEntry =
			AssetManager.GetAssetBundleEntry(
				AssetId,
				PresentationBundleName);
		if (BundleEntry.IsValid())
		{
			for (const FTopLevelAssetPath& AssetPath : BundleEntry.AssetPaths)
			{
				PresentationAssetPaths.AddUnique(FSoftObjectPath(AssetPath));
			}
		}

		// Merge paths from the loaded definition as an editor-safe fallback for
		// assets that predate the serialized bundle metadata. The metadata still
		// remains responsible for including these references in cooked builds.
		if (IsValid(DesiredPair.Value))
		{
			TArray<FSoftObjectPath> DefinitionPaths;
			DesiredPair.Value->GetWeaponPresentationAssetPaths(DefinitionPaths);
			for (const FSoftObjectPath& AssetPath : DefinitionPaths)
			{
				if (!AssetPath.IsNull())
				{
					PresentationAssetPaths.AddUnique(AssetPath);
				}
			}
		}

		PresentationAssetPaths.RemoveAll(
			[](const FSoftObjectPath& AssetPath)
			{
				return AssetPath.IsNull();
			});

		// A weapon definition with no presentation references has nothing to
		// preload. Record an empty sentinel so subsequent refreshes stay cheap.
		if (PresentationAssetPaths.IsEmpty())
		{
			PandoraWeaponPresentationLoadHandles.Add(AssetId, {});
			continue;
		}

		TSharedPtr<FStreamableHandle> LoadHandle =
			AssetManager.GetStreamableManager().RequestAsyncLoad(
				PresentationAssetPaths);
		if (!LoadHandle.IsValid())
		{
			UE_LOG(
				InventoryComponentLog,
				Error,
				TEXT("Failed to start weapon presentation preload for loadout item '%s'."),
				*AssetId.ToString());
			continue;
		}

		const TWeakPtr<FStreamableHandle> WeakLoadHandle = LoadHandle;
		LoadHandle->BindCompleteDelegate(
			FStreamableDelegate::CreateWeakLambda(
				this,
				[AssetId, WeakLoadHandle]()
				{
					const TSharedPtr<FStreamableHandle> CompletedHandle =
						WeakLoadHandle.Pin();
					if (CompletedHandle.IsValid() && CompletedHandle->HasError())
					{
						UE_LOG(
							InventoryComponentLog,
							Error,
							TEXT("Weapon presentation assets failed to load for item '%s'."),
							*AssetId.ToString());
					}
				}));

		TArray<TSharedPtr<FStreamableHandle>> LoadHandles;
		LoadHandles.Add(MoveTemp(LoadHandle));
		PandoraWeaponPresentationLoadHandles.Add(
			AssetId,
			MoveTemp(LoadHandles));
	}
}

void UInventoryComponent::ReleasePandoraWeaponLoadoutPresentationAssets()
{
	for (TPair<FPrimaryAssetId, TArray<TSharedPtr<FStreamableHandle>>>& HandlePair :
		PandoraWeaponPresentationLoadHandles)
	{
		for (const TSharedPtr<FStreamableHandle>& LoadHandle : HandlePair.Value)
		{
			if (LoadHandle.IsValid())
			{
				LoadHandle->ReleaseHandle();
			}
		}
	}
	PandoraWeaponPresentationLoadHandles.Reset();
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
		ReplicatedEntries.MarkEntryDirty(*ExistingEntry);
		MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, ReplicatedEntries, this);
	}
	else
	{
		FReplicatedInventoryEntry& NewEntry = ReplicatedEntries.Entries.AddDefaulted_GetRef();
		NewEntry.ItemId = ItemInstance->GetItemId();
		NewEntry.ItemDefinition = ItemInstance->ItemDefinition;
		NewEntry.Quantity = ItemInstance->Quantity;
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

void UInventoryComponent::ServerSplitConsumableStack_Implementation(const FGuid ItemId)
{
	SplitConsumableStack(ItemId);
}

void UInventoryComponent::ServerMergeConsumableStacks_Implementation(const FGuid SourceItemId, const FGuid TargetItemId)
{
	MergeConsumableStacks(SourceItemId, TargetItemId);
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

bool UInventoryComponent::SetPandoraWeaponLoadoutItemId(
	const EEnum_Direction Direction,
	const FGuid ItemId)
{
	if (!HasInventoryAuthority())
	{
		return false;
	}

	const int32 SlotIndex = GetPandoraWeaponLoadoutIndex(Direction);
	if (SlotIndex == INDEX_NONE)
	{
		return false;
	}

	EnsurePandoraWeaponLoadoutArray();
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
		return true;
	}

	MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, PandoraWeaponLoadoutItemIds, this);
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
	RefreshPandoraWeaponLoadoutPresentationAssets();
	OnPandoraWeaponLoadoutChanged.Broadcast();
	return true;
}

bool UInventoryComponent::ClearPandoraWeaponLoadoutReferencesToItem(const FGuid ItemId)
{
	if (!HasInventoryAuthority() || !ItemId.IsValid())
	{
		return false;
	}

	EnsurePandoraWeaponLoadoutArray();
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
	return true;
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
	const bool bApplied = AppliedHandle.WasSuccessfullyApplied();

	return bApplied;
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
