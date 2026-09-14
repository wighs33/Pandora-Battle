#pragma once

#include "AbilitySystem/Skill/SkillAction.h"
#include "AbilitySystem/Ability/SkillAbility.h"
#include "Definition/AbilitySystem/SkillPresentationSettings.h"
#include "SkillWeaponTrailAction.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitDelay;

/** 무기의 궤적과 검기 타격을 몽타주 이벤트에 연결한다. */
UCLASS(meta = (DisplayName = "Weapon Trail"))
class LABPROJECT_API USkillWeaponTrailAction : public USkillAction
{
	GENERATED_BODY()

public:

	/** 무기의 궤적과 검기 타격을 몽타주 이벤트에 연결한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ShowOnlyInnerProperties))
	FSkillSwordTrailSettings Settings;

protected:
	virtual void OnStart() override;

	virtual void OnStop() override;

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
