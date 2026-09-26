#pragma once

#include "CoreMinimal.h"
#include "GameFeature/GameFeatureAction_WorldNetworkBase.h"
#include "GameFeatureAction_AddAttributes.generated.h"

class AActor;
class UAttributeSet;
class UPdAbilitySystemComponent;
class UWorld;
class FActorExtensionHandle;
struct FAssetBundleData;
struct FGameFeatureDeactivatingContext;
struct FGameFeatureStateChangeContext;
struct FWorldContext;

DECLARE_LOG_CATEGORY_EXTERN(PdGameFeatureAction_AddAttributesLog, Log, All);

struct FGameFeatureAttributeHandles
{
	TArray<TSharedPtr<FActorExtensionHandle>> ExtensionRequestHandles;
	// 키는 초기화한 액터, 값은 이 피처가 생성해 해제할 속성 집합이다. 빈 값도 중복 초기화를 막는다.
	TMap<TWeakObjectPtr<AActor>, TArray<TWeakObjectPtr<UAttributeSet>>> AttributeSets;
};

/**
 * GameFeature 액션 실행 순서:
 * 1. 프로젝트 태그 설정
 * 2. 컴포넌트 추가
 * 3. 속성 추가
 * 4. 능력 추가
 * 5. 액터 확장 추가
 *
 * 속성 추가는 필요한 기능 컴포넌트가 생성된 후, 액터 확장에서 RequiredAttributeSets를 확인하기 전에 실행한다.
 */
UCLASS(meta = (DisplayName = "Add Attributes"))
class LABPROJECT_API UGameFeatureAction_AddAttributes : public UGameFeatureAction_WorldNetworkBase
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void PostLoad() override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

#if WITH_EDITORONLY_DATA
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) override;
#endif

	// Public API ------------------------------------------------------------------------------------------------------
	UGameFeatureAction_AddAttributes();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	void RegisterAttributeExtension(UWorld* World, FGameFeatureStateChangeContext ChangeContext);

	void AddAttributesToActor(AActor* Actor, FGameFeatureAttributeHandles& Handles);
	void RemoveAttributesFromActor(AActor* Actor, FGameFeatureAttributeHandles& Handles) const;
	void RemoveAllAttributes(FGameFeatureAttributeHandles& Handles) const;
	void AddAttributeSetsToActor(AActor* Actor, UPdAbilitySystemComponent* AbilitySystemComponent,
		FGameFeatureAttributeHandles& Handles) const;
	void CollectTargetClasses(TArray<TSubclassOf<AActor>>& OutTargetClasses) const;
	void CollectAttributeSetClasses(TArray<TSubclassOf<UAttributeSet>>& OutAttributeSetClasses) const;
	UAttributeSet* FindExistingAttributeSet(AActor* Actor, TSubclassOf<UAttributeSet> AttributeSetClass) const;
	UPdAbilitySystemComponent* GetAbilitySystemComponent(AActor* Actor) const;

public:
	UPROPERTY(EditAnywhere, Category = "Attributes", meta = (AllowAbstract = "false"))
	TArray<TSoftClassPtr<AActor>> TargetClasses;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use TargetClasses instead."))
	TSoftClassPtr<AActor> TargetClass;

	UPROPERTY(EditAnywhere, Category = "Attributes", meta = (AssetBundles = "Client,Server"))
	TArray<TSoftClassPtr<UAttributeSet>> AttributeSetClasses;

private:
	TMap<FGameFeatureStateChangeContext, FGameFeatureAttributeHandles> ContextHandles;
};
