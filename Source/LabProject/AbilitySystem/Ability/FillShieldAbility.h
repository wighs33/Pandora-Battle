#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "GameplayTagContainer.h"
#include "FillShieldAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UGameplayEffect;

UCLASS(Blueprintable)
class LABPROJECT_API UFillShieldAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UFillShieldAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	const FShieldSkillConfig* GetFillShieldSkillConfig() const;
	UAnimMontage* GetConfiguredFillShieldMontage() const;
	TSubclassOf<UGameplayEffect> GetConfiguredFillShieldGameplayEffectClass() const;
	FGameplayTag GetConfiguredMontageTriggerEventTag() const;
	void StartWaitMontageTriggerTask();
	bool StartFillShieldMontageTask();
	void ApplyFillShieldFromMontageTrigger();
	void CleanupFillShieldTasks();

	UFUNCTION()
	void HandleFillShieldMontageFinished();

	UFUNCTION()
	void HandleMontageTriggerEvent(FGameplayEventData Payload);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> FillShieldMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitMontageTriggerTask;

	UPROPERTY(Transient)
	bool bFillShieldApplied = false;
};
