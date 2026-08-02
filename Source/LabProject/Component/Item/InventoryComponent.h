#pragma once

#include "Common/Enum_Direction.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "UObject/PrimaryAssetId.h"
#include "Components/PlayerStateComponent.h"
#include "InventoryComponent.generated.h"

class UInventoryComponent;
class UItemDefinition;
class UItemInstance;
class UProjectTagConfig;
struct FStreamableHandle;

DECLARE_LOG_CATEGORY_EXTERN(InventoryComponentLog, Log, All);
DECLARE_MULTICAST_DELEGATE(FPdInventoryChanged);
DECLARE_MULTICAST_DELEGATE(FPdPandoraWeaponLoadoutChanged);

USTRUCT(BlueprintType)
struct FItemList
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	TArray<TObjectPtr<UItemInstance>> Items;
};

USTRUCT()
struct LABPROJECT_API FReplicatedInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

public:
	void PostReplicatedAdd(const struct FReplicatedInventoryList& InArraySerializer);

	void PostReplicatedChange(const struct FReplicatedInventoryList& InArraySerializer);

	void PreReplicatedRemove(const struct FReplicatedInventoryList& InArraySerializer);

	UPROPERTY()
	FGuid ItemId;

	UPROPERTY()
	TObjectPtr<const UItemDefinition> ItemDefinition = nullptr;

	UPROPERTY()
	int32 Quantity = 0;
};

USTRUCT()
struct LABPROJECT_API FReplicatedInventoryList : public FIrisFastArraySerializer
{
	GENERATED_BODY()

public:
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FReplicatedInventoryEntry, FReplicatedInventoryList>(Entries, DeltaParms, *this);
	}

	void MarkEntryDirty(FReplicatedInventoryEntry& Entry)
	{
		MarkItemDirty(Entry);
	}

	UPROPERTY()
	TArray<FReplicatedInventoryEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UInventoryComponent> Owner = nullptr;
};

template<>
struct TStructOpsTypeTraits<FReplicatedInventoryList> : public TStructOpsTypeTraitsBase2<FReplicatedInventoryList>
{
	enum { WithNetDeltaSerializer = true };
};

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UInventoryComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

	friend struct FReplicatedInventoryEntry;

public:
	UInventoryComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Timing hooks
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static constexpr int32 ConsumableQuickSlotCount = 4;
	static constexpr int32 PandoraWeaponLoadoutSlotCount = 3;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Inventory")
	void AddItemsByPrimaryAssetIds(const TArray<FPrimaryAssetId>& ItemDefinitions);

	/**
	 * Includes requests whose Asset Manager completion delegate is queued even
	 * when no streamable handle was returned (for example, an already-loaded
	 * primary asset). This is intentionally a native-only readiness query.
	 */
	bool HasPendingItemLoads();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Inventory|Stack")
	void SetItemQuantityByPrimaryAssetId(FPrimaryAssetId ItemDefinitionId, int32 Quantity);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Inventory|Quick Slots")
	void SetConsumableItemQuantityAndQuickSlotByPrimaryAssetId(
		FPrimaryAssetId ItemDefinitionId,
		int32 Quantity,
		int32 SlotIndex);

	UFUNCTION()
	void ClearAllItems();
	void FilterItem(UItemInstance* ItemInstance);

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void AddValueToMap(FGameplayTag TypeTag, UItemInstance* ItemInstance);

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	FGuid GetOrCreateItemId(UItemInstance* ItemInstance);

	UFUNCTION(BlueprintPure, Category = "!Inventory")
	UItemInstance* FindItemInstanceById(FGuid ItemId) const;

	const FItemList& GetAllItems() const { return AllItemList; }
	void GetOwnedOrPendingItemDefinitionIds(
		TSet<FPrimaryAssetId>& OutItemDefinitionIds) const;
	const TMap<FGameplayTag, FItemList>& GetFilteredItemMap() const { return Map_Type_ItemList; }
	UFUNCTION(BlueprintCallable, Category = "!Inventory|Quick Slots")
	bool SetConsumableQuickSlot(int32 SlotIndex, UItemInstance* ItemInstance);

	UFUNCTION(BlueprintCallable, Category = "!Inventory|Quick Slots")
	bool ClearConsumableQuickSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "!Inventory|Quick Slots")
	bool UseConsumableQuickSlot(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category = "!Inventory|Quick Slots")
	UItemInstance* GetConsumableQuickSlotItem(int32 SlotIndex) const;

	UFUNCTION(BlueprintCallable, Category = "!Inventory|Pandora Weapon Loadout")
	bool SetPandoraWeaponLoadoutSlot(EEnum_Direction Direction, UItemInstance* WeaponInstance);

	UFUNCTION(BlueprintCallable, Category = "!Inventory|Pandora Weapon Loadout")
	bool ClearPandoraWeaponLoadoutSlot(EEnum_Direction Direction);

	UFUNCTION(BlueprintPure, Category = "!Inventory|Pandora Weapon Loadout")
	FGuid GetPandoraWeaponLoadoutItemId(EEnum_Direction Direction) const;

	UFUNCTION(BlueprintPure, Category = "!Inventory|Pandora Weapon Loadout")
	UItemInstance* GetPandoraWeaponLoadoutItem(EEnum_Direction Direction) const;

	UFUNCTION(BlueprintCallable, Category = "!Inventory|Stack")
	bool SplitConsumableStack(FGuid ItemId);

	UFUNCTION(BlueprintCallable, Category = "!Inventory|Stack")
	bool MergeConsumableStacks(FGuid SourceItemId, FGuid TargetItemId);

	void ApplyProjectTagConfig(const UProjectTagConfig* ProjectTagConfig);

	FPdInventoryChanged OnInventoryChanged;
	FPdPandoraWeaponLoadoutChanged OnPandoraWeaponLoadoutChanged;

