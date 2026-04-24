#pragma once

#include "CoreMinimal.h"
#include "Ability/PdGameplayAbility.h"
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
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UFUNCTION()
	void HandleUnequipMontageCompleted();

	UFUNCTION()
	void HandleUnequipMontageInterrupted();

	UFUNCTION()
	void HandleUnequipMontageCancelled();

	UFUNCTION()
	void HandleUnequipCommitEvent(FGameplayEventData Payload);

	void ClearActiveUnequipEffect();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Event", meta = (Categories = "GameplayEvent"))
	FGameplayTag CommitUnequipEventTag;

	UPROPERTY(Transient)
	FActiveGameplayEffectHandle ActiveUnequipEffectHandle;

	UPROPERTY(Transient)
	TObjectPtr<const UItemDefinition> ActiveUnequipItemDefinition;
};
