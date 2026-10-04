#pragma once

#include "Skill/Actions/SkillAction.h"
#include "Definition/AbilitySystem/SkillPresentationSettings.h"
#include "SkillWeaponTrailAction.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitDelay;
class AMeleeWeapon;
class AWeaponBase;
class USkillDefinition;

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
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnStart() override;

	virtual void OnStop() override;

private:
	UFUNCTION()
	void HandleTrailMontageFinished();

	UFUNCTION()
	void HandleTrailDurationFinished();

	UFUNCTION()
	void HandleTrailAttackTraceStart(FGameplayEventData Payload);

	UFUNCTION()
	void HandleTrailAttackTraceEnd(FGameplayEventData Payload);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	/** 궤적·검기를 붙일 현재 무기. 궤적을 쓰는데 무기에 궤적 컴포넌트가 없으면 nullptr. */
	AWeaponBase* ResolveTrailWeapon(bool bNeedsTrailComponent) const;
	/** 스킬 피해·상태 이상을 계산해 근접 무기의 검기에 넘긴다. 근접 무기가 아니면 계산만 한다. */
	void PrepareSlash(AMeleeWeapon* MeleeWeapon, const USkillDefinition& SkillDataAsset);
	/** 몽타주의 콤보 입력 열림·닫힘 이벤트에 검기 타격 판정을 맞춘다. */
	void ListenForSlashHitWindow();
	/** 몽타주가 있으면 몽타주가 끝날 때, 없으면 스킬 지속 시간이 끝날 때 액션을 마친다. */
	void WaitForTrailEnd(const USkillDefinition& SkillDataAsset);

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> TrailMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> TrailDurationTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> TrailAttackTraceStartTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> TrailAttackTraceEndTask;

	bool bStartedWeaponTrail = false;
	TWeakObjectPtr<AWeaponBase> TrailWeapon;
};
