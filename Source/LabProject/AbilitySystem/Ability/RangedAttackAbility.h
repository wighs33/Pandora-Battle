#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "RangedAttackAbility.generated.h"

class UGameplayEffect;

UCLASS(Blueprintable)
class LABPROJECT_API URangedAttackAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	URangedAttackAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// Timing hooks
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// Delegate callbacks
	UFUNCTION()
	void OnAttackMontageCompleted();

	UFUNCTION()
	void OnAttackMontageInterrupted();

	UFUNCTION()
	void OnAttackMontageCancelled();

	// State helpers
	void CleanupAttackState();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Effect")
	TSubclassOf<UGameplayEffect> AttackingEffectClass;
};
