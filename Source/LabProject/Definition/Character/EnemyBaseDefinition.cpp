#include "Definition/Character/EnemyBaseDefinition.h"

#include "Abilities/GameplayAbility.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnemyBaseDefinition)

UEnemyBaseDefinition::UEnemyBaseDefinition()
{
	Combat.DefaultStatDefinition = TSoftObjectPtr<UStatUpgradeDefinition>(
		FSoftObjectPath(TEXT("/Game/GAS/Attribute/DA_Stat.DA_Stat")));

	static ConstructorHelpers::FClassFinder<UGameplayAbility> AttackAbilityFinder(
		TEXT("/Game/GAS/Ability/GA_Attack"));
	if (AttackAbilityFinder.Succeeded())
	{
		Combat.DefaultCombatAbilities.AddUnique(AttackAbilityFinder.Class);
	}

	static ConstructorHelpers::FClassFinder<UGameplayAbility> PunchAbilityFinder(
		TEXT("/Game/GAS/Ability/GA_Punch"));
	if (PunchAbilityFinder.Succeeded())
	{
		Combat.DefaultPunchAbilityClass = PunchAbilityFinder.Class;
		Combat.DefaultCombatAbilities.AddUnique(PunchAbilityFinder.Class);
	}

	static ConstructorHelpers::FClassFinder<UGameplayAbility> RangedAttackAbilityFinder(
		TEXT("/Game/GAS/Ability/GA_RangedAttack"));
	if (RangedAttackAbilityFinder.Succeeded())
	{
		Combat.DefaultCombatAbilities.AddUnique(RangedAttackAbilityFinder.Class);
	}

	static ConstructorHelpers::FClassFinder<UGameplayAbility> DeathAbilityFinder(
		TEXT("/Game/GAS/Ability/GA_Death"));
	if (DeathAbilityFinder.Succeeded())
	{
		Combat.DefaultCombatAbilities.AddUnique(DeathAbilityFinder.Class);
	}
}

FPrimaryAssetId UEnemyBaseDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("EnemyBaseDefinition"), GetFName());
}

FSoftObjectPath UEnemyBaseDefinition::GetDefaultDefinitionPath()
{
	return FSoftObjectPath(
		TEXT("/Game/Data/DA_EnemyBase.DA_EnemyBase"));
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

	if (Combat.DefaultCombatAbilityLevel < 1)
	{
		Context.AddError(FText::FromString(
			TEXT("Combat.DefaultCombatAbilityLevel must be at least 1.")));
		Result = EDataValidationResult::Invalid;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : Combat.DefaultCombatAbilities)
	{
		if (!AbilityClass)
		{
			Context.AddError(FText::FromString(
				TEXT("Combat.DefaultCombatAbilities cannot contain null entries.")));
			Result = EDataValidationResult::Invalid;
			break;
		}
	}

	return Result;
}
#endif
