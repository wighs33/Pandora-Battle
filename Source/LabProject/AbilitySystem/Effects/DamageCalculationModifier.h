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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Damage")
	FGameplayTag NativeShieldBuffTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Damage", meta = (ClampMin = "0.0"))
	float NativeStrengthDamageScale = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Damage", meta = (ClampMin = "0.0"))
	float NativeArmorMitigationScale = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Debug")
	bool bNativePrintDamageResult = false;
};
