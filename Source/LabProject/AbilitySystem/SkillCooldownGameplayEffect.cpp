#include "AbilitySystem/SkillCooldownGameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillCooldownGameplayEffect)

USkillCooldownGameplayEffect::USkillCooldownGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(1.0f);
}
