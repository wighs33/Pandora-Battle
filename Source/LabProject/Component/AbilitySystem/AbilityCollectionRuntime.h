#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "AbilityCollectionRuntime.generated.h"

class UGameplayAbility;
class UPandoraSkillRuntimeContext;
class UPdAbilitySystemComponent;

/**
 * Owns ability collection concerns: input routing, queries, grants, source
 * object lifetime, and replicated-list snapshots.
 */
UCLASS()
class LABPROJECT_API UAbilityCollectionRuntime : public UObject
{
	GENERATED_BODY()

public:
	void AbilityInputTagPressed(UPdAbilitySystemComponent& AbilitySystemComponent, const FGameplayTag& InputTag) const;
	void AbilityInputTagReleased(UPdAbilitySystemComponent& AbilitySystemComponent, const FGameplayTag& InputTag) const;
	void ReplayReleasedPressInputAfterActivation(
		UPdAbilitySystemComponent& AbilitySystemComponent,
		FGameplayAbilitySpecHandle AbilityHandle) const;

	const FGameplayAbilitySpec* FindActiveAbilitySpecByTags(
		const UPdAbilitySystemComponent& AbilitySystemComponent,
		const FGameplayTagContainer& AbilityTags) const;

	bool HasActiveAbilityOfClass(
		const UPdAbilitySystemComponent& AbilitySystemComponent,
		TSubclassOf<UGameplayAbility> AbilityClass,
		bool bIncludeChildClasses) const;

	bool HasActiveAbilityOfAnyClass(
		const UPdAbilitySystemComponent& AbilitySystemComponent,
		const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses,
		bool bIncludeChildClasses) const;

	TArray<FGameplayAbilitySpecHandle> GrantAbilities(
		UPdAbilitySystemComponent& AbilitySystemComponent,
		const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses,
		int32 AbilityLevel,
		UObject* SourceObject);

	void RemoveAbilities(
		UPdAbilitySystemComponent& AbilitySystemComponent,
		const TArray<FGameplayAbilitySpecHandle>& AbilityHandles) const;

	void ReactivateAutoActivatedAbilities(UPdAbilitySystemComponent& AbilitySystemComponent) const;

	void CachePandoraSkillRuntimeContext(UObject* SourceObject);
	void ReleasePandoraSkillRuntimeContextIfUnused(
		const UPdAbilitySystemComponent& AbilitySystemComponent,
		UPandoraSkillRuntimeContext* RuntimeContext,
		FGameplayAbilitySpecHandle RemovedHandle);

	bool IsPandoraAbilitySpec(const FGameplayAbilitySpec& AbilitySpec) const;
	bool HasReplicatedAbilityListChanged(const UPdAbilitySystemComponent& AbilitySystemComponent) const;
	void CacheReplicatedAbilityList(const UPdAbilitySystemComponent& AbilitySystemComponent);

private:
	bool HasGrantedAbilityClass(
		const UPdAbilitySystemComponent& AbilitySystemComponent,
		TSubclassOf<UGameplayAbility> AbilityClass) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPandoraSkillRuntimeContext>> GrantedPandoraSkillRuntimeContexts;

	TArray<FGameplayAbilitySpecHandle> LastReplicatedAbilityHandles;
	TArray<TSubclassOf<UGameplayAbility>> LastReplicatedAbilityClasses;
};
