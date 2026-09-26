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

struct FGameFeatureActorExtensionHandles
{
	TArray<TSharedPtr<FActorExtensionHandle>> ExtensionRequestHandles;
	TMap<TWeakObjectPtr<AActor>, FActorExtension> ActorExtensions;
};

/**
 * GameFeature 액션 실행 순서:
 * 1. 프로젝트 태그 설정
 * 2. 컴포넌트 추가
 * 3. 속성 추가
 * 4. 능력 추가
 * 5. 액터 확장 추가
 *
 * NetworkReady·HasInputComponent·BindInput·InitAbilitySystem은 앞선 설정에 의존하므로
 * 액터 확장 추가는 일반적으로 마지막에 실행한다.
 */
UCLASS(meta = (DisplayName = "Add Actor Extension"))
class LABPROJECT_API UGameFeatureAction_AddActorExtension : public UGameFeatureAction_WorldNetworkBase
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

#if WITH_EDITORONLY_DATA
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) override;
#endif

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	bool CanActivateActorExtension(AActor* Actor) const;
	void ActivateActorExtension(AActor* Actor, FGameFeatureStateChangeContext ChangeContext);
	void DeactivateActorExtension(AActor* Actor, FGameFeatureStateChangeContext ChangeContext);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	void RegisterActorExtension(UWorld* World, FGameFeatureStateChangeContext ChangeContext);
	void DeactivateAllActorExtensions(FGameFeatureActorExtensionHandles& Handles) const;

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
	TMap<FGameFeatureStateChangeContext, FGameFeatureActorExtensionHandles> ContextHandles;
};
