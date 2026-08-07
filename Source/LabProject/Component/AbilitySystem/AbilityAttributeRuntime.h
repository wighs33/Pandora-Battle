#pragma once

#include "Common/Enum_Operation.h"
#include "CoreMinimal.h"
#include "Definition/AbilitySystem/AbilityAttributeConfig.h"
#include "UObject/Object.h"
#include "AbilityAttributeRuntime.generated.h"

class UGameplayEffect;
class UPdAbilitySystemComponent;

/**
 * Owns attribute mapping and stat-effect behavior for a Pd ability system.
 *
 * The object is instanced by UPdAbilitySystemComponent so its registration
 * order and runtime overrides follow the owning ASC instead of an actor type.
 */
UCLASS()
class LABPROJECT_API UAbilityAttributeRuntime : public UObject
{
	GENERATED_BODY()

public:
	int32 AddAttributeConfig(const FAttributeConfig& AttributeConfig);
	void RemoveAttributeConfig(int32 AttributeConfigHandle);

	bool ApplyAttributeDefaultValue(
		UPdAbilitySystemComponent& AbilitySystemComponent,
		const FGameplayAttribute& Attribute,
		float DefaultValue) const;

	bool ApplyStatUpEffectByTag(
		UPdAbilitySystemComponent& AbilitySystemComponent,
		TSubclassOf<UGameplayEffect> GameplayEffectClass,
		FGameplayTag StatTag,
		float Magnitude,
		EEnum_Operation Operation,
		float Level) const;

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
};
