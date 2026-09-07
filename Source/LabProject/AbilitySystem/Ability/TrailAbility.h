#pragma once

#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "TrailAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitDelay;

UCLASS(Blueprintable)
class LABPROJECT_API UTrailAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UTrailAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void OnAbilityEnding() override;

private:
	UFUNCTION()
	void HandleTrailMontageCompleted();

	UFUNCTION()
	void HandleTrailMontageInterrupted();

	UFUNCTION()
	void HandleTrailDurationFinished();

	UFUNCTION()
	void HandleTrailAttackTraceStart(FGameplayEventData Payload);

	UFUNCTION()
	void HandleTrailAttackTraceEnd(FGameplayEventData Payload);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> TrailMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> TrailDurationTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> TrailAttackTraceStartTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> TrailAttackTraceEndTask;

	UPROPERTY(Transient)
	bool bStartedWeaponTrail = false;
};
