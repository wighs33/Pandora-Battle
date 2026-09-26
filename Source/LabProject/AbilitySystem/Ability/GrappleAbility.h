#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "GrappleAbility.generated.h"

class AGrappleTargetActor;
class UAbilityTask_WaitInputRelease;
class UAbilityTask_WaitTargetData;
class UCharacterActionDefinition;
class UGrappleComponent;
struct FStreamableHandle;

/**
 * 그래플 입력을 누르는 동안 조준하고, 놓으면 GAS 대상 데이터를 제출한다.
 * 그래플 이동은 서버 권한으로 다시 수행한 트레이스가 승인한 경우에만 시작한다.
 */
UCLASS()
class LABPROJECT_API UGrappleAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
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

	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UGrappleAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual FGameplayTag GetDefaultInputTag() const override;
	virtual bool UsesInputRelease(const FGameplayAbilitySpec& Spec) const override { return true; }

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnAbilityEnding() override;

private:
	void HandleCharacterActionDefinitionPreloadComplete();
	void HandleGrappleFinished();

	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

	UFUNCTION()
	void HandleTargetDataValid(const FGameplayAbilityTargetDataHandle& Data);

	UFUNCTION()
	void HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void StartTargetDataTask();
	void StartInputReleaseTask();
	void SetLocalAimPresentation(bool bEnabled) const;
	UCharacterActionDefinition* LoadCharacterActionDefinition() const;
	void ReleaseCharacterActionDefinitionPreload();
	double GetConfiguredCooldownDuration() const;
	UGrappleComponent* GetGrappleComponent() const;

private:
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitTargetData> WaitTargetDataTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputRelease> WaitInputReleaseTask;

	UPROPERTY()
	TObjectPtr<AGrappleTargetActor> SpawnedTargetActor;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterActionDefinition> LoadedCharacterActionDefinition;

	TSharedPtr<FStreamableHandle> CharacterActionDefinitionPreloadHandle;

	mutable FGameplayTagContainer GrappleCooldownTags;
	FDelegateHandle GrappleFinishedDelegateHandle;
	bool bCommittedGrapple = false;
	bool bStartedGrapple = false;
};
