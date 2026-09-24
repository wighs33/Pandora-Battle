#pragma once

#include "CoreMinimal.h"
#include "Definition/AbilitySystem/SkillGameplayEffectConfig.h"
#include "SkillEffectSettings.generated.h"

class UGameplayEffect;

/** 스킬의 공통 피해 정의와 트리거 반복 주기. 각 기능은 이 값을 직접 읽는다. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillTopLevelDamageConfig
{
	GENERATED_BODY()

public:
	/** 적용할 GameplayEffect 클래스. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	TSubclassOf<UGameplayEffect> GameplayEffectClass;

	/** Magnitude를 전달할 SetByCaller 태그. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (Categories = "Data"))
	FGameplayTag MagnitudeDataTag;

	/** GameplayEffect에 전달할 기본 수치. 피해로 사용할 때 공격력 보정이 적용된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	double Magnitude = 0.0;

	/** 배치·소환 액터의 트리거 안에 머무는 대상에게 피해를 반복 적용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (DisplayName = "Repeat Trigger Damage While Overlapping"))
	bool bRepeatTriggerDamageWhileOverlapping = false;

	/** 트리거의 반복 피해 간격(초). 최소 0.05초. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (EditCondition = "bRepeatTriggerDamageWhileOverlapping", EditConditionHides, ClampMin = "0.05", ForceUnits = "s", DisplayName = "Trigger Damage Interval"))
	double TriggerDamageInterval = 1.0;

	// Public API ------------------------------------------------------------------------------------------------------
	FSkillGameplayEffectConfig ToGameplayEffectConfig() const
	{
		FSkillGameplayEffectConfig Config;
		Config.GameplayEffectClass = GameplayEffectClass;
		Config.MagnitudeDataTag = MagnitudeDataTag;
		Config.Magnitude = Magnitude;
		return Config;
	}
};

/** 시전자에게 적용하는 GameplayEffect와 강화 보정. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillSelfBuffSettings
{
	GENERATED_BODY()

	/** 이 버프 또는 회복 기능을 활성화한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Self Buff")
	bool bEnabled = false;

	/** 적용할 GameplayEffect 클래스. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Self Buff", meta = (EditCondition = "bEnabled"))
	TSubclassOf<UGameplayEffect> GameplayEffectClass;

	/** Magnitude를 전달할 SetByCaller 태그. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Self Buff", meta = (EditCondition = "bEnabled", Categories = "Data"))
	FGameplayTag MagnitudeDataTag;

	/** GameplayEffect에 전달할 기본 수치. 피해로 사용할 때 공격력 보정이 적용된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Self Buff", meta = (EditCondition = "bEnabled"))
	double Magnitude = 0.0;

	/** 버프가 활성화된 동안 무기 공격 피해에 더할 보정값. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Self Buff|Weapon Damage", meta = (EditCondition = "bEnabled", DisplayName = "Weapon Damage Bonus"))
	double WeaponDamageBonus = 0.0;

	/** 버프가 활성화된 동안 캐릭터 크기에 적용할 배율. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Self Buff|Character", meta = (EditCondition = "bEnabled", ClampMin = "1.0"))
	double CharacterScaleMultiplier = 1.0;

	/** 버프 중 무기 타격 트레이스 끝점의 Z 길이에 적용할 배율. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Self Buff|Weapon Trace", meta = (EditCondition = "bEnabled", ClampMin = "1.0", DisplayName = "Trace End Z Multiplier"))
	double WeaponTraceEndZMultiplier = 1.0;

	/** Ability 종료 시 자신에게 적용한 GameplayEffect를 제거한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Self Buff", meta = (EditCondition = "bEnabled"))
	bool bRemoveOnAbilityEnd = true;
};

/** 아군 범위 회복. 오라의 장판 생성 설정과 독립적이다. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillHealSettings
{
	GENERATED_BODY()

	/** 이 버프 또는 회복 기능을 활성화한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Heal")
	bool bEnabled = false;

	/** 회복 대상에게 적용할 GameplayEffect와 SetByCaller 회복량. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Heal", meta = (EditCondition = "bEnabled"))
	FSkillGameplayEffectConfig TeamHealEffect;

	/** 회복 범위 반경(cm). 0이면 지정된 상호작용 컴포넌트 크기를 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Heal", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ForceUnits = "cm"))
	double HealRadius = 0.0;

	/** 회복 효과 적용 간격(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Heal", meta = (EditCondition = "bEnabled", ClampMin = "0.05", ForceUnits = "s"))
	double HealInterval = 1.0;

	/** 시전자 자신도 회복 대상에 포함한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Heal", meta = (EditCondition = "bEnabled"))
	bool bHealSelf = false;

	/** 회복 범위의 대체 반경을 구할 상호작용 컴포넌트 이름. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Heal", meta = (EditCondition = "bEnabled"))
	FName HealingInteractionComponentName = TEXT("InteractionBox");
};
