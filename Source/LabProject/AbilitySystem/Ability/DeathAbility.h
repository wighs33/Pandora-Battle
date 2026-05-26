#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "DeathAbility.generated.h"

class UGameplayEffect;

UCLASS(Blueprintable)
class LABPROJECT_API UDeathAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UDeathAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Death")
	TSubclassOf<UGameplayEffect> DeathEffectClass;
};
