#include "AbilitySystem/PdStatUpExecution.h"

#include "AbilitySystem/PdAttributeSet.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdStatUpExecution)

namespace PdStatUpExecution
{
	const FName OperationSetByCallerName(TEXT("Pd.StatUp.Operation"));

	bool ResolveAttributeFromStatTag(const FGameplayTag& StatTag, FGameplayAttribute& OutAttribute)
	{
		OutAttribute = FGameplayAttribute();

		if (!StatTag.IsValid())
		{
			return false;
		}

		FString TagString = StatTag.ToString();
		FString AttributeName;
		if (!TagString.Split(TEXT("."), nullptr, &AttributeName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			AttributeName = TagString;
		}

		FProperty* AttributeProperty = FindFProperty<FProperty>(UPdAttributeSet::StaticClass(), *AttributeName);
		if (!FGameplayAttribute::IsSupportedProperty(AttributeProperty))
		{
			return false;
		}

		OutAttribute = FGameplayAttribute(AttributeProperty);
		return OutAttribute.IsValid();
	}

	EGameplayModOp::Type ResolveOperation(const FGameplayEffectSpec& Spec)
	{
		const float* OperationValue = Spec.SetByCallerNameMagnitudes.Find(OperationSetByCallerName);
		if (!OperationValue)
		{
			return EGameplayModOp::Additive;
		}

		return FMath::RoundToInt(*OperationValue) == static_cast<int32>(EPdStatChangeOperation::Multiply)
			? EGameplayModOp::Multiplicitive
			: EGameplayModOp::Additive;
	}
}

void UPdStatUpExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const EGameplayModOp::Type ModifierOp = PdStatUpExecution::ResolveOperation(Spec);

	for (const TPair<FGameplayTag, float>& SetByCallerPair : Spec.SetByCallerTagMagnitudes)
	{
		if (!SetByCallerPair.Key.IsValid() || FMath::IsNearlyZero(SetByCallerPair.Value))
		{
			continue;
		}

		FGameplayAttribute Attribute;
		if (!PdStatUpExecution::ResolveAttributeFromStatTag(SetByCallerPair.Key, Attribute))
		{
			continue;
		}

		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(Attribute, ModifierOp, SetByCallerPair.Value));
	}
}
