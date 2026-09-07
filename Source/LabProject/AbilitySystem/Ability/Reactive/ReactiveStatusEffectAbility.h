#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "ReactiveStatusEffectAbility.generated.h"

class UAbilityTask_WaitGameplayEffectApplied_Target;
class UAbilitySystemComponent;
class UStatusEffectDefinition;

UCLASS(Abstract, Blueprintable)
class LABPROJECT_API UReactiveStatusEffectAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UReactiveStatusEffectAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void OnAbilityEnding() override;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "!StatusEffect")
	FGameplayEffectSpecHandle ModifyEffectSpecBeforeApplication(FGameplayEffectSpecHandle SpecHandle);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect")
	TObjectPtr<UStatusEffectDefinition> StatusEffectDataAsset;

private:
	UFUNCTION()
	void OnGameplayEffectAppliedToTarget(AActor* TargetActor, FGameplayEffectSpecHandle SpecHandle, FActiveGameplayEffectHandle ActiveHandle);

	void ApplyDefaultSetByCallerMagnitudes(FGameplayEffectSpecHandle& SpecHandle) const;
	int32 GetDebuffStackCount(AActor* TargetActor, FActiveGameplayEffectHandle ActiveHandle) const;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEffectApplied_Target> WaitGameplayEffectAppliedTask;
};
