#include "Component/Skin/SkinComponent.h"

#include "Definition/Common/ProjectTagConfig.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Skin/SkinInstance.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinComponent)

DEFINE_LOG_CATEGORY(SkinComponentLog)

void FReplicatedSkinEntry::PostReplicatedAdd(const FReplicatedSkinList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->HandleReplicatedEntryAddedOrChanged(*this);
	}
}

void FReplicatedSkinEntry::PostReplicatedChange(const FReplicatedSkinList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->HandleReplicatedEntryAddedOrChanged(*this);
	}
}

void FReplicatedSkinEntry::PreReplicatedRemove(const FReplicatedSkinList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->HandleReplicatedEntryRemoved(SkinDefinition);
	}
}

void FReplicatedSkinList::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (Owner && Owner->bReplicatedInventoryChanged)
	{
		Owner->bReplicatedInventoryChanged = false;
		Owner->NotifySkinsChanged();
	}
}

USkinComponent::USkinComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	ReplicatedEntries.Owner = this;
}

void USkinComponent::BeginPlay()
{
	Super::BeginPlay();

	ReplicatedEntries.Owner = this;

	UProjectTagConfig::Get(this)->GetSkinFilterTypeTags(FilterTypeTags);
	if (HasSkinAuthority())
	{
		RebuildFilteredSkinMap();
	}
	else
	{
		RebuildRuntimeSkinsFromReplicatedEntries();
	}
}

void USkinComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	++SkinLoadGeneration;
	for (const TSharedPtr<FStreamableHandle>& Handle : PendingSkinLoadHandles)
	{
		if (Handle.IsValid() && !Handle->HasLoadCompleted())
		{
			Handle->CancelHandle();
		}
	}
	PendingSkinLoadHandles.Reset();
	ReplicatedEntries.Owner = nullptr;

	Super::EndPlay(EndPlayReason);
}

void USkinComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(USkinComponent, ReplicatedEntries, Params);
}

bool USkinComponent::HasSkinAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

// 보상으로 받은 스킨을 비동기로 읽고, 한 번의 지급이 끝난 뒤 목록 변경을 알린다.
void USkinComponent::AddSkinsByPrimaryAssetIds(const TArray<FPrimaryAssetId>& SkinDefinitions)
{
	if (!HasSkinAuthority() || SkinDefinitions.IsEmpty())
	{
		return;
	}
	PendingSkinLoadHandles.RemoveAll([](const TSharedPtr<FStreamableHandle>& Handle)
	{
		return !Handle.IsValid() || Handle->HasLoadCompleted();
	});
	const uint64 RequestGeneration = SkinLoadGeneration;
	TSharedPtr<FStreamableHandle> LoadHandle = UAssetManager::Get().LoadPrimaryAssets(
		SkinDefinitions, {}, FStreamableDelegate::CreateWeakLambda(this, [this, SkinDefinitions, RequestGeneration]()
		{
			if (RequestGeneration != SkinLoadGeneration || !HasSkinAuthority())
			{
				return;
			}
			bool bChanged = false;
			for (const FPrimaryAssetId& DefinitionId : SkinDefinitions)
			{
				const USkinDefinition* Definition = Cast<USkinDefinition>(UAssetManager::Get().GetPrimaryAssetObject(DefinitionId));
				if (!IsValid(Definition))
				{
					UE_LOG(SkinComponentLog, Error, TEXT("Failed to load skin definition '%s'."), *DefinitionId.ToString());
					continue;
				}
				bChanged |= AddSkinDefinition(Definition);
			}
			if (bChanged)
			{
				NotifySkinsChanged();
			}
		}));
	if (LoadHandle.IsValid())
	{
		PendingSkinLoadHandles.Add(LoadHandle);
	}
}

void USkinComponent::AddSkinDefinitions(const TArray<USkinDefinition*>& SkinDefinitions)
{
	if (!HasSkinAuthority())
	{
		return;
	}
	bool bChanged = false;
	for (const USkinDefinition* Definition : SkinDefinitions)
	{
		if (IsValid(Definition))
		{
			bChanged |= AddSkinDefinition(Definition);
		}
	}
	if (bChanged)
	{
		NotifySkinsChanged();
	}
}

void USkinComponent::FilterSkin(USkinInstance* SkinInstance)
{
	const USkinDefinition* SkinDefinition = IsValid(SkinInstance) ? SkinInstance->SkinDefinition.Get() : nullptr;
	if (!IsValid(SkinDefinition))
	{
		return;
	}

	if (!SkinDefinition->IdTag.IsValid())
	{

		return;
	}

	for (const FGameplayTag& TypeTag : FilterTypeTags)
	{
		if (SkinDefinition->IdTag.MatchesTag(TypeTag))
		{
			Map_Type_SkinList.FindOrAdd(TypeTag).Skins.AddUnique(SkinInstance);
		}
	}
}

