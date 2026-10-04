#pragma once

#include "CoreMinimal.h"

struct FGameplayEffectSpec;

// 피해 공식. 공격자가 내보낼 피해를 정할 때와 속성 세트가 들어온 피해를 줄일 때 같은 규칙을 쓰도록 상태 없이 모은다.
namespace PdDamageRules
{
	/** 피해 GE Spec이 어떤 상태 이상 피해인지. 상태 이상 피해는 맞는 쪽 저항으로 줄고 피격 반응이 없다. */
	enum class EStatusDamage : uint8
	{
		None,
		Burning,
		Frozen,
		ElectricShock
	};

	/** Spec의 동적·정의 태그에서 화상·동상·감전 순으로 상태 이상 피해 종류를 찾는다. */
	LABPROJECT_API EStatusDamage ClassifyStatusDamage(const FGameplayEffectSpec& EffectSpec);

	/** 치명타 판정. RollPercent(0~100)가 치명타 확률보다 작으면 2배에 치명타 수치 1%씩 더한 배율을 곱한다. */
	LABPROJECT_API float CalculateCriticalDamage(float BaseDamage, float Critical, float RollPercent, bool& bOutCriticalHit);

	/** 상태 이상 피해에 상태별 피해 증가율(%)과 추가 배율을 곱한다. 음수 입력은 0으로 본다. */
	LABPROJECT_API float CalculateStatusEffectDamage(float SkillScaledDamage, float DamageBonusPercent, float DamageScale);

	/** 맞는 쪽 상태 저항(0~100%)만큼 상태 이상 피해를 줄인다. */
	LABPROJECT_API float MitigateByStatusResistance(float Damage, float ResistancePercent);

	/** 방어력(0~100%)에 맞는 쪽 근력 반영 무기 피해를 곱한 값만큼 피해를 깎는다. */
	LABPROJECT_API float MitigateByArmor(float Damage, float ArmorPercent, float FinalStrengthDamage);

	/** 보호막이 먼저 피해를 받고 남은 피해를 체력이 받는다. */
	struct FShieldAbsorption
	{
		float ShieldDamage = 0.f;
		float HealthDamage = 0.f;
		float RemainingShield = 0.f;
	};

	LABPROJECT_API FShieldAbsorption AbsorbByShield(float Damage, float CurrentShield);
}
