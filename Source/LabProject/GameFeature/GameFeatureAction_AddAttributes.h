#pragma once

#include "CoreMinimal.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "GameFeature/GameFeatureAction_WorldNetworkBase.h"
#include "GameFeatureAction_AddAttributes.generated.h"

class AActor;
class UAttributeSet;
class UWorld;
class FActorExtensionHandle;
struct FAssetBundleData;
struct FGameFeatureDeactivatingContext;
struct FGameFeatureStateChangeContext;
struct FWorldContext;

DECLARE_LOG_CATEGORY_EXTERN(PdGameFeatureAction_AddAttributesLog, Log, All);

struct FPdGameFeatureAttributeHandles
{
	TArray<TSharedPtr<FActorExtensionHandle>> ExtensionRequestHandles;
	TMap<TWeakObjectPtr<AActor>, int32> AttributeConfigHandles;
	TMap<TWeakObjectPtr<AActor>, TArray<TWeakObjectPtr<UAttributeSet>>> AttributeSets;
};

/**
 * GameFeature action order:
 * 1. Set Project Tag Config
 * 2. Add Components
 * 3. Add Attributes
 * 4. Add Abilities
 * 5. Add Actor Extension
 *
 * Add Attributes should run after required feature components exist and before Actor Extension checks RequiredAttributeSets.
 */
UCLASS(meta = (DisplayName = "Add Attributes"))
class LABPROJECT_API UGameFeatureAction_AddAttributes : public UGameFeatureAction_WorldNetworkBase
{
	GENERATED_BODY()

public:
	UGameFeatureAction_AddAttributes();

	//------------------------------------------------------------------------------------------------------------------
	//--- Game Feature Events
	virtual void PostLoad() override;
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
	void RegisterAttributeExtension(UWorld* World, FGameFeatureStateChangeContext ChangeContext);

	//------------------------------------------------------------------------------------------------------------------
	//--- Attribute Setup
	void AddAttributesToActor(AActor* Actor, FPdGameFeatureAttributeHandles& Handles);
	void RemoveAttributesFromActor(AActor* Actor, FPdGameFeatureAttributeHandles& Handles) const;
	void RemoveAllAttributes(FPdGameFeatureAttributeHandles& Handles) const;
	void AddAttributeSetsToActor(AActor* Actor, UPdAbilitySystemComponent* AbilitySystemComponent,
		FPdGameFeatureAttributeHandles& Handles) const;
	void CollectTargetClasses(TArray<TSubclassOf<AActor>>& OutTargetClasses) const;
	void CollectAttributeSetClasses(TArray<TSubclassOf<UAttributeSet>>& OutAttributeSetClasses) const;
	UAttributeSet* FindExistingAttributeSet(AActor* Actor, TSubclassOf<UAttributeSet> AttributeSetClass) const;
	UPdAbilitySystemComponent* GetAbilitySystemComponent(AActor* Actor) const;

public:
	//------------------------------------------------------------------------------------------------------------------
	//--- Attribute Setup
	UPROPERTY(EditAnywhere, Category = "Attributes", meta = (AllowAbstract = "false"))
	TArray<TSoftClassPtr<AActor>> TargetClasses;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use TargetClasses instead."))
	TSoftClassPtr<AActor> TargetClass;

	UPROPERTY(EditAnywhere, Category = "Attributes", meta = (AssetBundles = "Client,Server"))
	TArray<TSoftClassPtr<UAttributeSet>> AttributeSetClasses;

	UPROPERTY(EditAnywhere, Category = "Attributes")
	FPdAttributeConfig AttributeConfig;

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Runtime State
	TMap<FGameFeatureStateChangeContext, FPdGameFeatureAttributeHandles> ContextHandles;
};
