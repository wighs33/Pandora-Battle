#include "SavedGameData/PlayerProfilePolicy.h"

#include "Engine/AssetManager.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Skin/SkinDefaultUnlockPolicy.h"

namespace PlayerProfilePolicy
{
	const FString& GetLocalProfileSaveSlotName()
	{
		static const FString LocalProfileSaveSlotName(TEXT("LocalProfile"));
		return LocalProfileSaveSlotName;
	}

	const FPrimaryAssetType& GetSkinDefinitionAssetType()
	{
		static const FPrimaryAssetType AssetType(TEXT("SkinDefinition"));
		return AssetType;
	}

	FPrimaryAssetId ResolveRedirectedAssetId(const FPrimaryAssetId& AssetId)
	{
		if (!AssetId.IsValid())
		{
			return FPrimaryAssetId();
		}

		if (const UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
		{
			const FPrimaryAssetId RedirectedId = AssetManager->GetRedirectedPrimaryAssetId(AssetId);
			if (RedirectedId.IsValid())
			{
				return RedirectedId;
			}
		}

		return AssetId;
	}

	FPrimaryAssetId MakeDefinitionAssetId(
		const FPrimaryAssetType& AssetType,
		const FName AssetName)
	{
		return AssetName.IsNone()
			? FPrimaryAssetId()
			: ResolveRedirectedAssetId(FPrimaryAssetId(AssetType, AssetName));
	}

	FPrimaryAssetId ResolvePandoraSaveId(const UPandoraDefinition* PandoraDefinition)
	{
		return PandoraDefinition
			? ResolveRedirectedAssetId(PandoraDefinition->GetPrimaryAssetId())
			: FPrimaryAssetId();
	}

	FPrimaryAssetId ResolveSkinSaveId(const USkinDefinition* SkinDefinition)
	{
		return SkinDefinition
			? ResolveRedirectedAssetId(SkinDefinition->GetPrimaryAssetId())
			: FPrimaryAssetId();
	}

	bool EnsureDefaultUnlockedSkins(UPdSaveGame& SaveGame)
	{
		bool bChanged = false;
		for (const FName DefaultSkinName : SkinDefaultUnlockPolicy::GetDefaultUnlockedSkinNames())
		{
			if (DefaultSkinName.IsNone())
			{
				continue;
			}

			const FPrimaryAssetId DefaultSkinId =
				MakeDefinitionAssetId(GetSkinDefinitionAssetType(), DefaultSkinName);
			if (!DefaultSkinId.IsValid())
			{
				continue;
			}

			int32& GrantedValue = SaveGame.PlayerSkinData.GrantedSkinsById.FindOrAdd(DefaultSkinId);
			if (GrantedValue < 1)
			{
				GrantedValue = 1;
				bChanged = true;
			}
		}
		return bChanged;
	}
}
