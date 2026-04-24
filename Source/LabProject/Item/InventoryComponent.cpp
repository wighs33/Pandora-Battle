#include "Item/InventoryComponent.h"

#include "Engine/AssetManager.h"
#include "Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
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
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	ReplicatedEntries.Owner = this;
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	ReplicatedEntries.Owner = this;

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

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UInventoryComponent, ReplicatedEntries);
}

void UInventoryComponent::AddItemsByPrimaryAssetIds(const TArray<FPrimaryAssetId>& ItemDefinitions)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(InventoryComponentLog, Warning, TEXT("AddItemsByPrimaryAssetIds failed: inventory can only be modified on the authority."));
		return;
	}

	if (ItemDefinitions.IsEmpty())
	{
		RebuildFilteredItemMap();
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	AssetManager.LoadPrimaryAssets(
		ItemDefinitions,
		{},
		FStreamableDelegate::CreateWeakLambda(this, [this, ItemDefinitions]()
		{
			UAssetManager& LoadedAssetManager = UAssetManager::Get();

			for (const FPrimaryAssetId& ItemDefinitionId : ItemDefinitions)
			{
				const UItemDefinition* ItemDefinition = Cast<UItemDefinition>(LoadedAssetManager.GetPrimaryAssetObject(ItemDefinitionId));
				if (!IsValid(ItemDefinition))
				{
					UE_LOG(InventoryComponentLog, Warning, TEXT("AddItemsByPrimaryAssetIds failed: could not resolve item definition '%s'."), *ItemDefinitionId.ToString());
					continue;
				}

				// 복제 엔트리에는 값 데이터만 저장합니다.
				// UObject 인스턴스는 런타임 캐시로 유지되고, 클라이언트에서는 복제 원본 기준으로 다시 만들어집니다.
				UItemInstance* NewItemInstance = NewObject<UItemInstance>(this);
				NewItemInstance->ItemDefinition = ItemDefinition;
				NewItemInstance->Quantity = 1;
				AddReplicatedItem(NewItemInstance);
			}

			// 배치 추가가 모두 끝난 뒤 카테고리 맵을 한 번 다시 만들어
			// 모든 런타임 캐시 뷰가 같은 상태를 보도록 맞춥니다.
			RebuildFilteredItemMap();
		}));
}

bool UInventoryComponent::RemoveItemById(FGuid ItemId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(InventoryComponentLog, Warning, TEXT("RemoveItemById failed: inventory can only be modified on the authority."));
		return false;
	}

	return RemoveReplicatedItemById(ItemId);
}

bool UInventoryComponent::SetItemQuantity(FGuid ItemId, int32 NewQuantity)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(InventoryComponentLog, Warning, TEXT("SetItemQuantity failed: inventory can only be modified on the authority."));
		return false;
	}

	return SetReplicatedItemQuantityById(ItemId, NewQuantity);
}

void UInventoryComponent::FilterItem(UItemInstance* ItemInstance)
{
	const UItemDefinition* ItemDefinition = IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
	if (!ItemDefinition)
	{
		return;
	}

	const TArray<FGameplayTag>& TypeTags = GetFilterTypeTags();
	for (const FGameplayTag& TypeTag : TypeTags)
	{
		if (ItemDefinition->IdTag.MatchesTag(TypeTag))
		{
			AddValueToMap(TypeTag, ItemInstance);
		}
	}
}

void UInventoryComponent::AddValueToMap(FGameplayTag TypeTag, UItemInstance* ItemInstance)
{
	if (!IsValid(ItemInstance))
	{
		return;
	}

	Map_Type_ItemList.FindOrAdd(TypeTag).Items.AddUnique(ItemInstance);
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

void UInventoryComponent::InitializeReplicatedEntriesFromRuntimeItems()
{
	for (UItemInstance* ItemInstance : AllItemList.Items)
	{
		AddReplicatedItem(ItemInstance);
	}
}

void UInventoryComponent::RebuildRuntimeItemsFromReplicatedEntries()
{
	AllItemList.Items.Reset();

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

	for (UItemInstance* ItemInstance : AllItemList.Items)
	{
		FilterItem(ItemInstance);
	}
}

void UInventoryComponent::HandleReplicatedEntryAddedOrChanged(const FReplicatedInventoryEntry& Entry)
{
	if (!Entry.ItemId.IsValid() || !IsValid(Entry.ItemDefinition))
	{
		return;
	}

	UItemInstance* ItemInstance = FindItemInstanceById(Entry.ItemId);
	if (!IsValid(ItemInstance))
	{
		ItemInstance = NewObject<UItemInstance>(this);
		AllItemList.Items.Add(ItemInstance);
	}

	ItemInstance->ItemId = Entry.ItemId;
	ItemInstance->ItemDefinition = Entry.ItemDefinition;
	ItemInstance->Quantity = Entry.Quantity;

	RebuildFilteredItemMap();
}

void UInventoryComponent::HandleReplicatedEntryRemoved(FGuid ItemId)
{
	if (!ItemId.IsValid())
	{
		return;
	}

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
}

void UInventoryComponent::AddReplicatedItem(UItemInstance* ItemInstance)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!IsValid(ItemInstance) || !IsValid(ItemInstance->ItemDefinition))
	{
		return;
	}

	ItemInstance->EnsureItemId();
	AllItemList.Items.AddUnique(ItemInstance);

	if (FReplicatedInventoryEntry* ExistingEntry = FindReplicatedEntryById(ItemInstance->GetItemId()))
	{
		ExistingEntry->ItemDefinition = ItemInstance->ItemDefinition;
		ExistingEntry->Quantity = ItemInstance->Quantity;
		ReplicatedEntries.MarkEntryDirty(*ExistingEntry);
	}
	else
	{
		FReplicatedInventoryEntry& NewEntry = ReplicatedEntries.Entries.AddDefaulted_GetRef();
		NewEntry.ItemId = ItemInstance->GetItemId();
		NewEntry.ItemDefinition = ItemInstance->ItemDefinition;
		NewEntry.Quantity = ItemInstance->Quantity;
		ReplicatedEntries.MarkEntryDirty(NewEntry);
	}
}

