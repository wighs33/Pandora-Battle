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
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void PostLoad() override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UHitReactAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnAbilityEnding() override;

	UFUNCTION()
	void OnHitReactMontageCompleted();

	UFUNCTION()
	void OnHitReactMontageInterrupted();

	UFUNCTION()
	void OnHitReactMontageCancelled();
	void HandleHitReactMontagePreloadComplete(uint32 RequestGeneration);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ClearActiveHitReactEffect();
	void BeginHitReactMontagePreload();
	void ReleaseHitReactMontagePreload();
	void StartHitReactMontage(
		UAnimMontage* Montage,
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo);

protected:
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
