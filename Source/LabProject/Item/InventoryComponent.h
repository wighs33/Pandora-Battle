#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "UObject/PrimaryAssetId.h"
#include "Components/PlayerStateComponent.h"
#include "InventoryComponent.generated.h"

class UInventoryComponent;
class UItemDefinition;
class UItemInstance;

// 인벤토리 로그 카테고리입니다.
DECLARE_LOG_CATEGORY_EXTERN(InventoryComponentLog, Log, All);

/**
 * <인벤토리 런타임 캐시>
 * - UI와 BP에서 직접 보는 캐시입니다.
 * - 실제 복제 원본은 ReplicatedEntries입니다.
 */
USTRUCT(BlueprintType)
struct FItemList
{
	GENERATED_BODY()

public:
	// 런타임 아이템 캐시입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	TArray<TObjectPtr<UItemInstance>> Items;
};

/**
 * <복제용 인벤토리 엔트리>
 * - FastArray 한 칸 데이터입니다.
 * - 아이템 식별자, 정의, 수량을 가집니다.
 */
USTRUCT()
struct LABPROJECT_API FReplicatedInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

public:
	/** 엔트리 추가 복제 후 캐시를 갱신합니다. */
	void PostReplicatedAdd(const struct FReplicatedInventoryList& InArraySerializer);

	/** 엔트리 변경 복제 후 캐시를 갱신합니다. */
	void PostReplicatedChange(const struct FReplicatedInventoryList& InArraySerializer);

	/** 엔트리 제거 복제 전에 캐시 제거를 요청합니다. */
	void PreReplicatedRemove(const struct FReplicatedInventoryList& InArraySerializer);

	// 안정적인 아이템 식별자입니다.
	UPROPERTY()
	FGuid ItemId;

	// 런타임 인스턴스 재구성용 정의 데이터입니다.
	UPROPERTY()
	TObjectPtr<const UItemDefinition> ItemDefinition = nullptr;

	// 아이템 수량입니다.
	UPROPERTY()
	int32 Quantity = 0;
};

/**
 * <복제용 인벤토리 리스트>
 * - FastArray 복제 컨테이너입니다.
 * - 엔트리 델타 복제를 담당합니다.
 */
USTRUCT()
struct LABPROJECT_API FReplicatedInventoryList : public FIrisFastArraySerializer
{
	GENERATED_BODY()

public:
	/** FastArray 델타 복제를 처리합니다. */
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FReplicatedInventoryEntry, FReplicatedInventoryList>(Entries, DeltaParms, *this);
	}

	/** 엔트리를 Dirty 상태로 표시합니다. */
	void MarkEntryDirty(FReplicatedInventoryEntry& Entry)
	{
		MarkItemDirty(Entry);
	}

	// 복제 원본 엔트리 배열입니다.
	UPROPERTY()
	TArray<FReplicatedInventoryEntry> Entries;

	// 소유 인벤토리 컴포넌트입니다.
	UPROPERTY(NotReplicated)
	TObjectPtr<UInventoryComponent> Owner = nullptr;
};

template<>
struct TStructOpsTypeTraits<FReplicatedInventoryList> : public TStructOpsTypeTraitsBase2<FReplicatedInventoryList>
{
	enum { WithNetDeltaSerializer = true };
};

/**
 * <플레이어 인벤토리 컴포넌트>
 * - 런타임 캐시와 복제 원본을 함께 관리합니다.
 * - 서버에서 원본을 수정합니다.
 * - 클라는 복제값으로 캐시를 다시 만듭니다.
 */
UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UInventoryComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

	friend struct FReplicatedInventoryEntry;

public:
	/** 인벤토리 컴포넌트 기본 상태를 초기화합니다. */
	UInventoryComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 시작 시 캐시와 복제 상태를 초기화합니다. */
	virtual void BeginPlay() override;

	/** 복제 프로퍼티를 등록합니다. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** PrimaryAssetId 배열로 아이템을 추가합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Inventory")
	void AddItemsByPrimaryAssetIds(const TArray<FPrimaryAssetId>& ItemDefinitions);

	/** ItemId로 아이템을 제거합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Inventory")
	bool RemoveItemById(FGuid ItemId);
	
	/** ItemId로 수량을 바꿉니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Inventory")
	bool SetItemQuantity(FGuid ItemId, int32 NewQuantity);

	/** 아이템을 필터 맵에 분류합니다. */
	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void FilterItem(UItemInstance* ItemInstance);

	/** 타입 태그 맵에 아이템을 추가합니다. */
	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void AddValueToMap(FGameplayTag TypeTag, UItemInstance* ItemInstance);

	/** 아이템의 ItemId를 반환하거나 새로 만듭니다. */
	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	FGuid GetOrCreateItemId(UItemInstance* ItemInstance);

	/** ItemId로 런타임 아이템 인스턴스를 찾습니다. */
	UFUNCTION(BlueprintPure, Category = "!Inventory")
	UItemInstance* FindItemInstanceById(FGuid ItemId) const;

protected:
	/** 기존 런타임 아이템을 복제 엔트리로 변환합니다. */
	void InitializeReplicatedEntriesFromRuntimeItems();

	/** 복제 엔트리 기준으로 런타임 캐시를 다시 만듭니다. */
	void RebuildRuntimeItemsFromReplicatedEntries();

	/** 현재 런타임 캐시 기준으로 필터 맵을 다시 만듭니다. */
	void RebuildFilteredItemMap();

	/** 복제 엔트리 추가 또는 변경을 반영합니다. */
	void HandleReplicatedEntryAddedOrChanged(const FReplicatedInventoryEntry& Entry);

	/** 복제 엔트리 제거를 반영합니다. */
	void HandleReplicatedEntryRemoved(FGuid ItemId);

	/** 런타임 아이템을 복제 엔트리에 추가하거나 갱신합니다. */
	void AddReplicatedItem(UItemInstance* ItemInstance);

	/** ItemId로 복제 엔트리를 제거합니다. */
	bool RemoveReplicatedItemById(FGuid ItemId);

	/** ItemId로 복제 엔트리 수량을 바꿉니다. */
	bool SetReplicatedItemQuantityById(FGuid ItemId, int32 NewQuantity);

	/** 필터용 타입 태그 목록을 반환합니다. */

	/** ItemId로 복제 엔트리 인덱스를 찾습니다. */
	int32 FindReplicatedEntryIndexById(FGuid ItemId) const;

	/** ItemId로 복제 엔트리를 찾습니다. */
	FReplicatedInventoryEntry* FindReplicatedEntryById(FGuid ItemId);

	/** ItemId로 상수 복제 엔트리를 찾습니다. */
	const FReplicatedInventoryEntry* FindReplicatedEntryById(FGuid ItemId) const;

public:
	// 전체 런타임 아이템 캐시입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Inventory|Filter", meta = (Categories = "Item"))
	TArray<FGameplayTag> FilterTypeTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	FItemList AllItemList;

	// 타입별 아이템 캐시 맵입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	TMap<FGameplayTag, FItemList> Map_Type_ItemList;

protected:
	// 복제 원본 엔트리입니다.
	UPROPERTY(Replicated)
	FReplicatedInventoryList ReplicatedEntries;
};
