#pragma once

#include "CoreMinimal.h"
#include "Skill/Actions/SkillAction.h"
#include "Skill/Actions/SkillMovementContactDamage.h"
#include "SkillDashAction.generated.h"

class UAbilityTask_ApplyRootMotionConstantForce;

/** 입력 방향이나 대상 쪽으로 루트 모션 돌진을 한다. */
UCLASS(meta = (DisplayName = "Dash"))
class LABPROJECT_API USkillDashAction : public USkillAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "!Skill", meta = (ClampMin = "0", Units = "cm/s"))
	float Speed = 1000.0f;
	UPROPERTY(EditAnywhere, Category = "!Skill", meta = (ClampMin = "0.01", Units = "s"))
	float Duration = 0.2f;
	UPROPERTY(EditAnywhere, Category = "!Skill")
	bool bEnableGravity = false;
	/** 대시 GameplayCue가 실행되는 동안 캐릭터를 숨긴다. */
	UPROPERTY(EditAnywhere, Category = "!Skill")
	bool bHideCharacter = true;

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnStart() override;
	virtual void OnStop() override;

private:
	UFUNCTION()
	void Completed();

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_ApplyRootMotionConstantForce> Task;

	/** 대시로 움직이는 동안만 닿은 적에게 피해를 준다. */
	FSkillMovementContactDamage ContactDamage;
};
