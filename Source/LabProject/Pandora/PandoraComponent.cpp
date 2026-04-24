// Fill out your copyright notice in the Description page of Project Settings.


#include "PandoraComponent.h"

#include "Engine/AssetManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Pandora/PandoraDefinition.h"
#include "Pandora/PandoraInstance.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraComponent)

DEFINE_LOG_CATEGORY(PandoraComponentLog)

void UPandoraComponent::BeginPlay()
{
	Super::BeginPlay();

	AddPandorasByPrimaryAssetIds(AllPandroaDefinition);
}

void UPandoraComponent::AddPandorasByPrimaryAssetIds(const TArray<FPrimaryAssetId>& PandoraDefinitions)
{
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

				UPandoraInstance* NewPandoraInstance = NewObject<UPandoraInstance>(this);
				NewPandoraInstance->PandoraDefinition = PandoraDefinition;

				AllPandoraList.Pandoras.AddUnique(NewPandoraInstance);
			}

			RebuildFilteredPandoraMap();
		}));
}

void UPandoraComponent::ActivatePandoras(const TArray<FPrimaryAssetId>& PandoraDefinitions)
{
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

				for (UPandoraInstance* PandoraInstance : AllPandoraList.Pandoras)
				{
					if (IsValid(PandoraInstance) && PandoraInstance->PandoraDefinition == PandoraDefinition)
					{
						PandoraInstance->IsOwned = true;
					}
				}
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

	for (const FGameplayTag& TypeTag : GetFilterTypeTags())
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

	Map_Type_PandoraList.FindOrAdd(TypeTag).Pandoras.Add(PandoraInstance);
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

const TArray<FGameplayTag>& UPandoraComponent::GetFilterTypeTags()
{
	static const TArray<FGameplayTag> TypeTags =
	{
		FGameplayTag::RequestGameplayTag(TEXT("Pandora.Offensive")),
		FGameplayTag::RequestGameplayTag(TEXT("Pandora.Defensive")),
		FGameplayTag::RequestGameplayTag(TEXT("Pandora.Support")),
		FGameplayTag::RequestGameplayTag(TEXT("Pandora.Special"))
	};

	return TypeTags;
}
