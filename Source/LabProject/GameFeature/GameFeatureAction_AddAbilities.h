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
 * GameFeature action order:
 * 1. Set Project Tag Config
 * 2. Add Components
 * 3. Add Attributes
 * 4. Add Abilities
 * 5. Add Actor Extension
 *
 * Add Abilities should run after ASC and attributes are ready, before input binding can activate those abilities.
 */
UCLASS(meta = (DisplayName = "Add Abilities"))
class LABPROJECT_API UGameFeatureAction_AddAbilities : public UGameFeatureAction_WorldNetworkBase
{
	GENERATED_BODY()

public:
	UGameFeatureAction_AddAbilities();

	//------------------------------------------------------------------------------------------------------------------
	//--- Game Feature Events
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

#if WITH_EDITORONLY_DATA
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) override;
#endif

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Activation
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	void RegisterAbilityExtension(UWorld* World, FGameFeatureStateChangeContext ChangeContext);

	//------------------------------------------------------------------------------------------------------------------
	//--- Ability Grants
	void GrantAbilitiesToActor(AActor* Actor, FGameFeatureAbilityGrantHandles& Handles);
	void RemoveAbilitiesFromActor(AActor* Actor, FGameFeatureAbilityGrantHandles& Handles) const;
	void RemoveAllGrantedAbilities(FGameFeatureAbilityGrantHandles& Handles) const;
	UAbilitySystemComponent* GetAbilitySystemComponent(AActor* Actor) const;
	bool HasAbilityClass(const UAbilitySystemComponent* AbilitySystemComponent, TSubclassOf<UGameplayAbility> AbilityClass) const;

public:
	//------------------------------------------------------------------------------------------------------------------
	//--- Ability Setup
	UPROPERTY(EditAnywhere, Category = "Abilities", meta = (AllowAbstract = "false"))
	TSoftClassPtr<AActor> TargetClass;

	UPROPERTY(EditAnywhere, Category = "Abilities", meta = (TitleProperty = "{Ability}"))
	TArray<FGameFeatureAbilityEntry> Abilities;

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Runtime State
	TMap<FGameFeatureStateChangeContext, FGameFeatureAbilityGrantHandles> ContextHandles;
};
