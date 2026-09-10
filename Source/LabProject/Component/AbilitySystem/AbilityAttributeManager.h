#pragma once

#include "Common/Enum_Operation.h"
#include "CoreMinimal.h"
#include "Definition/AbilitySystem/AbilityAttributeConfig.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "AbilityAttributeManager.generated.h"

class UGameplayEffect;
class UStatUpgradeDefinition;
class UPdAbilitySystemComponent;

/**
 * ASC별 속성 설정과 능력치 효과를 관리한다.
 *
 * 설정의 우선순위와 기본값 적용 기록은 소유 ASC마다 독립적으로 유지한다.
 */
UCLASS()
class LABPROJECT_API UAbilityAttributeManager : public UObject
{
	GENERATED_BODY()

public:
	int32 AddAttributeConfig(const FAttributeConfig& AttributeConfig);
	void RemoveAttributeConfig(int32 AttributeConfigHandle);

	bool HasAppliedConfiguredAttributeDefaults(
		const UAttributeSet* AttributeSet,
		const FSoftObjectPath& DefinitionPath) const;
	void MarkConfiguredAttributeDefaultsApplied(
		UAttributeSet* AttributeSet,
		const FSoftObjectPath& DefinitionPath);
	void ClearConfiguredAttributeDefaultsApplied(
		const UAttributeSet* AttributeSet,
		const FSoftObjectPath& DefinitionPath);

	bool ApplyConfiguredAttributeDefaults(UPdAbilitySystemComponent& AbilitySystemComponent, const UStatUpgradeDefinition& Definition);

	bool ApplyAttributeDefaultValue(
		UPdAbilitySystemComponent& AbilitySystemComponent,
		const FGameplayAttribute& Attribute,
		float DefaultValue) const;

	bool ApplyStatUpEffectByTags(
		UPdAbilitySystemComponent& AbilitySystemComponent,
		TSubclassOf<UGameplayEffect> GameplayEffectClass,
		const TMap<FGameplayTag, float>& StatMagnitudes,
		EEnum_Operation Operation,
		float Level) const;

	bool ResolveAttributeFromTag(
		const FGameplayTag& StatTag,
		FGameplayAttribute& OutAttribute) const;

	bool ResolveDamageMagnitudeSetByCallerTag(
		const UPdAbilitySystemComponent& AbilitySystemComponent,
		FGameplayTag& OutTag) const;

	bool ResolveStatUpOperationSetByCallerTag(
		const UPdAbilitySystemComponent& AbilitySystemComponent,
		FGameplayTag& OutTag) const;

private:
	TMap<int32, FAttributeConfig> ActiveAttributeConfigs;
	TArray<int32> AttributeConfigOrder;
	int32 NextAttributeConfigHandle = 1;

	TWeakObjectPtr<UAttributeSet> ConfiguredDefaultsAttributeSet;
	FSoftObjectPath ConfiguredDefaultsDefinitionPath;
};
