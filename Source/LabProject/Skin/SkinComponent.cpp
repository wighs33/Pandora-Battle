#include "SkinComponent.h"

#include "Common/ProjectTagConfig.h"
#include "Engine/AssetManager.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Skin/SkinDefinition.h"
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
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		InitializeReplicatedEntriesFromRuntimeSkins();
		RebuildFilteredSkinMap();
	}
	else
	{
		RebuildRuntimeSkinsFromReplicatedEntries();
	}
}

void USkinComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(USkinComponent, ReplicatedEntries, Params);
}

void USkinComponent::MakeAndAddSkins(const TArray<FPrimaryAssetId>& SkinDefinitions)
{
	AddSkinsByPrimaryAssetIds(SkinDefinitions);
}

void USkinComponent::AddSkinsByPrimaryAssetIds(const TArray<FPrimaryAssetId>& SkinDefinitions)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(SkinComponentLog, Warning, TEXT("AddSkinsByPrimaryAssetIds failed: skins can only be modified on the authority."));
		return;
	}

	if (SkinDefinitions.IsEmpty())
	{
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	AssetManager.LoadPrimaryAssets(
		SkinDefinitions,
		{},
		FStreamableDelegate::CreateWeakLambda(this, [this, SkinDefinitions]()
		{
			UAssetManager& LoadedAssetManager = UAssetManager::Get();

			for (const FPrimaryAssetId& SkinDefinitionId : SkinDefinitions)
			{
				const USkinDefinition* SkinDefinition = Cast<USkinDefinition>(LoadedAssetManager.GetPrimaryAssetObject(SkinDefinitionId));
				if (!IsValid(SkinDefinition))
				{
					UE_LOG(SkinComponentLog, Warning, TEXT("AddSkinsByPrimaryAssetIds failed: could not resolve skin definition '%s'."), *SkinDefinitionId.ToString());
					continue;
				}

				if (FindReplicatedEntryByDefinition(SkinDefinition))
				{
					continue;
				}

				USkinInstance* NewSkinInstance = NewObject<USkinInstance>(this);
				NewSkinInstance->SkinDefinition = SkinDefinition;
				AddReplicatedSkin(NewSkinInstance);
			}
		}));
}

void USkinComponent::FilterSkin(USkinInstance* SkinInstance)
{
	const USkinDefinition* SkinDefinition = IsValid(SkinInstance) ? SkinInstance->SkinDefinition.Get() : nullptr;
	if (!SkinDefinition)
	{
		return;
	}

	if (!SkinDefinition->IdTag.IsValid())
	{
		UE_LOG(SkinComponentLog, Warning, TEXT("FilterSkin skipped skin '%s': invalid IdTag."), *GetNameSafe(SkinDefinition));
		return;
	}

	for (const FGameplayTag& TypeTag : FilterTypeTags)
	{
		if (SkinDefinition->IdTag.MatchesTag(TypeTag))
		{
			AddValueToMap(TypeTag, SkinInstance);
		}
	}
}

void USkinComponent::AddValueToMap(FGameplayTag TypeTag, USkinInstance* SkinInstance)
{
	if (!IsValid(SkinInstance))
	{
		return;
	}

	Map_Type_SkinList.FindOrAdd(TypeTag).Skins.AddUnique(SkinInstance);
}

void USkinComponent::ApplyProjectTagConfig(const UProjectTagConfig* ProjectTagConfig)
{
	const UProjectTagConfig* EffectiveConfig = ProjectTagConfig ? ProjectTagConfig : UProjectTagConfig::GetDefaultConfig();
	EffectiveConfig->GetSkinFilterTypeTags(FilterTypeTags);

	RebuildFilteredSkinMap();
}

bool USkinComponent::HasSkinDefinition(const USkinDefinition* SkinDefinition) const
{
	return IsValid(SkinDefinition)
		&& (FindSkinInstanceByDefinition(SkinDefinition) || FindReplicatedEntryByDefinition(SkinDefinition));
}

void USkinComponent::InitializeReplicatedEntriesFromRuntimeSkins()
{
	for (USkinInstance* SkinInstance : AllSkinList.Skins)
	{
		AddReplicatedSkin(SkinInstance);
	}
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
	if (!IsValid(Entry.SkinDefinition))
	{
		return;
	}

	USkinInstance* SkinInstance = FindSkinInstanceByDefinition(Entry.SkinDefinition);
	if (!IsValid(SkinInstance))
	{
		SkinInstance = NewObject<USkinInstance>(this);
		SkinInstance->SkinDefinition = Entry.SkinDefinition;
		AllSkinList.Skins.Add(SkinInstance);
		FilterSkin(SkinInstance);
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
}

void USkinComponent::AddReplicatedSkin(USkinInstance* SkinInstance)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(SkinInstance))
	{
		return;
	}

	const USkinDefinition* SkinDefinition = SkinInstance->SkinDefinition.Get();
	if (!IsValid(SkinDefinition))
	{
		return;
	}

	if (FindReplicatedEntryByDefinition(SkinDefinition))
	{
		return;
	}

	AllSkinList.Skins.AddUnique(SkinInstance);
	FilterSkin(SkinInstance);

	FReplicatedSkinEntry& NewEntry = ReplicatedEntries.Entries.AddDefaulted_GetRef();
	NewEntry.SkinDefinition = SkinDefinition;
	ReplicatedEntries.MarkEntryDirty(NewEntry);
	MARK_PROPERTY_DIRTY_FROM_NAME(USkinComponent, ReplicatedEntries, this);
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
