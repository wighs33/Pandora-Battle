#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectExecutionCalculation.h"
#include "StatUpExecution.generated.h"

/**
 * <스탯 증가 Execution 계산 클래스>
 * - SetByCaller로 전달된 스탯 태그와 수치를 읽어 실제 Attribute 변경으로 변환합니다.
 * - 연산 방식은 별도의 SetByCaller 태그로 전달받아 Add 또는 Multiply로 해석합니다.
 * - 태그 기반 스탯 증가 요청을 공통 Execution 로직으로 처리할 때 사용합니다.
 */
UCLASS()
class LABPROJECT_API UStatUpExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

private:
	EGameplayModOp::Type ResolveOperation(const FGameplayEffectSpec& Spec, const FGameplayTag& OperationSetByCallerTag) const;
};
