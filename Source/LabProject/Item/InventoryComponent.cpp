#include "Item/InventoryComponent.h"

#include "Common/ProjectTagConfig.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(InventoryComponent)

DEFINE_LOG_CATEGORY(InventoryComponentLog);

/** 엔트리 추가 복제 후 캐시를 갱신합니다. */
void FReplicatedInventoryEntry::PostReplicatedAdd(const FReplicatedInventoryList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->HandleReplicatedEntryAddedOrChanged(*this);
	}
}

/** 엔트리 변경 복제 후 캐시를 갱신합니다. */
void FReplicatedInventoryEntry::PostReplicatedChange(const FReplicatedInventoryList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->HandleReplicatedEntryAddedOrChanged(*this);
	}
}

/** 엔트리 제거 복제 전에 캐시 제거를 요청합니다. */
void FReplicatedInventoryEntry::PreReplicatedRemove(const FReplicatedInventoryList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->HandleReplicatedEntryRemoved(ItemId);
	}
}

/** 인벤토리 컴포넌트 기본 상태를 초기화합니다. */
UInventoryComponent::UInventoryComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// =================================================================================================================
	// === 기본 설정

	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	ReplicatedEntries.Owner = this;
}

/** 시작 시 캐시와 복제 상태를 초기화합니다. */
void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// =================================================================================================================
	// === Owner 포인터 보정

	ReplicatedEntries.Owner = this;

	UProjectTagConfig::Get(this)->GetItemFilterTypeTags(FilterTypeTags);
	// =================================================================================================================
	// === 권한별 초기화
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		InitializeReplicatedEntriesFromRuntimeItems();
		RebuildFilteredItemMap();
	}
	else
	{
		RebuildRuntimeItemsFromReplicatedEntries();
	}
}

/** 복제 프로퍼티를 등록합니다. */
void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UInventoryComponent, ReplicatedEntries, Params);
}

/** PrimaryAssetId 배열로 아이템을 추가합니다. */
void UInventoryComponent::AddItemsByPrimaryAssetIds(const TArray<FPrimaryAssetId>& ItemDefinitions)
{
	// =================================================================================================================
	// === 서버 권한 검사
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(InventoryComponentLog, Warning, TEXT("[InventoryAdd] failed: inventory can only be modified on authority owner=%s"),
			*GetNameSafe(GetOwner()));
		return;
	}

	// =================================================================================================================
	// === 빈 입력 처리

	if (ItemDefinitions.IsEmpty())
	{
		RebuildFilteredItemMap();
		return;
	}

	// =================================================================================================================
	// === 에셋 비동기 로드

	UAssetManager& AssetManager = UAssetManager::Get();
	TSharedPtr<FStreamableHandle> LoadHandle = AssetManager.LoadPrimaryAssets(
		ItemDefinitions,
		{},
		FStreamableDelegate::CreateWeakLambda(this, [this, ItemDefinitions]()
		{
			UAssetManager& LoadedAssetManager = UAssetManager::Get();

			// =================================================================================================================
			// === 로드 완료 후 아이템 생성

			for (const FPrimaryAssetId& ItemDefinitionId : ItemDefinitions)
			{
				const UItemDefinition* ItemDefinition = Cast<UItemDefinition>(LoadedAssetManager.GetPrimaryAssetObject(ItemDefinitionId));
				if (!IsValid(ItemDefinition))
				{
					UE_LOG(InventoryComponentLog, Warning, TEXT("[InventoryAdd] failed: could not resolve item definition=%s owner=%s"),
						*ItemDefinitionId.ToString(),
						*GetNameSafe(GetOwner()));
					continue;
				}

				UItemInstance* NewItemInstance = NewObject<UItemInstance>(this);
				NewItemInstance->ItemDefinition = ItemDefinition;
				NewItemInstance->Quantity = 1;
				AddReplicatedItem(NewItemInstance);
				FilterItem(NewItemInstance);
			}

			PendingItemLoadHandles.RemoveAll(
				[](const TSharedPtr<FStreamableHandle>& PendingHandle)
				{
					return !PendingHandle.IsValid() || PendingHandle->HasLoadCompleted();
				});
		}));

	if (LoadHandle.IsValid())
	{
		PendingItemLoadHandles.Add(LoadHandle);
	}
	else
	{
		UE_LOG(InventoryComponentLog, Warning, TEXT("[InventoryAdd] failed: could not request load count=%d owner=%s"),
			ItemDefinitions.Num(),
			*GetNameSafe(GetOwner()));
	}
}

