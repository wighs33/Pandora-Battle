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

DECLARE_LOG_CATEGORY_EXTERN(InventoryComponentLog, Log, All);

USTRUCT(BlueprintType)
struct FItemList
{
	GENERATED_BODY()

public:
	// UI/BP에서 사용하는 런타임 캐시입니다. 실제 복제 원본 데이터는 ReplicatedEntries에 있습니다.
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

	// 장착 처리와 서버 검증에서 사용하는 안정적인 아이템 식별자입니다.
	UPROPERTY()
	FGuid ItemId;

	// 클라이언트에서 런타임 아이템 인스턴스를 다시 만들 때 필요한 복제 데이터입니다.
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

	// 모든 변경은 Dirty 표시를 해야 Iris가 변경분만 델타 복제할 수 있습니다.
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

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UInventoryComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

	friend struct FReplicatedInventoryEntry;

public:
	UInventoryComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 아이템 추가
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Inventory")
	void AddItemsByPrimaryAssetIds(const TArray<FPrimaryAssetId>& ItemDefinitions);

	// 아이템 하나를 런타임 캐시와 복제 엔트리에서 함께 제거
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Inventory")
	bool RemoveItemById(FGuid ItemId);
	
	// 새 수량이 0 이하이면 RemoveItemById와 같은 경로로 제거
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Inventory")
	bool SetItemQuantity(FGuid ItemId, int32 NewQuantity);

	// 내부 분류 helper
	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void FilterItem(UItemInstance* ItemInstance);

	// 카테고리별 맵을 다시 만들 때 사용하는 내부 helper입니다.
	// 이 함수 자체는 인벤토리 아이템을 생성, 제거, 복제하지 않습니다.
	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void AddValueToMap(FGameplayTag TypeTag, UItemInstance* ItemInstance);

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	FGuid GetOrCreateItemId(UItemInstance* ItemInstance);

	UFUNCTION(BlueprintPure, Category = "!Inventory")
	UItemInstance* FindItemInstanceById(FGuid ItemId) const;

protected:
	// AllItemList에 이미 들어있는 에디터/런타임 기본 아이템을 복제 엔트리로 변환합니다.
	void InitializeReplicatedEntriesFromRuntimeItems();

	// 복제된 값 데이터 기준으로 런타임 UObject 인스턴스를 다시 만듭니다.
	// 주로 클라이언트에서 게임플레이 코드가 캐시를 조회하기 전에 FastArray 상태를 먼저 받았을 때 사용합니다.
	void RebuildRuntimeItemsFromReplicatedEntries();

	// AllItemList를 기준으로 각 아이템에 FilterItem을 호출해 Map_Type_ItemList를 다시 만듭니다.
	void RebuildFilteredItemMap();

	// FastArray 콜백은 여기로 모아서, 캐시 재구성 로직을 컴포넌트가 직접 소유하게 합니다.
	void HandleReplicatedEntryAddedOrChanged(const FReplicatedInventoryEntry& Entry);
	void HandleReplicatedEntryRemoved(FGuid ItemId);

	// 런타임 아이템 인스턴스에 대응하는 복제 원본 엔트리를 추가하거나 갱신합니다.
	void AddReplicatedItem(UItemInstance* ItemInstance);

	// 공개 API가 사용하는 내부 변경 helper입니다.
	// 이 경로를 분리해두면 런타임 캐시와 복제 엔트리를 항상 같은 상태로 맞추기 쉬워집니다.
	bool RemoveReplicatedItemById(FGuid ItemId);
	bool SetReplicatedItemQuantityById(FGuid ItemId, int32 NewQuantity);

	// 현재 정적 필터 정책은 기존 블루프린트의 로컬 변수 목록과 동일하게 유지합니다.
	static const TArray<FGameplayTag>& GetFilterTypeTags();

	int32 FindReplicatedEntryIndexById(FGuid ItemId) const;
	FReplicatedInventoryEntry* FindReplicatedEntryById(FGuid ItemId);
	const FReplicatedInventoryEntry* FindReplicatedEntryById(FGuid ItemId) const;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	FItemList AllItemList;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	TMap<FGameplayTag, FItemList> Map_Type_ItemList;

protected:
	UPROPERTY(Replicated)
	FReplicatedInventoryList ReplicatedEntries;
};
