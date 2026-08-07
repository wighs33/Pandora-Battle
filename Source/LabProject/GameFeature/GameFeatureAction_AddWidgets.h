#pragma once

#include "CoreMinimal.h"
#include "GameFeature/GameFeatureAction_WorldNetworkBase.h"
#include "GameFeatureAction_AddWidgets.generated.h"

class AActor;
class APdHUD;
class FActorExtensionHandle;
class FWidgetContentBundleLease;
class UWidgetClassDefinition;
class UWorld;
struct FAssetBundleData;
struct FGameFeatureDeactivatingContext;
struct FGameFeatureStateChangeContext;
struct FWorldContext;

DECLARE_LOG_CATEGORY_EXTERN(PdGameFeatureAction_AddWidgetsLog, Log, All);

struct FGameFeatureWidgetHandles
{
	TArray<TSharedPtr<FActorExtensionHandle>> ExtensionRequestHandles;
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<UWidgetClassDefinition>> WidgetDefinitionsByActor;
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<UWidgetClassDefinition>> PendingWidgetDefinitionsByActor;
	TMap<TWeakObjectPtr<AActor>, TArray<TSharedPtr<FWidgetContentBundleLease>>>
		WidgetContentLeasesByActor;
};

UCLASS(meta = (DisplayName = "Add Widgets"))
class LABPROJECT_API UGameFeatureAction_AddWidgets : public UGameFeatureAction_WorldNetworkBase
{
	GENERATED_BODY()

public:
	UGameFeatureAction_AddWidgets();

	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

#if WITH_EDITORONLY_DATA
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) override;
#endif

private:
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	void RegisterWidgetExtension(UWorld* World, FGameFeatureStateChangeContext ChangeContext);
	bool CanActivateWidgetExtension(AActor* Actor) const;
	void AddWidgetsToActor(AActor* Actor, FGameFeatureStateChangeContext ChangeContext);
	void CompleteAddWidgetsToActor(
		AActor* Actor,
		FGameFeatureStateChangeContext ChangeContext,
		UWidgetClassDefinition* ExpectedWidgetClassDefinition);
	void RemoveWidgetsFromActor(AActor* Actor, FGameFeatureStateChangeContext ChangeContext);
	void RemoveAllWidgets(FGameFeatureWidgetHandles& Handles) const;

public:
	UPROPERTY(EditAnywhere, Category = "UI", meta = (AllowAbstract = "false"))
	TSoftClassPtr<APdHUD> TargetHudClass;

	UPROPERTY(EditAnywhere, Category = "UI", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UWidgetClassDefinition> WidgetClassDefinition;

private:
	TMap<FGameFeatureStateChangeContext, FGameFeatureWidgetHandles> ContextHandles;
};
