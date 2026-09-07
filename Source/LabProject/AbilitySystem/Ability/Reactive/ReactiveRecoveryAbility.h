#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "ReactiveRecoveryAbility.generated.h"

struct FOnAttributeChangeData;

/**
 * 살아 있는 플레이어의 체력·마나 자동 회복을 관리한다.
 *
 * 회복량이 바뀌어도 적용 중인 GE의 수치만 갱신해 회복 주기를 유지한다.
 * 사망 시 능력 종료로 정리되고 리스폰 시 기존 자동 활성화 흐름으로 재개된다.
 */
UCLASS()
class LABPROJECT_API UReactiveRecoveryAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UReactiveRecoveryAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void OnAbilityEnding() override;

private:
	void HandleRecoveryAttributeChanged(const FOnAttributeChangeData& Data);
	void RefreshRecoveryEffect();

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TMap<FGameplayAttribute, FDelegateHandle> AttributeChangedHandles;
	FActiveGameplayEffectHandle RecoveryEffectHandle;
	bool bRefreshingRecovery = false;
};
