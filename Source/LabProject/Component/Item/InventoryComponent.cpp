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

	// Loadout weapons are removed from the inventory view. Reject direct RPCs
	// against them as well so an equipped item cannot disappear mid-combat.
	if (PandoraWeaponLoadoutItemIds.Contains(SourceItemId)
		|| PandoraWeaponLoadoutItemIds.Contains(TargetItemId))
	{
		return false;
	}

	const int32 SourceEntryIndex = FindReplicatedEntryIndexById(SourceItemId);
	FReplicatedInventoryEntry* TargetEntry = FindReplicatedEntryById(TargetItemId);
	if (SourceEntryIndex == INDEX_NONE || !TargetEntry)
	{
		return false;
	}

	const int64 MergedUpgradeLevel =
		static_cast<int64>(SourceItem->GetUpgradeLevel())
		+ static_cast<int64>(TargetItem->GetUpgradeLevel())
		+ 1;
	const int32 NewUpgradeLevel = static_cast<int32>(FMath::Min<int64>(
		MergedUpgradeLevel,
		static_cast<int64>(MAX_int32)));
	TargetItem->SetUpgradeLevel(NewUpgradeLevel);
	TargetEntry->UpgradeLevel = NewUpgradeLevel;
	ReplicatedEntries.MarkEntryDirty(*TargetEntry);

	for (int32 ItemIndex = AllItemList.Items.Num() - 1; ItemIndex >= 0; --ItemIndex)
	{
		const UItemInstance* ItemInstance = AllItemList.Items[ItemIndex];
		if (IsValid(ItemInstance) && ItemInstance->GetItemId() == SourceItemId)
		{
			AllItemList.Items.RemoveAt(ItemIndex);
			break;
		}
	}

	ReplicatedEntries.Entries.RemoveAt(SourceEntryIndex);
	ReplicatedEntries.MarkArrayDirty();
	MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, ReplicatedEntries, this);
	ClearConsumableQuickSlotReferencesToItem(SourceItemId);
	ClearPandoraWeaponLoadoutReferencesToItem(SourceItemId);
	RebuildFilteredItemMap();
	OnInventoryChanged.Broadcast();
	return true;
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