/** ItemId로 아이템을 제거합니다. */
bool UInventoryComponent::RemoveItemById(FGuid ItemId)
{
	// =================================================================================================================
	// === 서버 권한 검사
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(InventoryComponentLog, Warning, TEXT("RemoveItemById failed: inventory can only be modified on the authority."));
		return false;
	}

	return RemoveReplicatedItemById(ItemId);
}

/** ItemId로 수량을 바꿉니다. */
bool UInventoryComponent::SetItemQuantity(FGuid ItemId, int32 NewQuantity)
{
	// =================================================================================================================
	// === 서버 권한 검사
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(InventoryComponentLog, Warning, TEXT("SetItemQuantity failed: inventory can only be modified on the authority."));
		return false;
	}

	return SetReplicatedItemQuantityById(ItemId, NewQuantity);
}

/** 아이템을 필터 맵에 분류합니다. */
void UInventoryComponent::FilterItem(UItemInstance* ItemInstance)
{
	// =================================================================================================================
	// === 정의 데이터 검사
	const UItemDefinition* ItemDefinition = IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
	if (!ItemDefinition)
	{
		UE_LOG(InventoryComponentLog, Warning, TEXT("[InventoryFilter] skipped: invalid item or definition item=%s"),
			*GetNameSafe(ItemInstance));
		return;
	}

	if (!ItemDefinition->IdTag.IsValid())
	{
		UE_LOG(
			InventoryComponentLog,
			Warning,
			TEXT("FilterItem skipped item '%s': invalid IdTag."),
			*GetNameSafe(ItemDefinition));
		return;
	}

	if (FilterTypeTags.IsEmpty())
	{
		UE_LOG(InventoryComponentLog, Warning, TEXT("[InventoryFilter] FilterItem skipped: FilterTypeTags is empty. item=%s definition=%s idTag=%s owner=%s"),
			*GetNameSafe(ItemInstance),
			*GetNameSafe(ItemDefinition),
			*ItemDefinition->IdTag.ToString(),
			*GetNameSafe(GetOwner()));
		return;
	}

	// =================================================================================================================
	// === 타입 태그 매칭

	bool bMatchedAnyType = false;
	for (const FGameplayTag& TypeTag : FilterTypeTags)
	{
		if (ItemDefinition->IdTag.MatchesTag(TypeTag))
		{
			AddValueToMap(TypeTag, ItemInstance);
			bMatchedAnyType = true;
			UE_LOG(InventoryComponentLog, Log, TEXT("[InventoryFilter] Item matched filter type: item=%s definition=%s idTag=%s typeTag=%s"),
				*GetNameSafe(ItemInstance),
				*GetNameSafe(ItemDefinition),
				*ItemDefinition->IdTag.ToString(),
				*TypeTag.ToString());
		}
	}

	if (!bMatchedAnyType)
	{
		UE_LOG(InventoryComponentLog, Warning, TEXT("[InventoryFilter] Item did not match any filter type: item=%s definition=%s idTag=%s filterTypeCount=%d"),
			*GetNameSafe(ItemInstance),
			*GetNameSafe(ItemDefinition),
			*ItemDefinition->IdTag.ToString(),
			FilterTypeTags.Num());
	}
}

/** 타입 태그 맵에 아이템을 추가합니다. */
void UInventoryComponent::AddValueToMap(FGameplayTag TypeTag, UItemInstance* ItemInstance)
{
	if (!IsValid(ItemInstance))
	{
		return;
	}

	FItemList& ItemList = Map_Type_ItemList.FindOrAdd(TypeTag);
	ItemList.Items.AddUnique(ItemInstance);
	UE_LOG(InventoryComponentLog, Log, TEXT("[InventoryFilter] AddValueToMap: owner=%s typeTag=%s item=%s definition=%s typeCount=%d"),
		*GetNameSafe(GetOwner()),
		*TypeTag.ToString(),
		*GetNameSafe(ItemInstance),
		*GetNameSafe(ItemInstance ? ItemInstance->ItemDefinition.Get() : nullptr),
		ItemList.Items.Num());
}

