#include "PandoraComponent.h"

#include "Common/ProjectTagConfig.h"
#include "Engine/AssetManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Pandora/PandoraDefinition.h"
#include "Pandora/PandoraInstance.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraComponent)

DEFINE_LOG_CATEGORY(PandoraComponentLog)

void FReplicatedPandoraEntry::PostReplicatedAdd(const FReplicatedPandoraList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->HandleReplicatedEntryAddedOrChanged(*this);
	}
}

void FReplicatedPandoraEntry::PostReplicatedChange(const FReplicatedPandoraList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->HandleReplicatedEntryAddedOrChanged(*this);
	}
}

void FReplicatedPandoraEntry::PreReplicatedRemove(const FReplicatedPandoraList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->HandleReplicatedEntryRemoved(PandoraDefinition);
	}
}

UPandoraComponent::UPandoraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	ReplicatedEntries.Owner = this;
}

void UPandoraComponent::BeginPlay()
{
	Super::BeginPlay();

	ReplicatedEntries.Owner = this;

	UProjectTagConfig::Get(this)->GetPandoraFilterTypeTags(FilterTypeTags);
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		InitializeReplicatedEntriesFromRuntimePandoras();
		AddPandorasByPrimaryAssetIds(AllPandroaDefinition);
	}
	else
	{
		RebuildRuntimePandorasFromReplicatedEntries();
	}
}

void UPandoraComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UPandoraComponent, ReplicatedEntries, Params);
}

void UPandoraComponent::AddPandorasByPrimaryAssetIds(const TArray<FPrimaryAssetId>& PandoraDefinitions)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(PandoraComponentLog, Warning, TEXT("AddPandorasByPrimaryAssetIds failed: pandoras can only be modified on the authority."));
		return;
	}

	if (PandoraDefinitions.IsEmpty())
	{
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	AssetManager.LoadPrimaryAssets(
		PandoraDefinitions,
		{},
		FStreamableDelegate::CreateWeakLambda(this, [this, PandoraDefinitions]()
		{
			UAssetManager& LoadedAssetManager = UAssetManager::Get();

			for (const FPrimaryAssetId& PandoraDefinitionId : PandoraDefinitions)
			{
				const UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(LoadedAssetManager.GetPrimaryAssetObject(PandoraDefinitionId));
				if (!IsValid(PandoraDefinition))
				{
					UE_LOG(PandoraComponentLog, Warning, TEXT("AddPandorasByPrimaryAssetIds failed: could not resolve pandora definition '%s'."), *PandoraDefinitionId.ToString());
					continue;
				}

				if (FindReplicatedEntryByDefinition(PandoraDefinition))
				{
					continue;
				}

				UPandoraInstance* NewPandoraInstance = NewObject<UPandoraInstance>(this);
				NewPandoraInstance->PandoraDefinition = PandoraDefinition;
				NewPandoraInstance->IsOwned = false;
				AddReplicatedPandora(NewPandoraInstance);
			}
		}));
}

void UPandoraComponent::ActivatePandoras(const TArray<FPrimaryAssetId>& PandoraDefinitions)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(PandoraComponentLog, Warning, TEXT("ActivatePandoras failed: pandoras can only be modified on the authority."));
		return;
	}

	if (PandoraDefinitions.IsEmpty())
	{
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	AssetManager.LoadPrimaryAssets(
		PandoraDefinitions,
		{},
		FStreamableDelegate::CreateWeakLambda(this, [this, PandoraDefinitions]()
		{
			UAssetManager& LoadedAssetManager = UAssetManager::Get();

			for (const FPrimaryAssetId& PandoraDefinitionId : PandoraDefinitions)
			{
				const UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(LoadedAssetManager.GetPrimaryAssetObject(PandoraDefinitionId));
				if (!IsValid(PandoraDefinition))
				{
					UE_LOG(PandoraComponentLog, Warning, TEXT("ActivatePandoras failed: could not resolve pandora definition '%s'."), *PandoraDefinitionId.ToString());
					continue;
				}

				if (FReplicatedPandoraEntry* ExistingEntry = FindReplicatedEntryByDefinition(PandoraDefinition))
				{
					if (!ExistingEntry->IsOwned)
					{
						ExistingEntry->IsOwned = true;
						ReplicatedEntries.MarkEntryDirty(*ExistingEntry);
						MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, ReplicatedEntries, this);
					}

					if (UPandoraInstance* PandoraInstance = FindPandoraInstanceByDefinition(PandoraDefinition))
					{
						PandoraInstance->IsOwned = true;
					}
					else
					{
						UPandoraInstance* NewPandoraInstance = NewObject<UPandoraInstance>(this);
						NewPandoraInstance->PandoraDefinition = PandoraDefinition;
						NewPandoraInstance->IsOwned = true;
						AllPandoraList.Pandoras.Add(NewPandoraInstance);
						FilterPandoras(NewPandoraInstance);
					}

					continue;
				}

				UPandoraInstance* NewPandoraInstance = NewObject<UPandoraInstance>(this);
				NewPandoraInstance->PandoraDefinition = PandoraDefinition;
				NewPandoraInstance->IsOwned = true;
				AddReplicatedPandora(NewPandoraInstance);
			}
		}));
}

