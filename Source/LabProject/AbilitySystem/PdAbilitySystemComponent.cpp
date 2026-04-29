#include "PdAbilitySystemComponent.h"
#include "AbilitySystem/AttributeDefinition.h"
#include "AbilitySystem/EffectSetByCallerDefinition.h"
#include "GameplayEffect.h"

UPdAbilitySystemComponent::UPdAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
{
	// 복제 모드 설정
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

/** 단일 스탯 태그에 대한 증가 효과를 적용합니다. */
bool UPdAbilitySystemComponent::ApplyStatUpEffectByTag(TSubclassOf<UGameplayEffect> GameplayEffectClass, FGameplayTag StatTag, float Magnitude, EEnum_Operation Operation, float Level)
{
	// =================================================================================================================
	// === 기본 입력값 및 연산 태그 확인
	
	FGameplayTag OperationSetByCallerTag;
	if (!GameplayEffectClass || !StatTag.IsValid() || FMath::IsNearlyZero(Magnitude) || !ResolveStatUpOperationSetByCallerTag(OperationSetByCallerTag))
	{
		return false;
	}
	
	// =================================================================================================================
	// === GameplayEffectSpec 생성

	FGameplayEffectContextHandle EffectContext = MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(GameplayEffectClass, Level, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return false;
	}
	
	// =================================================================================================================
	// === 스탯별 SetByCaller 값 설정

	SpecHandle.Data->SetSetByCallerMagnitude(StatTag, Magnitude);
	SpecHandle.Data->SetSetByCallerMagnitude(OperationSetByCallerTag, static_cast<float>(Operation));
	ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return true;
}

/** 스탯 태그를 실제 GameplayAttribute로 해석합니다. */
bool UPdAbilitySystemComponent::ResolveAttributeFromTag(const FGameplayTag& StatTag, FGameplayAttribute& OutAttribute) const
{
	OutAttribute = FGameplayAttribute();

	// AttributeDefinition에 태그 해석 위임
	return AttributeDefinition && AttributeDefinition->ResolveAttributeFromTag(StatTag, OutAttribute);
}

/** 데미지 크기용 SetByCaller 태그를 반환합니다. */
bool UPdAbilitySystemComponent::ResolveDamageMagnitudeSetByCallerTag(FGameplayTag& OutTag) const
{
	// =================================================================================================================
	// === 초기화 및 안전가드
	
	OutTag = FGameplayTag();

	if (!EffectSetByCallerDefinition)
	{
		return false;
	}
	
	// =================================================================================================================
	// === 데미지 크기 태그 조회 후 반환

	OutTag = EffectSetByCallerDefinition->GetDamageMagnitudeTag();
	return OutTag.IsValid();
}

/** 스탯 증가 연산용 SetByCaller 태그를 반환합니다. */
bool UPdAbilitySystemComponent::ResolveStatUpOperationSetByCallerTag(FGameplayTag& OutTag) const
{
	// =================================================================================================================
	// === 초기화 및 안전가드
	
	OutTag = FGameplayTag();

	if (!EffectSetByCallerDefinition)
	{
		return false;
	}

	// =================================================================================================================
	// === 스탯 증가 연산 태그 조회 후 반환
	
	OutTag = EffectSetByCallerDefinition->GetStatUpOperationTag();
	return OutTag.IsValid();
}
