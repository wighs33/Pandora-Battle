#pragma once

#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "CharacterAbilityRuntimeComponent.generated.h"

class ACharacterBase;
class UAbilitySystemComponent;
class UActorComponent;
class UCharacterMovementComponent;
class UGameSettingDefinition;
struct FOnAttributeChangeData;

/**
 * Connects a character avatar to its ASC and owns ability-driven movement
 * state: death-tag routing, frostbite lock, airborne tags, attribute speed,
 * low-stamina presentation, and the legacy stamina-regeneration policy.
 */
UCLASS(ClassGroup = (Character), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UCharacterAbilityRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCharacterAbilityRuntimeComponent();

	void CaptureBaseMovementSpeed();
	void InitializeAbilitySystemActorInfo();
	void ClearAbilitySystemActorInfo();
	void ShutdownRuntime();
	void HandleMovementModeChanged();
	void TickRuntime();

	bool IsFrozen() const { return bFrozenMovementActive; }
	bool NeedsCharacterTick() const { return bFrozenMovementActive; }
	void ClearFrozenStateForRespawn();
	void ApplyMovementSpeedFromAttribute();
	void RestoreCachedRotationSettings(
		UCharacterMovementComponent* MovementComponent) const;

private:
	ACharacterBase* GetCharacterOwner() const;
	const ACharacterBase* GetCharacterOwnerConst() const;
	void TryInitializeAbilitySystemActorInfo();
	void QueueAbilitySystemActorInfoInitializationRetry();
	void BindStaminaRegenToASC(UAbilitySystemComponent* AbilitySystemComponent);
	void UnbindStaminaRegenFromASC();
	void BindMovementSpeedAttributeToASC(UAbilitySystemComponent* AbilitySystemComponent);
	void UnbindMovementSpeedAttribute();
	void BindDeadTagEvent(UAbilitySystemComponent* AbilitySystemComponent);
	void UnbindDeadTagEvent();
	void BindFrozenTagEvent(UAbilitySystemComponent* AbilitySystemComponent);
	void UnbindFrozenTagEvent();
	void RefreshAirborneGameplayTag();
	void SetAirborneGameplayTag(bool bAirborne);
	void MaintainFrozenRotationLock();

	void HandleMovementSpeedAttributeChanged(const FOnAttributeChangeData& Data);
	void HandleMovementStaminaAttributeChanged(const FOnAttributeChangeData& Data);
	void RefreshLowStaminaEffectComponent(
		float StaminaPercent,
		const UGameSettingDefinition* SettingDefinition);
	UActorComponent* ResolveLowStaminaEffectComponent(FName ComponentName);
	void QueueMovementSpeedAttributeApplyRetry();
	void RetryApplyMovementSpeedFromAttribute();

	void OnDeadTagChanged(FGameplayTag CallbackTag, int32 NewCount);
	void OnFrozenTagChanged(FGameplayTag CallbackTag, int32 NewCount);
	void HandleStaminaChanged(const FOnAttributeChangeData& Data);
	void ApplyStaminaRegenEffect();
	void RemoveStaminaRegenEffects();
	float GetCurrentMaxStamina() const;

	UPROPERTY(Transient)
	bool bActorInfoInitializationQueued = false;

	FTimerHandle ActorInfoInitializationRetryTimerHandle;

	UPROPERTY(Transient)
	int32 ActorInfoInitializationRetryCount = 0;

	UPROPERTY(Transient)
	float BaseMaxWalkSpeed = 450.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> MovementSpeedAttributeASC;

	FDelegateHandle MovementSpeedAttributeChangedDelegateHandle;
	FDelegateHandle MovementStaminaAttributeChangedDelegateHandle;
	FDelegateHandle MovementMaxStaminaAttributeChangedDelegateHandle;
	TWeakObjectPtr<UActorComponent> LowStaminaEffectComponent;
	FName CachedLowStaminaEffectComponentName = NAME_None;
	FTimerHandle MovementSpeedAttributeRetryTimerHandle;

	UPROPERTY(Transient)
	int32 MovementSpeedAttributeRetryAttempts = 0;

	TWeakObjectPtr<UAbilitySystemComponent> DeadTagBoundAbilitySystemComponent;
	FDelegateHandle DeadTagChangedDelegateHandle;

	UPROPERTY(Transient)
	bool bFrozenMovementActive = false;

	UPROPERTY(Transient)
	bool bFrozenRotationStateCached = false;

	UPROPERTY(Transient)
	bool bFrozenAppliedIgnoreLookInput = false;

	UPROPERTY(Transient)
	bool bFrozenCachedOrientRotationToMovement = true;

	UPROPERTY(Transient)
	bool bFrozenCachedUseControllerDesiredRotation = false;

	UPROPERTY(Transient)
	bool bFrozenCachedUseControllerRotationYaw = false;

	UPROPERTY(Transient)
	FRotator FrozenCachedRotationRate = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	FRotator FrozenLockedActorRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	FRotator FrozenLockedControlRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	FRotator FrozenLockedMeshRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	float FrozenLockedAimYaw = 0.0f;

	UPROPERTY(Transient)
	float FrozenLockedAimPitch = 0.0f;

	TWeakObjectPtr<UAbilitySystemComponent> FrozenTagBoundAbilitySystemComponent;
	FDelegateHandle FrozenTagChangedDelegateHandle;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> StaminaRegenASC;

	FDelegateHandle StaminaChangedDelegateHandle;
	FTimerHandle StaminaRegenDelayTimerHandle;
};