void UPandoraComponent::FilterPandoras(UPandoraInstance* PandoraInstance)
{
	const UPandoraDefinition* PandoraDefinition = IsValid(PandoraInstance) ? PandoraInstance->PandoraDefinition.Get() : nullptr;
	if (!PandoraDefinition)
	{
		return;
	}

	if (!PandoraDefinition->IdTag.IsValid())
	{
		UE_LOG(PandoraComponentLog, Warning, TEXT("FilterPandoras skipped pandora '%s': invalid IdTag."), *GetNameSafe(PandoraDefinition));
		return;
	}

	for (const FGameplayTag& TypeTag : FilterTypeTags)
	{
		if (PandoraDefinition->IdTag.MatchesTag(TypeTag))
		{
			AddValueToMap(TypeTag, PandoraInstance);
		}
	}
}

void UPandoraComponent::AddValueToMap(FGameplayTag TypeTag, UPandoraInstance* PandoraInstance)
{
	if (!IsValid(PandoraInstance))
	{
		return;
	}

	Map_Type_PandoraList.FindOrAdd(TypeTag).Pandoras.AddUnique(PandoraInstance);
}

void UPandoraComponent::ApplyProjectTagConfig(const UProjectTagConfig* ProjectTagConfig)
{
	const UProjectTagConfig* EffectiveConfig = ProjectTagConfig ? ProjectTagConfig : UProjectTagConfig::GetDefaultConfig();
	EffectiveConfig->GetPandoraFilterTypeTags(FilterTypeTags);

	RebuildFilteredPandoraMap();
}

void UPandoraComponent::InitializeReplicatedEntriesFromRuntimePandoras()
{
	for (UPandoraInstance* PandoraInstance : AllPandoraList.Pandoras)
	{
		AddReplicatedPandora(PandoraInstance);
	}
}

void UPandoraComponent::RebuildRuntimePandorasFromReplicatedEntries()
{
	AllPandoraList.Pandoras.Reset();

	for (const FReplicatedPandoraEntry& Entry : ReplicatedEntries.Entries)
	{
		if (!IsValid(Entry.PandoraDefinition))
		{
			continue;
		}

		UPandoraInstance* NewPandoraInstance = NewObject<UPandoraInstance>(this);
		NewPandoraInstance->PandoraDefinition = Entry.PandoraDefinition;
		NewPandoraInstance->IsOwned = Entry.IsOwned;
		AllPandoraList.Pandoras.Add(NewPandoraInstance);
	}

	RebuildFilteredPandoraMap();
}

