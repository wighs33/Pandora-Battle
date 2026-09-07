#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "GrappleAbility.generated.h"

class ATargetActor_GrappleTrace;
class UAbilityTask_WaitInputRelease;
class UAbilityTask_WaitTargetData;
class UCharacterActionDefinition;
class UGrappleComponent;
struct FStreamableHandle;

/**
 * Hold the grapple input to aim, then release it to submit GAS target data.
 * Only the server-authoritative retrace is allowed to start grapple movement.
 */
UCLASS()
class LABPROJECT_API UGrappleAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UGrappleAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual FGameplayTag GetDefaultInputTag() const override;

protected:
	virtual void OnAvatarSet(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilitySpec& Spec) override;
	virtual void OnRemoveAbility(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void OnAbilityEnding() override;

	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

private:
	void StartTargetDataTask();
	void StartInputReleaseTask();
	void SetLocalAimPresentation(bool bEnabled) const;
	UCharacterActionDefinition* LoadCharacterActionDefinition() const;
	void HandleCharacterActionDefinitionPreloadComplete();
	void ReleaseCharacterActionDefinitionPreload();
	double GetConfiguredCooldownDuration() const;
	UGrappleComponent* GetGrappleComponent() const;
	void HandleGrappleFinished();

	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

	UFUNCTION()
	void HandleTargetDataValid(const FGameplayAbilityTargetDataHandle& Data);

	UFUNCTION()
	void HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data);

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitTargetData> WaitTargetDataTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputRelease> WaitInputReleaseTask;

	UPROPERTY()
	TObjectPtr<ATargetActor_GrappleTrace> SpawnedTargetActor;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterActionDefinition> LoadedCharacterActionDefinition;

	TSharedPtr<FStreamableHandle> CharacterActionDefinitionPreloadHandle;

	mutable FGameplayTagContainer GrappleCooldownTags;
	FDelegateHandle GrappleFinishedDelegateHandle;
	bool bCommittedGrapple = false;
	bool bStartedGrapple = false;
};
