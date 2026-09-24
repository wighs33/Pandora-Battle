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

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UUnequipAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnAbilityEnding() override;
	virtual void OnAbilityEnded(bool bWasCancelled) override;

	UFUNCTION()
	void OnUnequipMontageCompleted();

	UFUNCTION()
	void OnUnequipMontageInterrupted();

	UFUNCTION()
	void OnUnequipMontageCancelled();

	UFUNCTION()
	void OnUnequipCommitTiming(FGameplayEventData Payload);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ClearActiveUnequipEffect();
	void FinalizeUnequipCommit();
	bool CommitPendingUnequipIfPossible();
	bool ShouldActivateRequestedEquip() const;
	bool ActivateRequestedEquipIfNeeded(bool bShouldActivate);

protected:
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
