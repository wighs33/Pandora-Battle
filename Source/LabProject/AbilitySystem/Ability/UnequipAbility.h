#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "UnequipAbility.generated.h"

class UGameplayEffect;
class UItemDefinition;

UCLASS(Blueprintable)
class LABPROJECT_API UUnequipAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UUnequipAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// Timing hooks
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	// Delegate callbacks
	UFUNCTION()
	void OnUnequipMontageCompleted();

	UFUNCTION()
	void OnUnequipMontageInterrupted();

	UFUNCTION()
	void OnUnequipMontageCancelled();

	UFUNCTION()
	void OnUnequipCommitTiming(FGameplayEventData Payload);

	// State helpers
	void ClearActiveUnequipEffect();
	void FinalizeUnequipCommit();
	bool CommitPendingUnequipIfPossible();
	bool ShouldActivateRequestedEquip() const;
	bool ActivateRequestedEquipIfNeeded(bool bShouldActivate);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Effect")
	TSubclassOf<UGameplayEffect> UnequipEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Event", meta = (Categories = "GameplayEvent"))
	FGameplayTag CommitUnequipEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Activation", meta = (Categories = "Action"))
	FGameplayTag PostUnequipEquipAbilityTag;

	UPROPERTY(Transient)
	TObjectPtr<const UItemDefinition> ActiveUnequipWeaponDefinition;

	UPROPERTY(Transient)
	bool bUnequipCommitted = false;

	UPROPERTY(Transient)
	bool bUnequipTransitionResolved = false;
};
