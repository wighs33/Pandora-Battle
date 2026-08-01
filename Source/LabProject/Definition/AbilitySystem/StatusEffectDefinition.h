#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Data/PdStatusEffectDataAsset.h"
#include "StatusEffectDefinition.generated.h"

struct FGameplayEffectSpecHandle;

/**
 * Elemental status definition that keeps legacy assets compatible while
 * centralizing SetByCaller damage assignment.
 */
UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UStatusEffectDefinition : public UPdStatusEffectDataAsset
{
	GENERATED_BODY()

public:
	double ResolveDamageMagnitude() const;
	bool SetDamageMagnitude(
		FGameplayEffectSpecHandle& SpecHandle,
		float BaseDamageMagnitude,
		float DamageScale = 1.0f) const;
};
