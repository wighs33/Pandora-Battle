#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "HitReactAbility.generated.h"

class UAnimMontage;
class UGameplayEffect;
struct FStreamableHandle;

UCLASS(Blueprintable)
class LABPROJECT_API UHitReactAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UHitReactAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void PostLoad() override;

protected:
	// Timing hooks
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void OnAbilityEnding() override;

	// Delegate callbacks
	UFUNCTION()
	void OnHitReactMontageCompleted();

	UFUNCTION()
	void OnHitReactMontageInterrupted();

	UFUNCTION()
	void OnHitReactMontageCancelled();

	// State helpers
	void ClearActiveHitReactEffect();
	void BeginHitReactMontagePreload();
	void HandleHitReactMontagePreloadComplete(uint32 RequestGeneration);
	void ReleaseHitReactMontagePreload();
	void StartHitReactMontage(
		UAnimMontage* Montage,
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Animation")
	TSoftObjectPtr<UAnimMontage> HitReactMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Animation")
	FName HitReactStartSectionName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Effect")
	TSubclassOf<UGameplayEffect> HitReactEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Effect")
	bool bApplyHitReactEffect = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Cue", meta = (Categories = "GameplayCue"))
	FGameplayTag HitReactCueTag;

	TSharedPtr<FStreamableHandle> HitReactMontagePreloadHandle;
	uint32 HitReactMontageRequestGeneration = 0;
};
