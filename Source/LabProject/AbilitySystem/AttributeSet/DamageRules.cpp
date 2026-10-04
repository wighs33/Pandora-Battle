#include "AbilitySystem/AttributeSet/DamageRules.h"

#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"

namespace PdDamageRules
{
	namespace
	{
		constexpr float MaxPercentEffectValue = 100.f;

		float ClampPercentEffectValue(const float Value)
		{
			return FMath::Clamp(Value, 0.f, MaxPercentEffectValue);
		}

		bool EffectSpecHasStatusTag(const FGameplayEffectSpec& EffectSpec, const FGameplayTag& StatusTag)
		{
			if (!StatusTag.IsValid())
			{
				return false;
			}

			return EffectSpec.DynamicGrantedTags.HasTag(StatusTag)
				|| EffectSpec.GetDynamicAssetTags().HasTag(StatusTag)
				|| (EffectSpec.Def && EffectSpec.Def->GetGrantedTags().HasTag(StatusTag))
				|| (EffectSpec.Def && EffectSpec.Def->GetAssetTags().HasTag(StatusTag));
		}
	}

	EStatusDamage ClassifyStatusDamage(const FGameplayEffectSpec& EffectSpec)
	{
		if (EffectSpecHasStatusTag(EffectSpec, LabGameplayTags::Status_Burning))
		{
			return EStatusDamage::Burning;
		}

		if (EffectSpecHasStatusTag(EffectSpec, LabGameplayTags::Status_Frostbite))
		{
			return EStatusDamage::Frozen;
		}

		if (EffectSpecHasStatusTag(EffectSpec, LabGameplayTags::Status_ElectricShock))
		{
			return EStatusDamage::ElectricShock;
		}

		return EStatusDamage::None;
	}

	float CalculateCriticalDamage(
		const float BaseDamage,
		const float Critical,
		const float RollPercent,
		bool& bOutCriticalHit)
	{
		bOutCriticalHit = false;
		if (!(BaseDamage > 0.f))
		{
			return 0.f;
		}

		if (RollPercent < FMath::Clamp(Critical, 0.f, 100.f))
		{
			bOutCriticalHit = true;
			return BaseDamage * (2.f + FMath::Max(Critical, 0.f) * 0.01f);
		}
		return BaseDamage;
	}

	float CalculateStatusEffectDamage(
		const float SkillScaledDamage,
		const float DamageBonusPercent,
		const float DamageScale)
	{
		const float DamageMultiplier = 1.0f + (DamageBonusPercent * 0.01f);
		const float ScaledDamage = FMath::Max(SkillScaledDamage, 0.0f)
			* DamageMultiplier
			* FMath::Max(DamageScale, 0.0f);
		return FMath::Max(ScaledDamage, 0.0f);
	}

	float MitigateByStatusResistance(const float Damage, const float ResistancePercent)
	{
		const float DamageMultiplier = 1.f - (ClampPercentEffectValue(ResistancePercent) * 0.01f);
		return FMath::Max(Damage, 0.f) * DamageMultiplier;
	}

	float MitigateByArmor(const float Damage, const float ArmorPercent, const float FinalStrengthDamage)
	{
		const float DamageReduction = FMath::Max(FinalStrengthDamage, 0.f) * ClampPercentEffectValue(ArmorPercent) * 0.01f;
		return FMath::Max(FMath::Max(Damage, 0.f) - DamageReduction, 0.f);
	}

	FShieldAbsorption AbsorbByShield(const float Damage, const float CurrentShield)
	{
		FShieldAbsorption Absorption;
		Absorption.HealthDamage = Damage;
		Absorption.RemainingShield = CurrentShield;
		if (CurrentShield > 0.f)
		{
			Absorption.ShieldDamage = FMath::Min(Damage, CurrentShield);
			Absorption.RemainingShield = FMath::Max(CurrentShield - Damage, 0.f);
			Absorption.HealthDamage = FMath::Max(Damage - Absorption.ShieldDamage, 0.f);
		}
		return Absorption;
	}
}
