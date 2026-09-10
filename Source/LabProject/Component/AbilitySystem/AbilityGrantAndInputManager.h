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
	// 능력 부여·회수와 자동 실행
	TArray<FGameplayAbilitySpecHandle> GrantAbilities(UPdAbilitySystemComponent& AbilitySystemComponent,
		const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses, int32 AbilityLevel, UObject* SourceObject);

	void RemoveAbilities(UPdAbilitySystemComponent& AbilitySystemComponent, const TArray<FGameplayAbilitySpecHandle>& AbilityHandles) const;

	void ReactivateAutoActivatedAbilities(UPdAbilitySystemComponent& AbilitySystemComponent) const;

	// 실행 중 능력 조회
	const FGameplayAbilitySpec* FindActiveAbilitySpecByTags(
		const UPdAbilitySystemComponent& AbilitySystemComponent, const FGameplayTagContainer& AbilityTags) const;

	bool HasActiveAbilityOfClass(const UPdAbilitySystemComponent& AbilitySystemComponent, TSubclassOf<UGameplayAbility> AbilityClass,
		bool bIncludeChildClasses) const;

	bool HasActiveAbilityOfAnyClass(const UPdAbilitySystemComponent& AbilitySystemComponent,
		const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses, bool bIncludeChildClasses) const;

	// 프레임 입력 수집·전달·정리
	void QueueAbilityInputPressed(UPdAbilitySystemComponent& AbilitySystemComponent, const FGameplayTag& InputTag);

	void QueueAbilityInputReleased(const FGameplayTag& InputTag);

	void ProcessAbilityInput(UPdAbilitySystemComponent& AbilitySystemComponent);

	void ClearAbilityInput(FGameplayAbilitySpecHandle Handle);

	// 사망·초기화 시 누른 키와 대기 입력을 모두 비운다.
	void ClearAllAbilityInputs()
	{
		AbilityHandlesByPressedInputTag.Reset();
		AbilityInputStates.Reset();
	}

private:
	struct FAbilityInputState
	{
		bool bPressPending = false;
		bool bReleasePending = false;
		// 서버 응답을 기다리는 동안 같은 활성화 요청을 다시 보내지 않는다.
		bool bActivationRequestSent = false;
	};

	void SendInputToActiveAbility(UPdAbilitySystemComponent& AbilitySystemComponent, FGameplayAbilitySpecHandle Handle, bool bPressed);

	// 슬롯이 바뀌어도 누를 때 선택한 능력에 해제를 전달하기 위한 기록이다.
	TMap<FGameplayTag, TArray<FGameplayAbilitySpecHandle>> AbilityHandlesByPressedInputTag;
	TMap<FGameplayAbilitySpecHandle, FAbilityInputState> AbilityInputStates;
};