/** 아이템의 ItemId를 반환하거나 새로 만듭니다. */
FGuid UInventoryComponent::GetOrCreateItemId(UItemInstance* ItemInstance)
{
	return IsValid(ItemInstance) ? ItemInstance->GetOrCreateItemId() : FGuid();
}

/** ItemId로 런타임 아이템 인스턴스를 찾습니다. */
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

void UInventoryComponent::ApplyProjectTagConfig(const UProjectTagConfig* ProjectTagConfig)
{
	const UProjectTagConfig* EffectiveConfig = ProjectTagConfig ? ProjectTagConfig : UProjectTagConfig::GetDefaultConfig();
	EffectiveConfig->GetItemFilterTypeTags(FilterTypeTags);

	RebuildFilteredItemMap();
	OnInventoryChanged.Broadcast();
}

/** 기존 런타임 아이템을 복제 엔트리로 변환합니다. */
void UInventoryComponent::InitializeReplicatedEntriesFromRuntimeItems()
{
	for (UItemInstance* ItemInstance : AllItemList.Items)
	{
		AddReplicatedItem(ItemInstance);
	}
}

/** 복제 엔트리 기준으로 런타임 캐시를 다시 만듭니다. */
void UInventoryComponent::RebuildRuntimeItemsFromReplicatedEntries()
{
	// =================================================================================================================
	// === 런타임 캐시 초기화
	AllItemList.Items.Reset();

	// =================================================================================================================
	// === 복제 엔트리 기반 재구성
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

/** 현재 런타임 캐시 기준으로 필터 맵을 다시 만듭니다. */
void UInventoryComponent::RebuildFilteredItemMap()
{
	UE_LOG(InventoryComponentLog, Log, TEXT("[InventoryFilter] RebuildFilteredItemMap started: owner=%s allCount=%d filterTypeCount=%d"),
		*GetNameSafe(GetOwner()),
		AllItemList.Items.Num(),
		FilterTypeTags.Num());

	Map_Type_ItemList.Reset();

	if (FilterTypeTags.IsEmpty())
	{
		UE_LOG(InventoryComponentLog, Warning, TEXT("[InventoryFilter] RebuildFilteredItemMap skipped: FilterTypeTags is empty. owner=%s"),
			*GetNameSafe(GetOwner()));
		return;
	}

	for (UItemInstance* ItemInstance : AllItemList.Items)
	{
		FilterItem(ItemInstance);
	}

	UE_LOG(InventoryComponentLog, Log, TEXT("[InventoryFilter] RebuildFilteredItemMap completed: owner=%s mapTypes=%d"),
		*GetNameSafe(GetOwner()),
		Map_Type_ItemList.Num());
}

/** 복제 엔트리 추가 또는 변경을 반영합니다. */
void UInventoryComponent::HandleReplicatedEntryAddedOrChanged(const FReplicatedInventoryEntry& Entry)
{
	// =================================================================================================================
	// === 엔트리 유효성 검사
	if (!Entry.ItemId.IsValid() || !IsValid(Entry.ItemDefinition))
	{
		UE_LOG(InventoryComponentLog, Warning, TEXT("[InventoryRep] skipped invalid replicated entry itemId=%s definition=%s owner=%s"),
			*Entry.ItemId.ToString(),
			*GetNameSafe(Entry.ItemDefinition),
			*GetNameSafe(GetOwner()));
		return;
	}

	// =================================================================================================================
	// === 기존 캐시 조회 또는 새 생성

	UItemInstance* ItemInstance = FindItemInstanceById(Entry.ItemId);
	const bool bWasNewItemInstance = !IsValid(ItemInstance);
	const UItemDefinition* PreviousItemDefinition = bWasNewItemInstance ? nullptr : ItemInstance->ItemDefinition.Get();
	if (!IsValid(ItemInstance))
	{
		ItemInstance = NewObject<UItemInstance>(this);
		AllItemList.Items.Add(ItemInstance);
	}

	// =================================================================================================================
	// === 캐시 동기화
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
}

/** 복제 엔트리 제거를 반영합니다. */
void UInventoryComponent::HandleReplicatedEntryRemoved(FGuid ItemId)
{
	if (!ItemId.IsValid())
	{
		return;
	}

	// =================================================================================================================
	// === 런타임 캐시에서 제거

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
	OnInventoryChanged.Broadcast();
}

/** 런타임 아이템을 복제 엔트리에 추가하거나 갱신합니다. */
void UInventoryComponent::AddReplicatedItem(UItemInstance* ItemInstance)
{
	// =================================================================================================================
	// === 서버 권한 및 입력 검사
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(InventoryComponentLog, Warning, TEXT("[InventoryCache] add skipped: no authority owner=%s item=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(ItemInstance));
		return;
	}

	if (!IsValid(ItemInstance) || !IsValid(ItemInstance->ItemDefinition))
	{
		UE_LOG(InventoryComponentLog, Warning, TEXT("[InventoryCache] add skipped: invalid item or definition owner=%s item=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(ItemInstance));
		return;
	}

	// =================================================================================================================
	// === ItemId 보장 및 런타임 캐시 추가

	ItemInstance->EnsureItemId();
	AllItemList.Items.AddUnique(ItemInstance);

	// =================================================================================================================
	// === 기존 엔트리 갱신 또는 새 엔트리 추가

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

	OnInventoryChanged.Broadcast();
}

/** ItemId로 복제 엔트리를 제거합니다. */
bool UInventoryComponent::RemoveReplicatedItemById(FGuid ItemId)
{
	if (!ItemId.IsValid())
	{
		return false;
	}

	// =================================================================================================================
	// === 엔트리 인덱스 조회

	const int32 EntryIndex = FindReplicatedEntryIndexById(ItemId);
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	// =================================================================================================================
	// === 서버 런타임 캐시 제거

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
	// === 복제 엔트리 제거

	ReplicatedEntries.Entries.RemoveAt(EntryIndex);
	ReplicatedEntries.MarkArrayDirty();
	MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, ReplicatedEntries, this);
	RebuildFilteredItemMap();
	OnInventoryChanged.Broadcast();
	return true;
}

/** ItemId로 복제 엔트리 수량을 바꿉니다. */
bool UInventoryComponent::SetReplicatedItemQuantityById(FGuid ItemId, int32 NewQuantity)
{
	if (!ItemId.IsValid())
	{
		return false;
	}

	// =================================================================================================================
	// === 0 이하 수량은 제거로 처리

	if (NewQuantity <= 0)
	{
		return RemoveReplicatedItemById(ItemId);
	}

	// =================================================================================================================
	// === 엔트리와 캐시 조회

	FReplicatedInventoryEntry* Entry = FindReplicatedEntryById(ItemId);
	UItemInstance* ItemInstance = FindItemInstanceById(ItemId);
	if (!Entry || !IsValid(ItemInstance))
	{
		return false;
	}

	// =================================================================================================================
	// === 수량 동기화
	ItemInstance->Quantity = NewQuantity;
	Entry->Quantity = NewQuantity;
	ReplicatedEntries.MarkEntryDirty(*Entry);
	MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, ReplicatedEntries, this);
	RebuildFilteredItemMap();
	OnInventoryChanged.Broadcast();
	return true;
}

/** ItemId로 복제 엔트리 인덱스를 찾습니다. */
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

/** ItemId로 복제 엔트리를 찾습니다. */
FReplicatedInventoryEntry* UInventoryComponent::FindReplicatedEntryById(FGuid ItemId)
{
	const int32 EntryIndex = FindReplicatedEntryIndexById(ItemId);
	return EntryIndex != INDEX_NONE ? &ReplicatedEntries.Entries[EntryIndex] : nullptr;
}

/** ItemId로 상수 복제 엔트리를 찾습니다. */
const FReplicatedInventoryEntry* UInventoryComponent::FindReplicatedEntryById(FGuid ItemId) const
{
	const int32 EntryIndex = FindReplicatedEntryIndexById(ItemId);
	return EntryIndex != INDEX_NONE ? &ReplicatedEntries.Entries[EntryIndex] : nullptr;
}
