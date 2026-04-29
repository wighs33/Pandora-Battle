#pragma once

#include "CoreMinimal.h"
#include "Common/Enum_Operation.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemComponent.h"
#include "PdAbilitySystemComponent.generated.h"

class UGameplayEffect;
class UAttributeDefinition;
class UEffectSetByCallerDefinition;

/**
 * <프로젝트 전용 AbilitySystemComponent>
 * - AttributeDefinition을 통해 스탯 태그를 실제 Attribute로 해석합니다.
 * - EffectSetByCallerDefinition을 통해 공통 SetByCaller 태그를 해석합니다.
 * - SetByCaller 기반의 GameplayEffect를 사용해 단일 스탯 증가 효과를 적용합니다.
 */
UCLASS()
class LABPROJECT_API UPdAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UPdAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 현재 사용하는 AttributeDefinition 데이터 애셋을 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "!AbilitySystem|Attribute")
	const UAttributeDefinition* GetAttributeDefinition() const { return AttributeDefinition; }

	/** 현재 사용하는 EffectSetByCallerDefinition 데이터 애셋을 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "!AbilitySystem|SetByCaller")
	const UEffectSetByCallerDefinition* GetEffectSetByCallerDefinition() const { return EffectSetByCallerDefinition; }

	/** EffectSetByCallerDefinition의 Operation SetByCaller 태그를 사용해 단일 스탯 증가 효과를 적용합니다. */
	bool ApplyStatUpEffectByTag(TSubclassOf<UGameplayEffect> GameplayEffectClass, FGameplayTag StatTag, float Magnitude,
		EEnum_Operation Operation = EEnum_Operation::Add, float Level = 1.f);
	
	/** 스탯 태그를 실제 Attribute로 해석합니다. */
	bool ResolveAttributeFromTag(const FGameplayTag& StatTag, FGameplayAttribute& OutAttribute) const;

	/** EffectSetByCallerDefinition에서 데미지 수치 전달용 태그를 해석합니다. */
	bool ResolveDamageMagnitudeSetByCallerTag(FGameplayTag& OutTag) const;

	/** EffectSetByCallerDefinition에서 StatUp Operation 전달용 태그를 해석합니다. */
	bool ResolveStatUpOperationSetByCallerTag(FGameplayTag& OutTag) const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AbilitySystem|Attribute")
	TObjectPtr<UAttributeDefinition> AttributeDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AbilitySystem|SetByCaller")
	TObjectPtr<UEffectSetByCallerDefinition> EffectSetByCallerDefinition;
};
