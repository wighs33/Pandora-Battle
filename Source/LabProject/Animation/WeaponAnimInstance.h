#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "WeaponAnimInstance.generated.h"

class UAnimMontage;

UCLASS(Blueprintable)
class LABPROJECT_API UWeaponAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// Anim notify callbacks
	UFUNCTION()
	void AnimNotify_HoldBow();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Weapon|Animation")
	TObjectPtr<UAnimMontage> HoldBowMontage = nullptr;
};
