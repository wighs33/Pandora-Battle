#include "AbilitySystem/Effects/DamageCalculationModifier.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"
#include "GameplayEffectAttributeCaptureDefinition.h"
#include "Kismet/KismetSystemLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DamageCalculationModifier)

namespace
{
	const FGameplayEffectAttributeCaptureDefinition& GetSourceStrengthCaptureDefinition()
	{
		static const FGameplayEffectAttributeCaptureDefinition Definition(
			UBasicAttributeSet::GetStrengthAttribute(),
			EGameplayEffectAttributeCaptureSource::Source,
			false);
		return Definition;
	}

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
	NativeShieldBuffTag = LabGameplayTags::Status_Buff_Shield;

	RelevantAttributesToCapture.Add(GetSourceStrengthCaptureDefinition());
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

	float SourceStrength = 0.0f;
	GetCapturedAttributeMagnitude(GetSourceStrengthCaptureDefinition(), Spec, EvaluationParameters, SourceStrength);
	SourceStrength = FMath::Max(SourceStrength, 0.0f);

	float TargetArmor = 0.0f;
	GetCapturedAttributeMagnitude(GetTargetArmorCaptureDefinition(), Spec, EvaluationParameters, TargetArmor);
	TargetArmor = FMath::Max(TargetArmor, 0.0f);

	const double StrengthMultiplier = 1.0 + (static_cast<double>(NativeStrengthDamageScale) * SourceStrength);
	const double ArmorDivider = 1.0 + (static_cast<double>(NativeArmorMitigationScale) * TargetArmor);
	const float CalculatedDamage = ArmorDivider > UE_DOUBLE_SMALL_NUMBER
		? FMath::Max(static_cast<float>((static_cast<double>(BaseDamage) * StrengthMultiplier) / ArmorDivider), 0.0f)
		: 0.0f;
	const float FinalDamage = bTargetHasShield ? 0.0f : CalculatedDamage;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bNativePrintDamageResult)
	{
		UKismetSystemLibrary::PrintString(
			this,
			FString::Printf(
				TEXT("Damage %.2f -> %.2f Strength=%.2f Armor=%.2f ShieldBuff=%s"),
				BaseDamage,
				FinalDamage,
				SourceStrength,
				TargetArmor,
				bTargetHasShield ? TEXT("true") : TEXT("false")),
			true,
			true,
			bTargetHasShield ? FLinearColor::Blue : FLinearColor::Red,
			2.0f);
	}
#endif

	return FinalDamage;
}
