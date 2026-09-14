#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Skill/SkillAction.h"
#include "AbilitySystem/Ability/SkillAbility.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "GameplayTagContainer.h"
#include "SkillShieldAction.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UGameplayEffect;

UCLASS(meta = (DisplayName = "Shield"))
class LABPROJECT_API USkillShieldAction : public USkillAction
{
	GENERATED_BODY()

protected:
	virtual void OnStart() override;

	virtual void OnStop() override;

private:
	UAnimMontage* GetResolvedShieldMontage() const;
	TSubclassOf<UGameplayEffect> GetResolvedShieldGameplayEffectClass() const;
	FGameplayTag GetResolvedMontageTriggerEventTag() const;
	void StartWaitMontageTriggerTask();
	bool StartShieldMontageTask();
	void ApplyShieldFromMontageTrigger();
	void CleanupShieldTasks();

	UFUNCTION()
	void HandleShieldMontageFinished();

	UFUNCTION()
	void HandleMontageTriggerEvent(FGameplayEventData Payload);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ShieldMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitMontageTriggerTask;

	UPROPERTY(Transient)
	bool bShieldApplied = false;
};
