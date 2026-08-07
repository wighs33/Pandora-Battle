#include "Skin/SkinDefaultUnlockPolicy.h"

#include "Definition/Skin/SkinDefinition.h"
#include "Engine/AssetManager.h"

namespace
{
	struct FDefaultSkinGrantCatalogEntry
	{
		FName SkinName = NAME_None;
	};

	const TArray<FDefaultSkinGrantCatalogEntry>&
	GetDefaultSkinGrantCatalog()
	{
		static TArray<FDefaultSkinGrantCatalogEntry> Catalog;
		static bool bCatalogInitialized = false;
		if (bCatalogInitialized)
		{
			return Catalog;
		}

		UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
		if (!AssetManager)
		{
			return Catalog;
		}
		TArray<FPrimaryAssetId> SkinDefinitionIds;
		AssetManager->GetPrimaryAssetIdList(
			FPrimaryAssetType(TEXT("SkinDefinition")),
			SkinDefinitionIds);
		if (SkinDefinitionIds.IsEmpty())
		{
			return Catalog;
		}
		SkinDefinitionIds.Sort(
			[](const FPrimaryAssetId& Left, const FPrimaryAssetId& Right)
			{
				return Left.ToString() < Right.ToString();
			});

		for (const FPrimaryAssetId& SkinDefinitionId : SkinDefinitionIds)
		{
			const USkinDefinition* SkinDefinition =
				Cast<USkinDefinition>(
					AssetManager->GetPrimaryAssetObject(SkinDefinitionId));
			if (!SkinDefinition)
			{
				SkinDefinition = Cast<USkinDefinition>(
					AssetManager->GetPrimaryAssetPath(
						SkinDefinitionId).TryLoad());
			}

			if (IsValid(SkinDefinition)
				&& SkinDefinition->IsGrantedByDefault()
				&& !SkinDefinition->GestureMontage)
			{
				FDefaultSkinGrantCatalogEntry& Entry =
					Catalog.AddDefaulted_GetRef();
				Entry.SkinName = SkinDefinition->GetFName();
			}
		}
		bCatalogInitialized = true;

		return Catalog;
	}
}

const TArray<FName>& SkinDefaultUnlockPolicy::GetDefaultUnlockedSkinNames()
{
	static TArray<FName> DefaultSkinNames;
	DefaultSkinNames.Reset();

	const TArray<FDefaultSkinGrantCatalogEntry>& Catalog =
		GetDefaultSkinGrantCatalog();
	DefaultSkinNames.Reserve(Catalog.Num());
	for (const FDefaultSkinGrantCatalogEntry& Entry : Catalog)
	{
		DefaultSkinNames.AddUnique(Entry.SkinName);
	}
	return DefaultSkinNames;
}

bool SkinDefaultUnlockPolicy::IsDefaultUnlockedSkinName(const FName SkinName)
{
	if (SkinName.IsNone())
	{
		return false;
	}

	for (const FName DefaultSkinName : GetDefaultUnlockedSkinNames())
	{
		if (SkinName.IsEqual(DefaultSkinName, ENameCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

bool SkinDefaultUnlockPolicy::IsDefaultUnlockedSkinDefinition(const USkinDefinition* SkinDefinition)
{
	return IsValid(SkinDefinition)
		&& SkinDefinition->IsGrantedByDefault()
		&& !SkinDefinition->GestureMontage;
}
