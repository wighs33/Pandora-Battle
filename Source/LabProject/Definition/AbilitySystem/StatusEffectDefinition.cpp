#include "Definition/AbilitySystem/StatusEffectDefinition.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffectTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatusEffectDefinition)

UStatusEffectDefinition::UStatusEffectDefinition()
{
	RemovalPolicyTags.AddTag(LabGameplayTags::Effect_Policy_RemoveOnRespawn);
}

FPrimaryAssetId UStatusEffectDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("StatusEffect"), GetFName());
}

double UStatusEffectDefinition::ResolveDamageMagnitude() const
{
	return static_cast<double>(StatusDamageMagnitude);
}

float UStatusEffectDefinition::GetAttackDamageBonusPercent(
	const UAbilitySystemComponent* SourceAbilitySystemComponent) const
{
	const UBasicAttributeSet* AttributeSet = SourceAbilitySystemComponent
		? SourceAbilitySystemComponent->GetSet<UBasicAttributeSet>()
		: nullptr;
	if (!AttributeSet || !StatusEffectTag.IsValid())
	{
		return 0.0f;
	}

	if (StatusEffectTag.MatchesTag(LabGameplayTags::Status_Burning))
	{
		return FMath::Max(AttributeSet->GetBurn(), 0.0f);
	}

	if (StatusEffectTag.MatchesTag(LabGameplayTags::Status_Frostbite))
	{
		return FMath::Max(AttributeSet->GetFrostbite(), 0.0f);
	}

	if (StatusEffectTag.MatchesTag(LabGameplayTags::Status_ElectricShock))
	{
		return FMath::Max(AttributeSet->GetElectricShock(), 0.0f);
	}

	return 0.0f;
}

float UStatusEffectDefinition::CalculateDamageMagnitude(
	const UAbilitySystemComponent* SourceAbilitySystemComponent,
	const float SkillScaledDamageMagnitude,
	const float DamageScale) const
{
	const float AttackDamageBonusPercent = GetAttackDamageBonusPercent(SourceAbilitySystemComponent);
	const double DamageMultiplier = 1.0 + (static_cast<double>(AttackDamageBonusPercent) * 0.01);
	const double ScaledDamage = static_cast<double>(FMath::Max(SkillScaledDamageMagnitude, 0.0f))
		* DamageMultiplier
		* static_cast<double>(FMath::Max(DamageScale, 0.0f));
	return FMath::Max(static_cast<float>(ScaledDamage), 0.0f);
}

bool UStatusEffectDefinition::SetDamageMagnitude(
	FGameplayEffectSpecHandle& SpecHandle,
	const UAbilitySystemComponent* SourceAbilitySystemComponent,
	const float SkillScaledDamageMagnitude,
	const float DamageScale) const
{
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return false;
	}

	const float DamageMagnitude = CalculateDamageMagnitude(
		SourceAbilitySystemComponent,
		SkillScaledDamageMagnitude,
		DamageScale);
	if (DamageMagnitude <= 0.0f)
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(LabGameplayTags::Data_Damage, DamageMagnitude);
	return true;
}

void UStatusEffectDefinition::AppendRemovalPolicyTags(FGameplayEffectSpecHandle& SpecHandle) const
{
	if (SpecHandle.IsValid() && SpecHandle.Data.IsValid() && !RemovalPolicyTags.IsEmpty())
	{
		SpecHandle.Data->AppendDynamicAssetTags(RemovalPolicyTags);
	}
}
