#pragma once

#include "CoreMinimal.h"
#include "GameFeature/GameFeatureAction_WorldNetworkBase.h"
#include "GameFeature/Extension/ActorExtension.h"
#include "GameFeatureAction_AddActorExtension.generated.h"

class AActor;
class FActorExtensionHandle;
class UWorld;
struct FAssetBundleData;
struct FGameFeatureDeactivatingContext;
struct FGameFeatureStateChangeContext;
struct FWorldContext;

DECLARE_LOG_CATEGORY_EXTERN(PdGameFeatureAction_AddActorExtensionLog, Log, All);

struct FPdGameFeatureActorExtensionHandles
{
	TArray<TSharedPtr<FActorExtensionHandle>> ExtensionRequestHandles;
	TMap<TWeakObjectPtr<AActor>, FActorExtension> ActorExtensions;
};

/**
 * GameFeature action order:
 * 1. Set Project Tag Config
 * 2. Add Components
 * 3. Add Attributes
 * 4. Add Abilities
 * 5. Add Actor Extension
 *
 * Add Actor Extension should usually run last because NetworkReady, HasInputComponent, BindInput, and InitAbilitySystem depend on earlier setup.
 */
UCLASS(meta = (DisplayName = "Add Actor Extension"))
class LABPROJECT_API UGameFeatureAction_AddActorExtension : public UGameFeatureAction_WorldNetworkBase
{
	GENERATED_BODY()

public:
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

#if WITH_EDITORONLY_DATA
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) override;
#endif

private:
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	void RegisterActorExtension(UWorld* World, FGameFeatureStateChangeContext ChangeContext);
	bool CanActivateActorExtension(AActor* Actor) const;
	void ActivateActorExtension(AActor* Actor, FGameFeatureStateChangeContext ChangeContext);
	void DeactivateActorExtension(AActor* Actor, FGameFeatureStateChangeContext ChangeContext);
	void DeactivateAllActorExtensions(FPdGameFeatureActorExtensionHandles& Handles) const;

public:
	UPROPERTY(EditAnywhere, Category = "Extension", meta = (AllowAbstract = "false"))
	TSoftClassPtr<AActor> TargetClass;

	UPROPERTY(EditAnywhere, Category = "Extension")
	FActorExtension Extension;

	UPROPERTY(EditAnywhere, Category = "Network", meta = (EditCondition = "bClientAction"))
	uint8 bAddToLocallyControlled : 1 = false;

	UPROPERTY(EditAnywhere, Category = "Network", meta = (EditCondition = "bClientAction"))
	uint8 bAddToSimulatedProxy : 1 = false;

private:
	TMap<FGameFeatureStateChangeContext, FPdGameFeatureActorExtensionHandles> ContextHandles;
};
