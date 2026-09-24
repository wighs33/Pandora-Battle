#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "DamageCalculationModifier.generated.h"

UCLASS(Blueprintable)
class LABPROJECT_API UDamageCalculationModifier : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

	// Public API ------------------------------------------------------------------------------------------------------
	UDamageCalculationModifier(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Damage")
	FGameplayTag NativeDamageDataTag;
};