void UPandoraComponent::RebuildFilteredPandoraMap()
{
	Map_Type_PandoraList.Reset();

	for (UPandoraInstance* PandoraInstance : AllPandoraList.Pandoras)
	{
		FilterPandoras(PandoraInstance);
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UKismetSystemLibrary::PrintString(this, TEXT("Filtering Pandoras"), true, true, FLinearColor(0.0f, 0.66f, 1.0f, 1.0f), 2.0f);
#endif
}

void UPandoraComponent::HandleReplicatedEntryAddedOrChanged(const FReplicatedPandoraEntry& Entry)
{
	if (!IsValid(Entry.PandoraDefinition))
	{
		return;
	}

	UPandoraInstance* PandoraInstance = FindPandoraInstanceByDefinition(Entry.PandoraDefinition);
	if (!IsValid(PandoraInstance))
	{
		PandoraInstance = NewObject<UPandoraInstance>(this);
		PandoraInstance->PandoraDefinition = Entry.PandoraDefinition;
		AllPandoraList.Pandoras.Add(PandoraInstance);
		FilterPandoras(PandoraInstance);
	}

	PandoraInstance->IsOwned = Entry.IsOwned;
}

void UPandoraComponent::HandleReplicatedEntryRemoved(const UPandoraDefinition* PandoraDefinition)
{
	if (!IsValid(PandoraDefinition))
	{
		return;
	}

	for (int32 Index = AllPandoraList.Pandoras.Num() - 1; Index >= 0; --Index)
	{
		UPandoraInstance* PandoraInstance = AllPandoraList.Pandoras[Index];
		if (IsValid(PandoraInstance) && PandoraInstance->PandoraDefinition == PandoraDefinition)
		{
			AllPandoraList.Pandoras.RemoveAt(Index);
		}
	}

	RebuildFilteredPandoraMap();
}

void UPandoraComponent::AddReplicatedPandora(UPandoraInstance* PandoraInstance)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(PandoraInstance))
	{
		return;
	}

	const UPandoraDefinition* PandoraDefinition = PandoraInstance->PandoraDefinition.Get();
	if (!IsValid(PandoraDefinition))
	{
		return;
	}

	if (FReplicatedPandoraEntry* ExistingEntry = FindReplicatedEntryByDefinition(PandoraDefinition))
	{
		if (ExistingEntry->IsOwned != PandoraInstance->IsOwned)
		{
			ExistingEntry->IsOwned = PandoraInstance->IsOwned;
			ReplicatedEntries.MarkEntryDirty(*ExistingEntry);
			MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, ReplicatedEntries, this);
		}

		if (UPandoraInstance* ExistingInstance = FindPandoraInstanceByDefinition(PandoraDefinition))
		{
			ExistingInstance->IsOwned = PandoraInstance->IsOwned;
		}

		return;
	}

	AllPandoraList.Pandoras.AddUnique(PandoraInstance);
	FilterPandoras(PandoraInstance);

	FReplicatedPandoraEntry& NewEntry = ReplicatedEntries.Entries.AddDefaulted_GetRef();
	NewEntry.PandoraDefinition = PandoraDefinition;
	NewEntry.IsOwned = PandoraInstance->IsOwned;
	ReplicatedEntries.MarkEntryDirty(NewEntry);
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, ReplicatedEntries, this);
}

UPandoraInstance* UPandoraComponent::FindPandoraInstanceByDefinition(const UPandoraDefinition* PandoraDefinition) const
{
	if (!IsValid(PandoraDefinition))
	{
		return nullptr;
	}

	for (UPandoraInstance* PandoraInstance : AllPandoraList.Pandoras)
	{
		if (IsValid(PandoraInstance) && PandoraInstance->PandoraDefinition == PandoraDefinition)
		{
			return PandoraInstance;
		}
	}

	return nullptr;
}

int32 UPandoraComponent::FindReplicatedEntryIndexByDefinition(const UPandoraDefinition* PandoraDefinition) const
{
	if (!IsValid(PandoraDefinition))
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < ReplicatedEntries.Entries.Num(); ++Index)
	{
		if (ReplicatedEntries.Entries[Index].PandoraDefinition == PandoraDefinition)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

FReplicatedPandoraEntry* UPandoraComponent::FindReplicatedEntryByDefinition(const UPandoraDefinition* PandoraDefinition)
{
	const int32 EntryIndex = FindReplicatedEntryIndexByDefinition(PandoraDefinition);
	return EntryIndex != INDEX_NONE ? &ReplicatedEntries.Entries[EntryIndex] : nullptr;
}

const FReplicatedPandoraEntry* UPandoraComponent::FindReplicatedEntryByDefinition(const UPandoraDefinition* PandoraDefinition) const
{
	const int32 EntryIndex = FindReplicatedEntryIndexByDefinition(PandoraDefinition);
	return EntryIndex != INDEX_NONE ? &ReplicatedEntries.Entries[EntryIndex] : nullptr;
}
