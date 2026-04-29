#include "AbilitySystem/StatUpExecution.h"

#include "AbilitySystem/PdAbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatUpExecution)

/** Spec에 저장된 연산 값을 읽어 실제 Modifier 연산 타입으로 변환합니다. */
EGameplayModOp::Type UStatUpExecution::ResolveOperation(const FGameplayEffectSpec& Spec, const FGameplayTag& OperationSetByCallerTag) const
{
	// =================================================================================================================
	// === 연산 태그 유효성 검사
	
	if (!OperationSetByCallerTag.IsValid())
	{
		return EGameplayModOp::Additive;
	}
	
	// =================================================================================================================
	// === SetByCaller에서 연산 값 조회

	const float* OperationValue = Spec.SetByCallerTagMagnitudes.Find(OperationSetByCallerTag);
	if (!OperationValue)
	{
		return EGameplayModOp::Additive;
	}
	
	// =================================================================================================================
	// === 연산 값에 따라 실제 Modifier 연산 타입 결정

	return FMath::RoundToInt(*OperationValue) == static_cast<int32>(EEnum_Operation::Multiply)
		? EGameplayModOp::Multiplicitive
		: EGameplayModOp::Additive;
}

/** 
 * SetByCaller 기반 스탯 증가 요청을 실제 Attribute 변경값으로 변환합니다. 
 * @param ExecutionParams 실행 주체
 * @param OutExecutionOutput 레퍼런스 반환
 */
void UStatUpExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	// =================================================================================================================
	// === 대상 ASC 확인
	
	const UPdAbilitySystemComponent* TargetASC = Cast<UPdAbilitySystemComponent>(ExecutionParams.GetTargetAbilitySystemComponent());
	if (!TargetASC)
	{
		return;
	}
	
	// =================================================================================================================
	// === 연산 방식 판별용 SetByCaller 태그 조회

	FGameplayTag OperationSetByCallerTag;
	if (!TargetASC->ResolveStatUpOperationSetByCallerTag(OperationSetByCallerTag))
	{
		return;
	}
	
	// =================================================================================================================
	// === 현재 Spec과 연산 방식 해석

	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const EGameplayModOp::Type ModifierOp = ResolveOperation(Spec, OperationSetByCallerTag);
	
	// =================================================================================================================
	// === SetByCaller 태그 목록 순회

	for (const TPair<FGameplayTag, float>& SetByCallerPair : Spec.SetByCallerTagMagnitudes)
	{
		// 연산 방식 자체를 담는 태그는 실제 스탯 처리 대상에서 제외합니다.
		if (SetByCallerPair.Key.MatchesTagExact(OperationSetByCallerTag))
		{
			continue;
		}

		// 유효하지 않은 태그나 의미 없는 수치는 무시합니다.
		if (!SetByCallerPair.Key.IsValid() || FMath::IsNearlyZero(SetByCallerPair.Value))
		{
			continue;
		}
		
		// =============================================================================================================
		// === 스탯 태그를 실제 Attribute로 해석

		FGameplayAttribute Attribute;
		if (!TargetASC->ResolveAttributeFromTag(SetByCallerPair.Key, Attribute))
		{
			continue;
		}
		
		// =============================================================================================================
		// === 계산된 Modifier 출력에 추가

		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(Attribute, ModifierOp, SetByCallerPair.Value));
	}
}
