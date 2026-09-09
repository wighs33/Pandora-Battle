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
DECLARE_DELEGATE_OneParam(FOnPdItemsAdded, const TArray<FPrimaryAssetId>&);
DECLARE_MULTICAST_DELEGATE(FPdInventoryChanged);
DECLARE_MULTICAST_DELEGATE(FPdEquipmentSlotsChanged);
DECLARE_MULTICAST_DELEGATE(FPdWeaponLoadoutChanged);

USTRUCT(BlueprintType)
struct FItemList
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "!Inventory")
	TArray<TObjectPtr<UItemInstance>> Items;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FEquippedItemSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "!Inventory|Equipment")
	FGameplayTag SlotTag;

	UPROPERTY(BlueprintReadOnly, Category = "!Inventory|Equipment")
	FGuid ItemId;
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

	UPROPERTY()
	int32 UpgradeLevel = 0;
};

USTRUCT()
struct LABPROJECT_API FReplicatedInventoryList : public FIrisFastArraySerializer
{
	GENERATED_BODY()

public:
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);

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

/**
 * 플레이어의 아이템 보유 상태와 슬롯 참조를 관리한다.
 *
 * 수량과 슬롯 변경을 함께 검증하고, 실제 장착 표현은 장비 컴포넌트에 맡긴다.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UInventoryComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

	friend struct FReplicatedInventoryEntry;
	friend struct FReplicatedInventoryList;

public:
	UInventoryComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Engine Callbacks
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static constexpr int32 ConsumableQuickSlotCount = 4;
	static constexpr int32 WeaponLoadoutSlotCount = 3;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Inventory")
	void AddItemsByPrimaryAssetIds(const TArray<FPrimaryAssetId>& ItemDefinitions);

	// 로딩과 추가가 완료된 실제 아이템 목록만 반환한다. 취소된 요청은 완료 통지를 보내지 않는다.
	void AddItemsByPrimaryAssetIdsWithCompletion(const TArray<FPrimaryAssetId>& ItemDefinitions, FOnPdItemsAdded OnComplete);

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

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	FGuid GetOrCreateItemId(UItemInstance* ItemInstance);

	UFUNCTION(BlueprintPure, Category = "!Inventory")
	UItemInstance* FindItemInstanceById(FGuid ItemId) const;

	const FItemList& GetAllItems() const { return AllItemList; }
	const TMap<FGameplayTag, FItemList>& GetFilteredItemMap() const { return Map_Type_ItemList; }
	UFUNCTION(BlueprintCallable, Category = "!Inventory|Quick Slots")
	bool SetConsumableQuickSlot(int32 SlotIndex, UItemInstance* ItemInstance);

	UFUNCTION(BlueprintCallable, Category = "!Inventory|Quick Slots")
	bool ClearConsumableQuickSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "!Inventory|Quick Slots")
	bool UseConsumableQuickSlot(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category = "!Inventory|Quick Slots")
	UItemInstance* GetConsumableQuickSlotItem(int32 SlotIndex) const;

	UFUNCTION(BlueprintCallable, Category = "!Inventory|Equipment")
	bool SetEquipmentSlot(FGameplayTag SlotTag, UItemInstance* ItemInstance);

	UFUNCTION(BlueprintCallable, Category = "!Inventory|Equipment")
	bool ClearEquipmentSlot(FGameplayTag SlotTag);

	UFUNCTION(BlueprintPure, Category = "!Inventory|Equipment")
	FGuid GetEquipmentSlotItemId(FGameplayTag SlotTag) const;

	UFUNCTION(BlueprintPure, Category = "!Inventory|Equipment")
	UItemInstance* GetEquipmentSlotItem(FGameplayTag SlotTag) const;

	UFUNCTION(BlueprintCallable, Category = "!Inventory|Weapon Loadout")
	bool AssignWeaponToLoadoutSlot(EEnum_Direction Direction, UItemInstance* WeaponInstance);

	UFUNCTION(BlueprintCallable, Category = "!Inventory|Weapon Loadout")
	bool ClearWeaponFromLoadoutSlot(EEnum_Direction Direction);

	UFUNCTION(BlueprintPure, Category = "!Inventory|Weapon Loadout")
	FGuid GetWeaponIdForLoadoutSlot(EEnum_Direction Direction) const;

	UFUNCTION(BlueprintPure, Category = "!Inventory|Weapon Loadout")
	UItemInstance* FindWeaponForLoadoutSlot(EEnum_Direction Direction) const;

	UFUNCTION(BlueprintCallable, Category = "!Inventory|Stack")
	bool SplitConsumableStack(FGuid ItemId);

	UFUNCTION(BlueprintCallable, Category = "!Inventory|Stack")
	bool MergeConsumableStacks(FGuid SourceItemId, FGuid TargetItemId);

	UFUNCTION(BlueprintCallable, Category = "!Inventory|Upgrade")
	bool MergeUpgradeableItems(FGuid SourceItemId, FGuid TargetItemId);

	void ApplyProjectTagConfig(const UProjectTagConfig* ProjectTagConfig);

	FPdInventoryChanged OnInventoryChanged;
	FPdEquipmentSlotsChanged OnEquipmentSlotsChanged;
	FPdWeaponLoadoutChanged OnWeaponLoadoutChanged;

protected:
	void HandleReplicatedEntryAddedOrChanged(const FReplicatedInventoryEntry& Entry);

	void HandleReplicatedEntryRemoved(FGuid ItemId);

	void FilterItem(UItemInstance* ItemInstance);
	void NotifyInventoryChanged();
	void FlushInventoryChanges();

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
	void ServerSetWeaponIdForLoadoutSlot(EEnum_Direction Direction, FGuid ItemId);

	UFUNCTION(Server, Reliable)
	void ServerSetEquipmentSlot(FGameplayTag SlotTag, FGuid ItemId);

	UFUNCTION(Server, Reliable)
	void ServerSplitConsumableStack(FGuid ItemId);

	UFUNCTION(Server, Reliable)
	void ServerMergeConsumableStacks(FGuid SourceItemId, FGuid TargetItemId);

	UFUNCTION(Server, Reliable)
	void ServerMergeUpgradeableItems(FGuid SourceItemId, FGuid TargetItemId);

	bool HasInventoryAuthority() const;
	void SetItemQuantityByPrimaryAssetIdInternal(
		FPrimaryAssetId ItemDefinitionId,
		int32 Quantity,
		int32 ConsumableQuickSlotIndex);
	void CleanupCompletedItemLoadHandles();
	void CompletePendingItemLoadRequest(uint64 RequestGeneration);
	void CancelPendingItemLoads();
	void RefreshWeaponLoadoutPresentationAssets();
	void ReleaseWeaponLoadoutPresentationAssets();
	void EnsureConsumableQuickSlotArray();
	void EnsureWeaponLoadoutSlotCount();
	bool IsValidConsumableQuickSlotIndex(int32 SlotIndex) const;
	bool SetConsumableQuickSlotItemId(int32 SlotIndex, FGuid ItemId);
	bool ClearConsumableQuickSlotReferencesToItem(FGuid ItemId);
	FGameplayTag ResolveEquipmentSlotTag(FGameplayTag SlotTag) const;
	int32 FindEquipmentSlotIndex(FGameplayTag SlotTag) const;
	bool SetEquipmentSlotItemId(FGameplayTag SlotTag, FGuid ItemId);
	bool ClearEquipmentSlotReferencesToItem(FGuid ItemId);
	bool SetWeaponIdForLoadoutSlot(EEnum_Direction Direction, FGuid ItemId);
	bool ClearLoadoutSlotsReferencingWeapon(FGuid ItemId);
	bool IsConsumableItem(const UItemInstance* ItemInstance) const;
	bool IsWeaponItem(const UItemInstance* ItemInstance) const;
	bool IsUpgradeableItem(const UItemInstance* ItemInstance) const;
	bool ApplyConsumableItemEffect(const UItemInstance* ItemInstance) const;
	UItemInstance* FindFirstItemInstanceByDefinition(const UItemDefinition* ItemDefinition) const;

	UFUNCTION()
	void OnRep_WeaponIdsByLoadoutSlot();

	UFUNCTION()
	void OnRep_EquippedItemSlots();

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Inventory|Filter", meta = (AllowPrivateAccess = "true"))
	TArray<FGameplayTag> FilterTypeTags;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Inventory", meta = (AllowPrivateAccess = "true"))
	FItemList AllItemList;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Inventory", meta = (AllowPrivateAccess = "true"))
	TMap<FGameplayTag, FItemList> Map_Type_ItemList;

	int32 InventoryUpdateDepth = 0;
	bool bInventoryChangePending = false;

protected:
	UPROPERTY(Replicated)
	FReplicatedInventoryList ReplicatedEntries;

	UPROPERTY(ReplicatedUsing = OnRep_ConsumableQuickSlotItemIds)
	TArray<FGuid> ConsumableQuickSlotItemIds;

	UPROPERTY(ReplicatedUsing = OnRep_WeaponIdsByLoadoutSlot)
	TArray<FGuid> WeaponIdsByLoadoutSlot;

	UPROPERTY(ReplicatedUsing = OnRep_EquippedItemSlots)
	TArray<FEquippedItemSlot> EquippedItemSlots;

	uint64 ItemLoadGeneration = 0;
	int32 PendingItemLoadRequestCount = 0;
	TArray<TSharedPtr<FStreamableHandle>> PendingItemLoadHandles;
	TMap<FPrimaryAssetId, TArray<TSharedPtr<FStreamableHandle>>> PandoraWeaponPresentationLoadHandles;
};
