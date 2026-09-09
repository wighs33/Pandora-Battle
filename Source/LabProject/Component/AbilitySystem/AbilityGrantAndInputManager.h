#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "AbilityGrantAndInputManager.generated.h"

class UGameplayAbility;
class UPdAbilitySystemComponent;

/**
 * 입력 전달과 능력 목록을 관리한다.
 *
 * 능력 부여·조회와 입력 수집·전달을 담당한다. 입력 유형별 실행 규칙은 능력에 맡긴다.
 */
UCLASS()
class LABPROJECT_API UAbilityGrantAndInputManager : public UObject
{
	GENERATED_BODY()

public:
	void ClearAllAbilityInputs()
	{
		PressedAbilityHandles.Reset();
		AbilityInputStates.Reset();
	}
	void ClearAbilityInput(FGameplayAbilitySpecHandle Handle);
	void ProcessAbilityInput(UPdAbilitySystemComponent& AbilitySystemComponent);
	void QueueAbilityInputPressed(UPdAbilitySystemComponent& AbilitySystemComponent, const FGameplayTag& InputTag);
	void QueueAbilityInputReleased(const FGameplayTag& InputTag);

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

private:
	struct FAbilityInputState
	{
		bool bPressPending = false;
		bool bReleasePending = false;
		bool bActivationRequested = false;
	};

	void SendInputToActiveAbility(UPdAbilitySystemComponent& AbilitySystemComponent, FGameplayAbilitySpecHandle Handle, bool bPressed);

	TMap<FGameplayTag, TArray<FGameplayAbilitySpecHandle>> PressedAbilityHandles;
	TMap<FGameplayAbilitySpecHandle, FAbilityInputState> AbilityInputStates;
};
