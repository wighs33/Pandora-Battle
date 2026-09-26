#pragma once

#include "CoreMinimal.h"
#include "GameFeature/GameFeatureAction_WorldNetworkBase.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "GameFeatureAction_AddAbilities.generated.h"

class AActor;
class UAbilitySystemComponent;
class UGameplayAbility;
class UWorld;
class FActorExtensionHandle;
struct FAssetBundleData;
struct FGameFeatureDeactivatingContext;
struct FGameFeatureStateChangeContext;
struct FWorldContext;

DECLARE_LOG_CATEGORY_EXTERN(PdGameFeatureAction_AddAbilitiesLog, Log, All);

USTRUCT(BlueprintType)
struct FGameFeatureAbilityEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Ability", meta = (AssetBundles = "Server"))
	TSoftClassPtr<UGameplayAbility> Ability;

	UPROPERTY(EditAnywhere, Category = "Ability", meta = (ClampMin = "1"))
	int32 Level = 1;

	UPROPERTY(EditAnywhere, Category = "Ability")
	FGameplayTag InputTag;
};

struct FGameFeatureAbilityGrantHandles
{
	TArray<TSharedPtr<FActorExtensionHandle>> ExtensionRequestHandles;
	TMap<TWeakObjectPtr<AActor>, TArray<FGameplayAbilitySpecHandle>> AbilitySpecHandles;
};

/**
 * GameFeature 액션 실행 순서:
 * 1. 프로젝트 태그 설정
 * 2. 컴포넌트 추가
 * 3. 속성 추가
 * 4. 능력 추가
 * 5. 액터 확장 추가
 *
 * 능력 추가는 ASC와 속성 준비 후, 입력 바인딩으로 능력을 활성화하기 전에 실행한다.
 */
UCLASS(meta = (DisplayName = "Add Abilities"))
class LABPROJECT_API UGameFeatureAction_AddAbilities : public UGameFeatureAction_WorldNetworkBase
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

	// Public API ------------------------------------------------------------------------------------------------------
	UGameFeatureAction_AddAbilities();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	void RegisterAbilityExtension(UWorld* World, FGameFeatureStateChangeContext ChangeContext);

	void GrantAbilitiesToActor(AActor* Actor, FGameFeatureAbilityGrantHandles& Handles);
	void RemoveAbilitiesFromActor(AActor* Actor, FGameFeatureAbilityGrantHandles& Handles) const;
	void RemoveAllGrantedAbilities(FGameFeatureAbilityGrantHandles& Handles) const;
	UAbilitySystemComponent* GetAbilitySystemComponent(AActor* Actor) const;
	bool HasAbilityClass(const UAbilitySystemComponent* AbilitySystemComponent, TSubclassOf<UGameplayAbility> AbilityClass) const;

public:
	UPROPERTY(EditAnywhere, Category = "Abilities", meta = (AllowAbstract = "false"))
	TSoftClassPtr<AActor> TargetClass;

	UPROPERTY(EditAnywhere, Category = "Abilities", meta = (TitleProperty = "{Ability}"))
	TArray<FGameFeatureAbilityEntry> Abilities;

private:
	TMap<FGameFeatureStateChangeContext, FGameFeatureAbilityGrantHandles> ContextHandles;
};
