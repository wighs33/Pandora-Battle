#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "EquipAbility.generated.h"

class UGameplayEffect;
class UItemDefinition;
class UAnimInstance;

UCLASS(Blueprintable)
class LABPROJECT_API UEquipAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) const override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UEquipAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnAbilityEnding() override;

	UFUNCTION()
	void OnEquipMontageCompleted();

	UFUNCTION()
	void OnEquipMontageInterrupted();

	UFUNCTION()
	void OnEquipMontageCancelled();

	UFUNCTION()
	void OnEquipCommitTiming(FGameplayEventData Payload);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ClearActiveEquipEffect();
	void ClearPendingEquipState();
	void ResolveEquipTransition();
	void FinalizeEquipCommit();
	bool CommitPendingEquipIfPossible();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Effect")
	TSubclassOf<UGameplayEffect> EquipEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Cue", meta = (Categories = "GameplayCue"))
	FGameplayTag EquipCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Event", meta = (Categories = "GameplayEvent"))
	FGameplayTag CommitEquipEventTag;

	UPROPERTY(Transient)
	TObjectPtr<const UItemDefinition> ActiveEquipWeaponDefinition;

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> PendingEquipAnimLayer;

	UPROPERTY(Transient)
	bool bEquipCommitted = false;

	UPROPERTY(Transient)
	bool bEquipTransitionResolved = false;

	UPROPERTY(Transient)
	bool bEquipAbilityCommitted = false;

	mutable FGameplayTagContainer EquipCooldownTags;
};
