#include "Definition/AbilitySystem/StatusEffectDefinition.h"

#include "Common/LabGameplayTags.h"
#include "GameplayEffectTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatusEffectDefinition)

double UStatusEffectDefinition::ResolveDamageMagnitude() const
{
	return static_cast<double>(StatusDamageMagnitude);
}

bool UStatusEffectDefinition::SetDamageMagnitude(
	FGameplayEffectSpecHandle& SpecHandle,
	const float BaseDamageMagnitude,
	const float DamageScale) const
{
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return false;
	}

	const float DamageMagnitude = FMath::Max(BaseDamageMagnitude, 0.0f)
		* FMath::Max(DamageScale, 0.0f);
	if (DamageMagnitude <= 0.0f)
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(LabGameplayTags::Data_Damage, DamageMagnitude);
	return true;
}
