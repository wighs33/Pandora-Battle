#pragma once

#include "CoreMinimal.h"
#include "SkillMovementSettings.generated.h"

/** 액션들이 공유하는 이동 속도 보정, 이동 제한과 접촉 피해 설정. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillMovementSettings
{
	GENERATED_BODY()

	/** 오라·배치 액션 실행 중 이동 속도 증가 효과를 적용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Movement", meta = (DisplayName = "Increase Movement Speed While Active"))
	bool bOverrideMovementSpeedWhileActive = false;

	/** 오라·배치 액션 유지 중 이동 속도 증가율(%). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Movement|Speed", meta = (EditCondition = "bOverrideMovementSpeedWhileActive", ClampMin = "0.0", ForceUnits = "%"))
	double MovementSpeedBonusPercent = 0.0;

	// 이동 제한과 접촉 피해

	/** 스킬이 유지되는 동안 캐릭터 이동을 제한한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Movement", meta = (DisplayName = "Lock Movement During Duration"))
	bool bLockMovementDuringDuration = false;

	/** 이동 경로에서 접촉한 적에게 공통 Damage 설정으로 피해를 적용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Movement|Contact Damage")
	bool bDamageEnemiesOnContact = false;
};
