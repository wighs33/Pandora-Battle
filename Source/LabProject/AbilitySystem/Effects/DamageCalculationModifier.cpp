#include "AbilitySystem/Effects/DamageCalculationModifier.h"

#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DamageCalculationModifier)

UDamageCalculationModifier::UDamageCalculationModifier(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NativeDamageDataTag = LabGameplayTags::Data_Damage;
}

float UDamageCalculationModifier::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	const float BaseDamage = GetSetByCallerMagnitudeByTag(Spec, NativeDamageDataTag);
	return FMath::Max(BaseDamage, 0.0f);
}
