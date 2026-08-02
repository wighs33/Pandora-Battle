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

public:
	UEquipAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// Timing hooks
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	// Delegate callbacks
	UFUNCTION()
	void OnEquipMontageCompleted();

	UFUNCTION()
	void OnEquipMontageInterrupted();

	UFUNCTION()
	void OnEquipMontageCancelled();

	UFUNCTION()
	void OnEquipCommitTiming(FGameplayEventData Payload);

	// State helpers
	void ClearActiveEquipEffect();
	void ClearPendingEquipState();
	void ResolveEquipTransition();
	void FinalizeEquipCommit();
	bool CommitPendingEquipIfPossible();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Effect")
	TSubclassOf<UGameplayEffect> EquippedItemEffectClass;

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
};