protected:
	// Replication timing callbacks
	void HandleReplicatedEntryAddedOrChanged(const FReplicatedInventoryEntry& Entry);

	void HandleReplicatedEntryRemoved(FGuid ItemId);

	// State rebuild helpers
	void InitializeReplicatedEntriesFromRuntimeItems();

	void RebuildRuntimeItemsFromReplicatedEntries();

	void RebuildFilteredItemMap();

	void AddReplicatedItem(UItemInstance* ItemInstance);

	bool RemoveReplicatedItemById(FGuid ItemId);

	bool SetReplicatedItemQuantityById(FGuid ItemId, int32 NewQuantity);


	int32 FindReplicatedEntryIndexById(FGuid ItemId) const;

	FReplicatedInventoryEntry* FindReplicatedEntryById(FGuid ItemId);

	const FReplicatedInventoryEntry* FindReplicatedEntryById(FGuid ItemId) const;

	UFUNCTION()
	void OnRep_ConsumableQuickSlotItemIds();

	UFUNCTION(Server, Reliable)
	void ServerSetConsumableQuickSlot(int32 SlotIndex, FGuid ItemId);

	UFUNCTION(Server, Reliable)
	void ServerClearConsumableQuickSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerUseConsumableQuickSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerSetPandoraWeaponLoadoutSlot(EEnum_Direction Direction, FGuid ItemId);

	UFUNCTION(Server, Reliable)
	void ServerSplitConsumableStack(FGuid ItemId);

	UFUNCTION(Server, Reliable)
	void ServerMergeConsumableStacks(FGuid SourceItemId, FGuid TargetItemId);

	bool HasInventoryAuthority() const;
	void SetItemQuantityByPrimaryAssetIdInternal(
		FPrimaryAssetId ItemDefinitionId,
		int32 Quantity,
		int32 ConsumableQuickSlotIndex);
	void CleanupCompletedItemLoadHandles();
	void CompletePendingItemLoadRequest(uint64 RequestGeneration);
	void CancelPendingItemLoads();
	void TrackPendingItemDefinitionRequests(
		const TArray<FPrimaryAssetId>& ItemDefinitionIds);
	void ReleasePendingItemDefinitionRequests(
		const TArray<FPrimaryAssetId>& ItemDefinitionIds);
	void RefreshPandoraWeaponLoadoutPresentationAssets();
	void ReleasePandoraWeaponLoadoutPresentationAssets();
	void EnsureConsumableQuickSlotArray();
	void EnsurePandoraWeaponLoadoutArray();
	bool IsValidConsumableQuickSlotIndex(int32 SlotIndex) const;
	bool SetConsumableQuickSlotItemId(int32 SlotIndex, FGuid ItemId);
	bool ClearConsumableQuickSlotReferencesToItem(FGuid ItemId);
	bool SetPandoraWeaponLoadoutItemId(EEnum_Direction Direction, FGuid ItemId);
	bool ClearPandoraWeaponLoadoutReferencesToItem(FGuid ItemId);
	bool IsConsumableItem(const UItemInstance* ItemInstance) const;
	bool IsWeaponItem(const UItemInstance* ItemInstance) const;
	bool ApplyConsumableItemEffect(const UItemInstance* ItemInstance) const;
	UItemInstance* FindFirstItemInstanceByDefinition(const UItemDefinition* ItemDefinition) const;

	UFUNCTION()
	void OnRep_PandoraWeaponLoadoutItemIds();

public:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Inventory|Filter")
	TArray<FGameplayTag> FilterTypeTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	FItemList AllItemList;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	TMap<FGameplayTag, FItemList> Map_Type_ItemList;

protected:
	UPROPERTY(Replicated)
	FReplicatedInventoryList ReplicatedEntries;

	UPROPERTY(ReplicatedUsing = OnRep_ConsumableQuickSlotItemIds)
	TArray<FGuid> ConsumableQuickSlotItemIds;

	UPROPERTY(ReplicatedUsing = OnRep_PandoraWeaponLoadoutItemIds)
	TArray<FGuid> PandoraWeaponLoadoutItemIds;

	uint64 ItemLoadGeneration = 0;
	int32 PendingItemLoadRequestCount = 0;
	TArray<TSharedPtr<FStreamableHandle>> PendingItemLoadHandles;
	TMap<FPrimaryAssetId, int32> PendingItemDefinitionRequestCounts;
	TMap<FPrimaryAssetId, TArray<TSharedPtr<FStreamableHandle>>> PandoraWeaponPresentationLoadHandles;
};
