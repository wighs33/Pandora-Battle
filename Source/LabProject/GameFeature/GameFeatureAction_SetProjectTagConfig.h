#pragma once

#include "CoreMinimal.h"
#include "GameFeature/GameFeatureAction_WorldNetworkBase.h"
#include "GameFeatureAction_SetProjectTagConfig.generated.h"

class UProjectTagConfig;
class UWorld;
struct FAssetBundleData;
struct FGameFeatureDeactivatingContext;
struct FGameFeatureStateChangeContext;
struct FWorldContext;

DECLARE_LOG_CATEGORY_EXTERN(PdGameFeatureAction_SetProjectTagConfigLog, Log, All);

struct FPdProjectTagConfigActionState
{
	TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<UProjectTagConfig>> PreviousConfigsByWorld;
};

/**
 * GameFeature action order:
 * 1. Set Project Tag Config
 * 2. Add Components
 * 3. Add Attributes
 * 4. Add Abilities
 * 5. Add Actor Extension
 *
 * This action should run first so UI, inventory, pandora, skin, combat, and SetByCaller tag lookups use the feature config.
 */
UCLASS(meta = (DisplayName = "Set Project Tag Config"))
class LABPROJECT_API UGameFeatureAction_SetProjectTagConfig : public UGameFeatureAction_WorldNetworkBase
{
	GENERATED_BODY()

public:
	UGameFeatureAction_SetProjectTagConfig();

	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

#if WITH_EDITORONLY_DATA
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) override;
#endif

private:
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	void ApplyConfigToWorld(UWorld* World, const UProjectTagConfig* Config) const;

public:
	UPROPERTY(EditAnywhere, Category = "Tags", meta = (AssetBundles = "Client,Server"))
	TSoftObjectPtr<UProjectTagConfig> ProjectTagConfig;

private:
	TMap<FGameFeatureStateChangeContext, FPdProjectTagConfigActionState> ContextStates;
};
