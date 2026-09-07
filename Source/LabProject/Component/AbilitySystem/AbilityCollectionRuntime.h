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
 * 입력 전달과 능력 목록을 관리한다.
 *
 * 능력 부여·조회, 눌렀던 입력의 추적, 판도라 원본 객체의 수명과 복제 등록을 담당한다.
 */
UCLASS()
class LABPROJECT_API UAbilityCollectionRuntime : public UObject
{
	GENERATED_BODY()

public:
	void ClearPressedAbilityInputs() { PressedAbilityHandles.Reset(); }
	void AbilityInputTagPressed(UPdAbilitySystemComponent& AbilitySystemComponent, const FGameplayTag& InputTag);
	void AbilityInputTagReleased(UPdAbilitySystemComponent& AbilitySystemComponent, const FGameplayTag& InputTag);
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

	void CachePandoraSkillRuntimeContext(UPdAbilitySystemComponent& AbilitySystemComponent, UObject* SourceObject);
	void ReleasePandoraSkillRuntimeContextIfUnused(
		UPdAbilitySystemComponent& AbilitySystemComponent,
		UPandoraSkillRuntimeContext* RuntimeContext,
		FGameplayAbilitySpecHandle RemovedHandle);

private:
	TMap<FGameplayTag, TArray<FGameplayAbilitySpecHandle>> PressedAbilityHandles;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPandoraSkillRuntimeContext>> GrantedPandoraSkillRuntimeContexts;

};
