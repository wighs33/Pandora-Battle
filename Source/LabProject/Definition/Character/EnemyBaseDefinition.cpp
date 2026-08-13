#include "Definition/Character/EnemyBaseDefinition.h"

#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Definition/Player/StatUpgradeDefinition.h"
#include "GameplayEffect.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnemyBaseDefinition)

FPrimaryAssetId UEnemyBaseDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GetDefaultPrimaryAssetId().PrimaryAssetType, GetFName());
}

FPrimaryAssetId UEnemyBaseDefinition::GetDefaultPrimaryAssetId()
{
	return FPrimaryAssetId(
		TEXT("EnemyBaseDefinition"),
		GetDefaultDefinitionPath().GetAssetFName());
}

FSoftObjectPath UEnemyBaseDefinition::GetDefaultDefinitionPath()
{
	return UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
		.EnemyBase.ToSoftObjectPath();
}

TSoftObjectPtr<UStatUpgradeDefinition>
UEnemyBaseDefinition::GetEffectiveDefaultStatDefinition() const
{
	if (!Combat.DefaultStatDefinition.IsNull())
	{
		return Combat.DefaultStatDefinition;
	}

	return UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
		.StatUpgrade;
}

#if WITH_EDITOR
EDataValidationResult UEnemyBaseDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	const auto RequireFiniteNonNegative =
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

	RequireFiniteNonNegative(Combat.InitialCombatDelay, TEXT("Combat.InitialCombatDelay"));
	RequireFiniteNonNegative(Combat.AttackInterval, TEXT("Combat.AttackInterval"));
	RequireFiniteNonNegative(Combat.AttackStartDistance, TEXT("Combat.AttackStartDistance"));
	RequireFiniteNonNegative(Combat.RangedAttackStartDistance, TEXT("Combat.RangedAttackStartDistance"));
	RequireFiniteNonNegative(TrainingBot.HitStunDuration, TEXT("TrainingBot.HitStunDuration"));
	RequireFiniteNonNegative(TrainingBot.RespawnDelay, TEXT("TrainingBot.RespawnDelay"));
	if (!FMath::IsFinite(MonsterMaxHealth) || MonsterMaxHealth <= 0.0f)
	{
		Context.AddError(FText::FromString(
			TEXT("MonsterMaxHealth must be finite and positive.")));
		Result = EDataValidationResult::Invalid;
	}

	if (Combat.DefaultMonsterClass.IsNull())
	{
		Context.AddError(FText::FromString(
			TEXT("Combat.DefaultMonsterClass is required.")));
		Result = EDataValidationResult::Invalid;
	}
	if (MonsterStateTree.IsNull())
	{
		Context.AddError(FText::FromString(
			TEXT("MonsterStateTree is required for server-side monster AI.")));
		Result = EDataValidationResult::Invalid;
	}

	const bool bHasAnyMonsterPresentationSetting =
		MonsterPresentation.ContactDamageEffectClass
		|| MonsterPresentation.HitReactMontage
		|| MonsterPresentation.DeathMontage;
	if (bHasAnyMonsterPresentationSetting
		&& (!MonsterPresentation.ContactDamageEffectClass
			|| !MonsterPresentation.HitReactMontage
			|| !MonsterPresentation.DeathMontage))
	{
		Context.AddError(FText::FromString(
			TEXT("MonsterPresentation must configure the contact damage effect, hit-react montage, and death montage together.")));
		Result = EDataValidationResult::Invalid;
	}
	if (!FMath::IsFinite(MonsterPresentation.HitReactPlayRate)
		|| MonsterPresentation.HitReactPlayRate <= 0.0f)
	{
		Context.AddError(FText::FromString(
			TEXT("MonsterPresentation.HitReactPlayRate must be finite and positive.")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
