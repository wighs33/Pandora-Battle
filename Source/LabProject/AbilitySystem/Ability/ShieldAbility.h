#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "GameplayTagContainer.h"
#include "ShieldAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UGameplayEffect;

UCLASS(Blueprintable)
class LABPROJECT_API UShieldAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UShieldAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Ability|Shield|Animation")
	TObjectPtr<UAnimMontage> ShieldMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Ability|Shield|Effect")
	TSubclassOf<UGameplayEffect> ShieldGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Shield|Event", meta = (Categories = "Event"))
	FGameplayTag MontageTriggerEventTag;

private:
	const FShieldSkillConfig* GetShieldSkillConfig() const;
	UAnimMontage* GetResolvedShieldMontage() const;
	TSubclassOf<UGameplayEffect> GetResolvedShieldGameplayEffectClass() const;
	FGameplayTag GetResolvedMontageTriggerEventTag() const;
	void StartWaitMontageTriggerTask();
	bool StartShieldMontageTask();
	void ApplyShieldFromMontageTrigger();
	void CleanupShieldTasks();

	UFUNCTION()
	void HandleShieldMontageFinished();

	UFUNCTION()
	void HandleMontageTriggerEvent(FGameplayEventData Payload);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ShieldMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitMontageTriggerTask;

	UPROPERTY(Transient)
	bool bShieldApplied = false;
};
