#include "Definition/Character/CharacterBaseDefinition.h"

#include "Definition/Mode/PdGameInstanceDefinition.h"

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
	return UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
		.Character.ToSoftObjectPath();
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

	RequireFiniteNonNegative(Death.ImpulseHorizontalStrength, TEXT("Death.ImpulseHorizontalStrength"));
	RequireFiniteNonNegative(Death.ImpulseUpwardStrength, TEXT("Death.ImpulseUpwardStrength"));
	RequireFiniteNonNegative(Death.ImpulseLocationZOffset, TEXT("Death.ImpulseLocationZOffset"));
	RequireFiniteNonNegative(Death.DissolveFallbackDuration, TEXT("Death.DissolveFallbackDuration"));

	if (Death.bUseDissolve && Death.DissolveScalarParameterName.IsNone())
	{
		Context.AddError(FText::FromString(
			TEXT("Death.DissolveScalarParameterName is required when dissolve is enabled.")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
