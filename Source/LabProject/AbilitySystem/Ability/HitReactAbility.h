#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "HitReactAbility.generated.h"

class UAnimMontage;
class UGameplayEffect;

UCLASS(Blueprintable)
class LABPROJECT_API UHitReactAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UHitReactAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// Timing hooks
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// Delegate callbacks
	UFUNCTION()
	void OnHitReactMontageCompleted();

	UFUNCTION()
	void OnHitReactMontageInterrupted();

	UFUNCTION()
	void OnHitReactMontageCancelled();

	// State helpers
	void ClearActiveHitReactEffect();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Animation")
	TSoftObjectPtr<UAnimMontage> HitReactMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Animation")
	FName HitReactStartSectionName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Effect")
	TSubclassOf<UGameplayEffect> HitReactEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Cue", meta = (Categories = "GameplayCue"))
	FGameplayTag HitReactCueTag;
};
