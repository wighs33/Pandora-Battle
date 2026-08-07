#pragma once

#include "CoreMinimal.h"
#include "SavedGameData/PdSaveGame.h"

class UPandoraDefinition;
class USkinDefinition;

/**
 * Stateless save-data policy shared by profile storage and progression paths.
 */
namespace PlayerProfilePolicy
{
	inline constexpr int32 MaxSavedMatchRecordCount =
		PdProfileSaveData::MaxMatchRecordCount;

	const FString& GetLocalProfileSaveSlotName();

	const FPrimaryAssetType& GetSkinDefinitionAssetType();
	FPrimaryAssetId ResolveRedirectedAssetId(const FPrimaryAssetId& AssetId);
	FPrimaryAssetId MakeDefinitionAssetId(
		const FPrimaryAssetType& AssetType,
		FName AssetName);
	FPrimaryAssetId ResolvePandoraSaveId(const UPandoraDefinition* PandoraDefinition);
	FPrimaryAssetId ResolveSkinSaveId(const USkinDefinition* SkinDefinition);

	bool EnsureDefaultUnlockedSkins(UPdSaveGame& SaveGame);
}
