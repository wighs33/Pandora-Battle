#include "AbilitySystem/StatUpExecution.h"

#include "Component/AbilitySystem/PdAbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatUpExecution)

EGameplayModOp::Type UStatUpExecution::ResolveOperation(const FGameplayEffectSpec& Spec, const FGameplayTag& OperationSetByCallerTag) const
{
	// =================================================================================================================
	if (!OperationSetByCallerTag.IsValid())
	{

		return EGameplayModOp::Additive;
	}

	// =================================================================================================================

	const float* OperationValue = Spec.SetByCallerTagMagnitudes.Find(OperationSetByCallerTag);
	if (!OperationValue)
	{

		return EGameplayModOp::Additive;
	}

	// =================================================================================================================

	return FMath::RoundToInt(*OperationValue) == static_cast<int32>(EEnum_Operation::Multiply)
		? EGameplayModOp::Multiplicitive
		: EGameplayModOp::Additive;
}

void UStatUpExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	// =================================================================================================================

	const UPdAbilitySystemComponent* TargetASC = Cast<UPdAbilitySystemComponent>(ExecutionParams.GetTargetAbilitySystemComponent());
	if (!TargetASC)
	{

		return;
	}

// =================================================================================================================

	FGameplayTag OperationSetByCallerTag;
	if (!TargetASC->ResolveStatUpOperationSetByCallerTag(OperationSetByCallerTag))
	{

		return;
	}

	// =================================================================================================================

	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const EGameplayModOp::Type ModifierOp = ResolveOperation(Spec, OperationSetByCallerTag);

// =================================================================================================================

	for (const TPair<FGameplayTag, float>& SetByCallerPair : Spec.SetByCallerTagMagnitudes)
	{
		if (SetByCallerPair.Key.MatchesTagExact(OperationSetByCallerTag))
		{

			continue;
		}

		if (!SetByCallerPair.Key.IsValid() || FMath::IsNearlyZero(SetByCallerPair.Value))
		{

			continue;
		}

		// =============================================================================================================

		FGameplayAttribute Attribute;
		if (!TargetASC->ResolveAttributeFromTag(SetByCallerPair.Key, Attribute))
		{

			continue;
		}

		// =============================================================================================================

		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(Attribute, ModifierOp, SetByCallerPair.Value));

	}
}
