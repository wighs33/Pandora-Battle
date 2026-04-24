#pragma once

#include "CoreMinimal.h"
#include "Ability/PdGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "AttackAbility.generated.h"

UCLASS(Blueprintable)
class LABPROJECT_API UAttackAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UAttackAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	const FGameplayTag& GetJumpSectionEventTag() const { return JumpSectionEventTag; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UFUNCTION()
	void HandleAttackMontageCompleted();

	UFUNCTION()
	void HandleAttackMontageInterrupted();

	UFUNCTION()
	void HandleAttackMontageCancelled();

	UFUNCTION()
	void HandleJumpSectionEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleAttackInputWindowStartedEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleAttackInputWindowEndedEvent(FGameplayEventData Payload);

	void ClearActiveAttackEffect();
	void ResetAttackInputState();
	FName GetCurrentAttackSectionName() const;
	FName GetNextAttackSectionName() const;
	bool TryJumpToNextSection();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Event", meta = (Categories = "GameplayEvent"))
	FGameplayTag AttackInputWindowStartEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Event", meta = (Categories = "GameplayEvent"))
	FGameplayTag AttackInputWindowEndEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Event", meta = (Categories = "GameplayEvent"))
	FGameplayTag JumpSectionEventTag;

	UPROPERTY(Transient)
	FActiveGameplayEffectHandle ActiveAttackEffectHandle;

	UPROPERTY(Transient)
	bool bCanReceiveAttackInput = false;

	UPROPERTY(Transient)
	bool bReachedJumpSectionTiming = false;

	UPROPERTY(Transient)
	bool bBufferedJumpSectionRequest = false;
};