void USkinComponent::ApplyProjectTagConfig(const UProjectTagConfig* ProjectTagConfig)
{
	const UProjectTagConfig* EffectiveConfig = ProjectTagConfig ? ProjectTagConfig : UProjectTagConfig::GetDefaultConfig();
	EffectiveConfig->GetSkinFilterTypeTags(FilterTypeTags);

	RebuildFilteredSkinMap();
	NotifySkinsChanged();
}

bool USkinComponent::HasSkinDefinition(const USkinDefinition* SkinDefinition) const
{
	return IsValid(SkinDefinition) && FindReplicatedEntryByDefinition(SkinDefinition) != nullptr;
}

void USkinComponent::RebuildRuntimeSkinsFromReplicatedEntries()
{
	AllSkinList.Skins.Reset();

	for (const FReplicatedSkinEntry& Entry : ReplicatedEntries.Entries)
	{
		if (!IsValid(Entry.SkinDefinition))
		{
			continue;
		}

		USkinInstance* NewSkinInstance = NewObject<USkinInstance>(this);
		NewSkinInstance->SkinDefinition = Entry.SkinDefinition;
		AllSkinList.Skins.Add(NewSkinInstance);
	}

	RebuildFilteredSkinMap();
	NotifySkinsChanged();
}

void USkinComponent::RebuildFilteredSkinMap()
{
	Map_Type_SkinList.Reset();

	for (USkinInstance* SkinInstance : AllSkinList.Skins)
	{
		FilterSkin(SkinInstance);
	}
}

void USkinComponent::HandleReplicatedEntryAddedOrChanged(const FReplicatedSkinEntry& Entry)
{
	if (!IsValid(Entry.SkinDefinition.Get()))
	{
		return;
	}

	USkinInstance* SkinInstance = FindSkinInstanceByDefinition(Entry.SkinDefinition.Get());
	if (!IsValid(SkinInstance))
	{
		SkinInstance = NewObject<USkinInstance>(this);
		SkinInstance->SkinDefinition = Entry.SkinDefinition;
		AllSkinList.Skins.Add(SkinInstance);
		FilterSkin(SkinInstance);
		bReplicatedInventoryChanged = true;
	}
}

void USkinComponent::HandleReplicatedEntryRemoved(const USkinDefinition* SkinDefinition)
{
	if (!IsValid(SkinDefinition))
	{
		return;
	}

	for (int32 Index = AllSkinList.Skins.Num() - 1; Index >= 0; --Index)
	{
		USkinInstance* SkinInstance = AllSkinList.Skins[Index];
		if (IsValid(SkinInstance) && SkinInstance->SkinDefinition == SkinDefinition)
		{
			AllSkinList.Skins.RemoveAt(Index);
		}
	}

	RebuildFilteredSkinMap();
	bReplicatedInventoryChanged = true;
}

bool USkinComponent::AddSkinDefinition(const USkinDefinition* SkinDefinition)
{
	if (FindReplicatedEntryByDefinition(SkinDefinition))
	{
		return false;
	}
	USkinInstance* SkinInstance = NewObject<USkinInstance>(this);
	SkinInstance->SkinDefinition = SkinDefinition;
	AllSkinList.Skins.Add(SkinInstance);
	FilterSkin(SkinInstance);

	FReplicatedSkinEntry& NewEntry = ReplicatedEntries.Entries.AddDefaulted_GetRef();
	NewEntry.SkinDefinition = SkinDefinition;
	ReplicatedEntries.MarkEntryDirty(NewEntry);
	MARK_PROPERTY_DIRTY_FROM_NAME(USkinComponent, ReplicatedEntries, this);
	return true;
}

void USkinComponent::NotifySkinsChanged()
{
	OnSkinsChanged.Broadcast();
}

USkinInstance* USkinComponent::FindSkinInstanceByDefinition(const USkinDefinition* SkinDefinition) const
{
	if (!IsValid(SkinDefinition))
	{
		return nullptr;
	}

	for (USkinInstance* SkinInstance : AllSkinList.Skins)
	{
		if (IsValid(SkinInstance) && SkinInstance->SkinDefinition == SkinDefinition)
		{
			return SkinInstance;
		}
	}

	return nullptr;
}

int32 USkinComponent::FindReplicatedEntryIndexByDefinition(const USkinDefinition* SkinDefinition) const
{
	if (!IsValid(SkinDefinition))
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < ReplicatedEntries.Entries.Num(); ++Index)
	{
		if (ReplicatedEntries.Entries[Index].SkinDefinition == SkinDefinition)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

FReplicatedSkinEntry* USkinComponent::FindReplicatedEntryByDefinition(const USkinDefinition* SkinDefinition)
{
	const int32 EntryIndex = FindReplicatedEntryIndexByDefinition(SkinDefinition);
	return EntryIndex != INDEX_NONE ? &ReplicatedEntries.Entries[EntryIndex] : nullptr;
}

const FReplicatedSkinEntry* USkinComponent::FindReplicatedEntryByDefinition(const USkinDefinition* SkinDefinition) const
{
	const int32 EntryIndex = FindReplicatedEntryIndexByDefinition(SkinDefinition);
	return EntryIndex != INDEX_NONE ? &ReplicatedEntries.Entries[EntryIndex] : nullptr;
}