bool UInventoryComponent::RemoveReplicatedItemById(FGuid ItemId)
{
	if (!ItemId.IsValid())
	{
		return false;
	}

	const int32 EntryIndex = FindReplicatedEntryIndexById(ItemId);
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	// 런타임 캐시는 서버에서 편의상 같이 들고 있는 뷰입니다.
	// 서버 측 UI/게임플레이 조회에서 즉시 사라지도록 여기서 먼저 제거합니다.
	for (int32 Index = AllItemList.Items.Num() - 1; Index >= 0; --Index)
	{
		UItemInstance* ItemInstance = AllItemList.Items[Index];
		if (IsValid(ItemInstance) && ItemInstance->GetItemId() == ItemId)
		{
			AllItemList.Items.RemoveAt(Index);
			break;
		}
	}

	// 엔트리 제거는 배열 구조 자체를 바꾸므로 FastArray에 배열 Dirty 표시가 필요합니다.
	ReplicatedEntries.Entries.RemoveAt(EntryIndex);
	ReplicatedEntries.MarkArrayDirty();
	RebuildFilteredItemMap();
	return true;
}

bool UInventoryComponent::SetReplicatedItemQuantityById(FGuid ItemId, int32 NewQuantity)
{
	if (!ItemId.IsValid())
	{
		return false;
	}

	if (NewQuantity <= 0)
	{
		return RemoveReplicatedItemById(ItemId);
	}

	FReplicatedInventoryEntry* Entry = FindReplicatedEntryById(ItemId);
	UItemInstance* ItemInstance = FindItemInstanceById(ItemId);
	if (!Entry || !IsValid(ItemInstance))
	{
		return false;
	}

	// 수량은 두 계층에서 항상 같아야 합니다.
	// 하나는 게임플레이/UI가 읽는 런타임 UObject 캐시이고, 다른 하나는 원격 클라이언트 캐시를 다시 만드는 복제 FastArray 엔트리입니다.
	ItemInstance->Quantity = NewQuantity;
	Entry->Quantity = NewQuantity;
	ReplicatedEntries.MarkEntryDirty(*Entry);
	RebuildFilteredItemMap();
	return true;
}

const TArray<FGameplayTag>& UInventoryComponent::GetFilterTypeTags()
{
	static const TArray<FGameplayTag> TypeTags =
	{
		FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon")),
		FGameplayTag::RequestGameplayTag(TEXT("Item.Equipment")),
		FGameplayTag::RequestGameplayTag(TEXT("Item.Consumable")),
		FGameplayTag::RequestGameplayTag(TEXT("Item.Valuable")),
		FGameplayTag::RequestGameplayTag(TEXT("Item.Equipment.Hat")),
		FGameplayTag::RequestGameplayTag(TEXT("Item.Equipment.Top")),
		FGameplayTag::RequestGameplayTag(TEXT("Item.Equipment.Bottom")),
		FGameplayTag::RequestGameplayTag(TEXT("Item.Equipment.Shoes")),
		FGameplayTag::RequestGameplayTag(TEXT("Item.Equipment.Earring")),
		FGameplayTag::RequestGameplayTag(TEXT("Item.Equipment.Necklace")),
		FGameplayTag::RequestGameplayTag(TEXT("Item.Equipment.Ring")),
		FGameplayTag::RequestGameplayTag(TEXT("Item.Equipment.Rune"))
	};

	return TypeTags;
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
