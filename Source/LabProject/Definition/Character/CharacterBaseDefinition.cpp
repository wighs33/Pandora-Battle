#include "Definition/Character/CharacterBaseDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(CharacterBaseDefinition)

FPrimaryAssetId UCharacterBaseDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("CharacterBaseDefinition"), GetFName());
}

FSoftObjectPath UCharacterBaseDefinition::GetDefaultDefinitionPath()
{
	return FSoftObjectPath(TEXT("/Game/Data/DA_CharacterBase.DA_CharacterBase"));
}

FSoftObjectPath UCharacterBaseDefinition::GetHumanoidDefinitionPath()
{
	return FSoftObjectPath(TEXT("/Game/Data/DA_CharacterHumanoid.DA_CharacterHumanoid"));
}

#if WITH_EDITOR
EDataValidationResult UCharacterBaseDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	auto RequireFiniteNonNegative =
		[&Context, &Result](const float Value, const TCHAR* FieldName)
		{
			if (!FMath::IsFinite(Value) || Value < 0.0f)
			{
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("%s must be finite and non-negative."),
					FieldName)));
				Result = EDataValidationResult::Invalid;
			}
		};

	RequireFiniteNonNegative(AbilityRuntime.ActorInfoRetryInterval, TEXT("AbilityRuntime.ActorInfoRetryInterval"));
	RequireFiniteNonNegative(AbilityRuntime.StaminaRegenDelay, TEXT("AbilityRuntime.StaminaRegenDelay"));
	RequireFiniteNonNegative(AbilityRuntime.MinimumMaxWalkSpeed, TEXT("AbilityRuntime.MinimumMaxWalkSpeed"));
	RequireFiniteNonNegative(HealthBar.WorldScale, TEXT("HealthBar.WorldScale"));
	RequireFiniteNonNegative(HealthBar.VisibilityTargetZOffset, TEXT("HealthBar.VisibilityTargetZOffset"));
	RequireFiniteNonNegative(HealthBar.HideGraceTime, TEXT("HealthBar.HideGraceTime"));
	RequireFiniteNonNegative(HealthBar.ViewModelRetryInterval, TEXT("HealthBar.ViewModelRetryInterval"));
	RequireFiniteNonNegative(Presentation.MaxBodyAuraRelativeOffsetDistance, TEXT("Presentation.MaxBodyAuraRelativeOffsetDistance"));
	RequireFiniteNonNegative(Presentation.MaxBodyAuraRelativeScale, TEXT("Presentation.MaxBodyAuraRelativeScale"));
	RequireFiniteNonNegative(Presentation.TeamOverlayMaterialRetryInterval, TEXT("Presentation.TeamOverlayMaterialRetryInterval"));
	RequireFiniteNonNegative(Death.ImpulseHorizontalStrength, TEXT("Death.ImpulseHorizontalStrength"));
	RequireFiniteNonNegative(Death.ImpulseUpwardStrength, TEXT("Death.ImpulseUpwardStrength"));
	RequireFiniteNonNegative(Death.ImpulseLocationZOffset, TEXT("Death.ImpulseLocationZOffset"));
	RequireFiniteNonNegative(Death.DissolveFallbackDuration, TEXT("Death.DissolveFallbackDuration"));

	if (!FMath::IsFinite(AbilityRuntime.StaminaRegenEffectLevel)
		|| AbilityRuntime.StaminaRegenEffectLevel < 1.0f)
	{
		Context.AddError(FText::FromString(
			TEXT("AbilityRuntime.StaminaRegenEffectLevel must be finite and at least 1.")));
		Result = EDataValidationResult::Invalid;
	}

	if (Death.bUseDissolve && Death.DissolveScalarParameterName.IsNone())
	{
		Context.AddError(FText::FromString(
			TEXT("Death.DissolveScalarParameterName is required when dissolve is enabled.")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
