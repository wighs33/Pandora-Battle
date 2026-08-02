#include "AbilitySystem/Effects/DamageCalculationModifier.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"
#include "GameplayEffectAttributeCaptureDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DamageCalculationModifier)

namespace
{
	const FGameplayEffectAttributeCaptureDefinition& GetTargetArmorCaptureDefinition()
	{
		static const FGameplayEffectAttributeCaptureDefinition Definition(
			UBasicAttributeSet::GetArmorAttribute(),
			EGameplayEffectAttributeCaptureSource::Target,
			false);
		return Definition;
	}
}

UDamageCalculationModifier::UDamageCalculationModifier(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NativeDamageDataTag = LabGameplayTags::Data_Damage;
	NativeShieldBuffTag = LabGameplayTags::Status_Defense_Shield;

	RelevantAttributesToCapture.Add(GetTargetArmorCaptureDefinition());
}

float UDamageCalculationModifier::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	const FGameplayTagContainer& TargetTags = GetTargetActorTags(Spec);
	const float BaseDamage = GetSetByCallerMagnitudeByTag(Spec, NativeDamageDataTag);
	const bool bTargetHasShield = NativeShieldBuffTag.IsValid() && TargetTags.HasTagExact(NativeShieldBuffTag);

	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvaluationParameters.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	float TargetArmor = 0.0f;
	GetCapturedAttributeMagnitude(GetTargetArmorCaptureDefinition(), Spec, EvaluationParameters, TargetArmor);
	TargetArmor = FMath::Max(TargetArmor, 0.0f);

	const float CalculatedDamage = FMath::Max(BaseDamage, 0.0f);
	const float FinalDamage = bTargetHasShield ? 0.0f : CalculatedDamage;

	return FinalDamage;
}
