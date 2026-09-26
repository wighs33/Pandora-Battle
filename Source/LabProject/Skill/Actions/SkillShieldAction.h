#pragma once

#include "CoreMinimal.h"
#include "Skill/Actions/SkillAction.h"
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
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnStart() override;

	virtual void OnStop() override;

private:
	UFUNCTION()
	void HandleShieldMontageFinished();

	UFUNCTION()
	void HandleMontageTriggerEvent(FGameplayEventData Payload);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	UAnimMontage* GetResolvedShieldMontage() const;
	TSubclassOf<UGameplayEffect> GetResolvedShieldGameplayEffectClass() const;
	FGameplayTag GetResolvedMontageTriggerEventTag() const;
	void StartWaitMontageTriggerTask();
	bool StartShieldMontageTask();
	void ApplyShieldFromMontageTrigger();
	void CleanupShieldTasks();

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ShieldMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitMontageTriggerTask;

	UPROPERTY(Transient)
	bool bShieldApplied = false;
};
