#include "AbilitySystem/EquipmentStatGameplayEffect.h"

#include "AbilitySystem/StatUpExecution.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipmentStatGameplayEffect)

UEquipmentStatGameplayEffect::UEquipmentStatGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition ExecutionDefinition;
	ExecutionDefinition.CalculationClass = UStatUpExecution::StaticClass();
	Executions.Add(ExecutionDefinition);
}
