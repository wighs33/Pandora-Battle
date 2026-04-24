// Fill out your copyright notice in the Description page of Project Settings.

#include "SkinComponent.h"

#include "Engine/AssetManager.h"
#include "Skin/SkinDefinition.h"
#include "Skin/SkinInstance.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinComponent)

DEFINE_LOG_CATEGORY(SkinComponentLog)

void USkinComponent::MakeAndAddSkins(const TArray<FPrimaryAssetId>& SkinDefinitions)
{
	AddSkinsByPrimaryAssetIds(SkinDefinitions);
}

void USkinComponent::AddSkinsByPrimaryAssetIds(const TArray<FPrimaryAssetId>& SkinDefinitions)
{
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

				USkinInstance* NewSkinInstance = NewObject<USkinInstance>(this);
				NewSkinInstance->SkinDefinition = SkinDefinition;

				AllSkinList.Skins.AddUnique(NewSkinInstance);
				FilterSkin(NewSkinInstance);
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

	const TArray<FGameplayTag>& TypeTags = GetFilterTypeTags();
	for (const FGameplayTag& TypeTag : TypeTags)
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

	Map_Type_SkinList.FindOrAdd(TypeTag).Skins.Add(SkinInstance);
}

const TArray<FGameplayTag>& USkinComponent::GetFilterTypeTags()
{
	static const TArray<FGameplayTag> TypeTags =
	{
		FGameplayTag::RequestGameplayTag(TEXT("Skin.Pandora")),
		FGameplayTag::RequestGameplayTag(TEXT("Skin.Cosmetics")),
		FGameplayTag::RequestGameplayTag(TEXT("Skin.Gesture")),
		FGameplayTag::RequestGameplayTag(TEXT("Skin.Riding")),
		FGameplayTag::RequestGameplayTag(TEXT("Skin.Cosmetics.Hat")),
		FGameplayTag::RequestGameplayTag(TEXT("Skin.Cosmetics.Top")),
		FGameplayTag::RequestGameplayTag(TEXT("Skin.Cosmetics.Bottom")),
		FGameplayTag::RequestGameplayTag(TEXT("Skin.Cosmetics.Shoes")),
		FGameplayTag::RequestGameplayTag(TEXT("Skin.Cosmetics.Hair")),
		FGameplayTag::RequestGameplayTag(TEXT("Skin.Cosmetics.Face")),
		FGameplayTag::RequestGameplayTag(TEXT("Skin.Cosmetics.Back")),
		FGameplayTag::RequestGameplayTag(TEXT("Skin.Cosmetics.Aura"))
	};

	return TypeTags;
}
