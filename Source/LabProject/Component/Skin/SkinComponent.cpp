#include "Component/Skin/SkinComponent.h"

#include "Definition/Common/ProjectTagConfig.h"
#include "Engine/AssetManager.h"
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
		InitializeReplicatedEntriesFromRuntimeSkins();
		RebuildFilteredSkinMap();
	}
	else
	{
		RebuildRuntimeSkinsFromReplicatedEntries();
	}
}

void USkinComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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

void USkinComponent::MakeAndAddSkins(const TArray<FPrimaryAssetId>& SkinDefinitions)
{
	AddSkinsByPrimaryAssetIds(SkinDefinitions);
}

void USkinComponent::AddSkinsByPrimaryAssetIds(const TArray<FPrimaryAssetId>& SkinDefinitions)
{
	if (!HasSkinAuthority())
	{

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
			if (!HasSkinAuthority())
			{
				return;
			}

			UAssetManager& LoadedAssetManager = UAssetManager::Get();

			for (const FPrimaryAssetId& SkinDefinitionId : SkinDefinitions)
			{
				const USkinDefinition* SkinDefinition = Cast<USkinDefinition>(LoadedAssetManager.GetPrimaryAssetObject(SkinDefinitionId));
				if (!CanReferenceSkinDefinition(SkinDefinition))
				{

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

void USkinComponent::AddSkinDefinitions(const TArray<USkinDefinition*>& SkinDefinitions)
{
	if (!HasSkinAuthority())
	{

		return;
	}

	for (USkinDefinition* SkinDefinition : SkinDefinitions)
	{
		if (!CanReferenceSkinDefinition(SkinDefinition) || FindReplicatedEntryByDefinition(SkinDefinition))
		{
			continue;
		}

		USkinInstance* NewSkinInstance = NewObject<USkinInstance>(this);
		NewSkinInstance->SkinDefinition = SkinDefinition;
		AddReplicatedSkin(NewSkinInstance);
	}
}

void USkinComponent::FilterSkin(USkinInstance* SkinInstance)
{
	const USkinDefinition* SkinDefinition = IsValid(SkinInstance) ? SkinInstance->SkinDefinition.Get() : nullptr;
	if (!CanReferenceSkinDefinition(SkinDefinition))
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
	return CanReferenceSkinDefinition(SkinDefinition)
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
	if (!CanReferenceSkinDefinition(Entry.SkinDefinition.Get()))
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
		NotifySkinsChanged();
	}
}

void USkinComponent::HandleReplicatedEntryRemoved(const USkinDefinition* SkinDefinition)
{
	if (!CanReferenceSkinDefinition(SkinDefinition))
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
	NotifySkinsChanged();
}

void USkinComponent::AddReplicatedSkin(USkinInstance* SkinInstance)
{
	if (!HasSkinAuthority() || !IsValid(SkinInstance))
	{
		return;
	}

	const USkinDefinition* SkinDefinition = SkinInstance->SkinDefinition.Get();
	if (!CanReferenceSkinDefinition(SkinDefinition))
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
	NotifySkinsChanged();
}

bool USkinComponent::CanReferenceSkinDefinition(const USkinDefinition* SkinDefinition) const
{
	if (!IsValid(SkinDefinition))
	{
		return false;
	}

	const FPrimaryAssetId PrimaryAssetId = SkinDefinition->GetPrimaryAssetId();
	return PrimaryAssetId.IsValid()
		&& PrimaryAssetId.PrimaryAssetType == FPrimaryAssetType(TEXT("SkinDefinition"));
}

void USkinComponent::NotifySkinsChanged()
{
	OnSkinsChanged.Broadcast();
}

USkinInstance* USkinComponent::FindSkinInstanceByDefinition(const USkinDefinition* SkinDefinition) const
{
	if (!CanReferenceSkinDefinition(SkinDefinition))
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
	if (!CanReferenceSkinDefinition(SkinDefinition))
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
