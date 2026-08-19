#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "DamageCalculationModifier.generated.h"

UCLASS(Blueprintable)
class LABPROJECT_API UDamageCalculationModifier : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UDamageCalculationModifier(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Damage")
	FGameplayTag NativeDamageDataTag;
};
