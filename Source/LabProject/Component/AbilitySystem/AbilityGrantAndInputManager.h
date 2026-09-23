#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "AbilityGrantAndInputManager.generated.h"

class UGameplayAbility;
class UAbilitySystemComponent;
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
	// 능력 부여·회수와 자동 실행
	TArray<FGameplayAbilitySpecHandle> GrantAbilities(UPdAbilitySystemComponent& AbilitySystemComponent,
		const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses, int32 AbilityLevel);

	void RemoveAbilities(UPdAbilitySystemComponent& AbilitySystemComponent, const TArray<FGameplayAbilitySpecHandle>& AbilityHandles) const;

	void ReactivateAutoActivatedAbilities(UPdAbilitySystemComponent& AbilitySystemComponent) const;

	// 부여 경로와 관계없이 다음 틱에 활성화를 시도한다. World가 없으면 즉시 시도한다.
	static void TryActivateGrantedAbilityNextTick(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAbilitySpecHandle AbilityHandle);

	// 실행 중 능력 조회
	const FGameplayAbilitySpec* FindActiveAbilitySpecByTags(
		const UPdAbilitySystemComponent& AbilitySystemComponent, const FGameplayTagContainer& AbilityTags) const;

	bool HasActiveAbilityOfClass(const UPdAbilitySystemComponent& AbilitySystemComponent, TSubclassOf<UGameplayAbility> AbilityClass,
		bool bIncludeChildClasses) const;

	bool HasActiveAbilityOfAnyClass(const UPdAbilitySystemComponent& AbilitySystemComponent,
		const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses, bool bIncludeChildClasses) const;

	// 누름은 즉시 전달하고, 해제를 사용하는 능력만 입력 대상을 보관한다.
	void HandleAbilityInputPressed(UPdAbilitySystemComponent& AbilitySystemComponent, const FGameplayTag& InputTag);

	void HandleAbilityInputReleased(UPdAbilitySystemComponent& AbilitySystemComponent, const FGameplayTag& InputTag);

	void ClearAbilityInput(UPdAbilitySystemComponent& AbilitySystemComponent, FGameplayAbilitySpecHandle Handle);

	// 사망·초기화 시 누른 키의 대상을 모두 비운다.
	void ClearAllAbilityInputs()
	{
		HoldAbilityHandlesByInputTag.Reset();
	}

private:
	void SendInputToActiveAbility(UPdAbilitySystemComponent& AbilitySystemComponent, FGameplayAbilitySpecHandle Handle, bool bPressed);

	// Press 스킬·그래플처럼 해제를 사용하는 능력만 누른 시점의 연결을 보관한다.
	TMap<FGameplayTag, TArray<FGameplayAbilitySpecHandle>> HoldAbilityHandlesByInputTag;
};
