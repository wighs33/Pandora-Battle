#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "PdGameInstanceDefinition.generated.h"

class UPdSaveGame;

UENUM(BlueprintType)
enum class EPdStartupWindowMode : uint8
{
	Windowed,
	WindowedFullscreen,
	Fullscreen
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPdGameInstanceLifecycleSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Lifecycle|Window")
	bool bApplyWindowModeOnStart = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Lifecycle|Window")
	EPdStartupWindowMode StartupWindowMode = EPdStartupWindowMode::WindowedFullscreen;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Lifecycle|Window")
	bool bSaveAppliedWindowMode = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Lifecycle|Presentation")
	bool bDisableCollisionVisualizationOutsideEditor = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Lifecycle|Audio")
	bool bRestoreWorldBgmOnStart = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Lifecycle|Audio")
	bool bStopBgmOnShutdown = true;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPdPlayerProfilePersistenceSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Profile")
	TSubclassOf<UPdSaveGame> SaveGameClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Profile|Saving",
		meta = (ClampMin = "0.05", ForceUnits = "s"))
	float SaveDebounceSeconds = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Profile|Saving",
		meta = (ClampMin = "0.05", ForceUnits = "s"))
	float SaveTickerIntervalSeconds = 0.10f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Profile|Saving",
		meta = (ClampMin = "0.1", ForceUnits = "s"))
	float SaveRetryDelaySeconds = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Profile|Saving",
		meta = (ClampMin = "0"))
	int32 MaxSaveRetryAttempts = 3;
};

/**
 * Data-driven fragments consumed by GameInstance-lifetime subsystems.
 */
UCLASS(BlueprintType, Const)
class LABPROJECT_API UPdGameInstanceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPdGameInstanceDefinition();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	static FSoftObjectPath GetDefaultDefinitionPath();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	const FPdGameInstanceLifecycleSettings& GetLifecycleSettings() const { return Lifecycle; }
	const FPdPlayerProfilePersistenceSettings& GetProfilePersistenceSettings() const { return ProfilePersistence; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Lifecycle",
		meta = (AllowPrivateAccess = "true"))
	FPdGameInstanceLifecycleSettings Lifecycle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Profile",
		meta = (AllowPrivateAccess = "true"))
	FPdPlayerProfilePersistenceSettings ProfilePersistence;
};
