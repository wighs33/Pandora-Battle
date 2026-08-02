#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectExecutionCalculation.h"
#include "StatUpExecution.generated.h"

UCLASS()
class LABPROJECT_API UStatUpExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

private:
	EGameplayModOp::Type ResolveOperation(const FGameplayEffectSpec& Spec, const FGameplayTag& OperationSetByCallerTag) const;
};
