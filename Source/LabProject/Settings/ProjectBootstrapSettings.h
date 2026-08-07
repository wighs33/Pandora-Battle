#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/PrimaryAssetId.h"
#include "ProjectBootstrapSettings.generated.h"

class UPdGameInstanceDefinition;

/**
 * Selects the project bootstrap asset and owns the game-entry preload manifest
 * without embedding content names in native code.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Project Bootstrap"))
class LABPROJECT_API UProjectBootstrapSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	const TSoftObjectPtr<UPdGameInstanceDefinition>& GetBootstrapDefinition() const
	{
		return BootstrapDefinition;
	}
	const TArray<FPrimaryAssetId>& GetGameEntryRequiredPrimaryAssets() const
	{
		return GameEntryRequiredPrimaryAssets;
	}
	const TArray<FPrimaryAssetType>& GetGameEntryRequiredPrimaryAssetTypes() const
	{
		return GameEntryRequiredPrimaryAssetTypes;
	}

private:
	UPROPERTY(Config, EditAnywhere, Category = "Content",
		meta = (AllowedTypes = "GameInstanceDefinition",
			DisplayName = "Game Instance Bootstrap Definition"))
	TSoftObjectPtr<UPdGameInstanceDefinition> BootstrapDefinition;

	/** Exact primary assets that must be loaded before entering gameplay. */
	UPROPERTY(Config, EditAnywhere, Category = "Content|Game Entry",
		meta = (TitleProperty = "PrimaryAssetName"))
	TArray<FPrimaryAssetId> GameEntryRequiredPrimaryAssets;

	/** Every registered asset of these primary asset types is loaded before gameplay. */
	UPROPERTY(Config, EditAnywhere, Category = "Content|Game Entry")
	TArray<FPrimaryAssetType> GameEntryRequiredPrimaryAssetTypes;
};
