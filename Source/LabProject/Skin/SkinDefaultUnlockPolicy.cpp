#include "Skin/SkinDefaultUnlockPolicy.h"

#include "Definition/Skin/SkinDefinition.h"
#include "Engine/AssetManager.h"

namespace
{
	const TArray<FName>& GetDefaultSkinGrantCatalog()
	{
		static TArray<FName> Catalog;
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

			if (SkinDefaultUnlockPolicy::IsDefaultUnlockedSkinDefinition(SkinDefinition))
			{
				Catalog.AddUnique(SkinDefinition->GetFName());
			}
		}
		bCatalogInitialized = true;

		return Catalog;
	}
}

const TArray<FName>& SkinDefaultUnlockPolicy::GetDefaultUnlockedSkinNames()
{
	return GetDefaultSkinGrantCatalog();
}

bool SkinDefaultUnlockPolicy::IsDefaultUnlockedSkinDefinition(const USkinDefinition* SkinDefinition)
{
	return IsValid(SkinDefinition)
		&& SkinDefinition->IsGrantedByDefault()
		&& !SkinDefinition->GestureMontage;
}
