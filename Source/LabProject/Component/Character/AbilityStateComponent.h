#pragma once

#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "AbilityStateComponent.generated.h"

class ACharacterBase;
class AController;
class UAbilitySystemComponent;
class UActorComponent;
class UCharacterMovementComponent;
class UGameSettingDefinition;
struct FOnAttributeChangeData;

/**
 * 캐릭터와 ASC를 연결하고 GAS 상태를 이동·빙결·사망 처리에 반영한다.
 * 능력 부여 정보는 ASC가, 사망 연출은 사망 컴포넌트가 소유한다.
 */
UCLASS(ClassGroup = (Character), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UAbilityStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAbilityStateComponent();

	// 캐릭터 초기화·빙의 변경·종료 시 연결 관리.
	void CaptureBaseMovementSpeed();
	void InitializeAbilitySystemActorInfo();
	void ClearAbilitySystemActorInfo();

	// 캐릭터 Tick·이동 모드 변경·리스폰에서 호출하는 상태 반영.
	void MaintainFrozenRotationLock();
	void RefreshAirborneGameplayTag();

	bool IsFrozen() const { return bFrozenMovementActive; }
	bool NeedsCharacterTick() const { return bFrozenMovementActive; }
	void ClearFrozenStateForRespawn();
	void ApplyMovementSpeedFromAttribute();
	void RestoreCachedRotationSettings(UCharacterMovementComponent* MovementComponent) const;

private:
	ACharacterBase* GetCharacterOwner() const;
	void TryInitializeAbilitySystemActorInfo();
	void QueueAbilitySystemActorInfoInitializationRetry();

	// 속성 기반 이동속도와 저스태미나 연출.
	void BindMovementSpeedAttributeToASC(UAbilitySystemComponent* AbilitySystemComponent);
	void UnbindMovementSpeedAttribute();

	void HandleMovementAttributesChanged(const FOnAttributeChangeData& Data);
	void RefreshLowStaminaEffectComponent(float StaminaPercent, const UGameSettingDefinition* SettingDefinition);
	UActorComponent* ResolveLowStaminaEffectComponent(FName ComponentName);
	void QueueMovementSpeedAttributeApplyRetry();
	void RetryApplyMovementSpeedFromAttribute();

	// 사망·빙결 태그 구독과 상태 복구.
	void BindDeadTagEvent(UAbilitySystemComponent* AbilitySystemComponent);
	void UnbindDeadTagEvent();
	void BindFrozenTagEvent(UAbilitySystemComponent* AbilitySystemComponent);
	void UnbindFrozenTagEvent();
	void OnDeadTagChanged(FGameplayTag CallbackTag, int32 NewCount);
	void OnFrozenTagChanged(FGameplayTag CallbackTag, int32 NewCount);

	// 서버의 소비 후 지연 스태미나 회복.
	void BindStaminaRegenToASC(UAbilitySystemComponent* AbilitySystemComponent);
	void UnbindStaminaRegenFromASC();
	void HandleStaminaChanged(const FOnAttributeChangeData& Data);
	void ApplyStaminaRegenEffect();
	void RemoveStaminaRegenEffects();
	float GetCurrentMaxStamina() const;

	// PlayerState 연결이 먼저 끊겨도 자신이 연결했던 ASC를 정확히 해제한다.
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	// 중복 예약을 막고 Pawn 교체 시 이전 초기화 재시도를 취소한다.
	UPROPERTY(Transient)
	bool bActorInfoInitializationQueued = false;

	FTimerHandle ActorInfoInitializationRetryTimerHandle;

	UPROPERTY(Transient)
	int32 ActorInfoInitializationRetryCount = 0;

	UPROPERTY(Transient)
	float BaseMaxWalkSpeed = 450.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> MovementAttributesAbilitySystemComponent;

	FDelegateHandle MovementSpeedAttributeChangedDelegateHandle;
	FDelegateHandle MovementStaminaAttributeChangedDelegateHandle;
	FDelegateHandle MovementMaxStaminaAttributeChangedDelegateHandle;
	TWeakObjectPtr<UActorComponent> LowStaminaEffectComponent;
	FName CachedLowStaminaEffectComponentName = NAME_None;
	FTimerHandle MovementSpeedAttributeRetryTimerHandle;

	UPROPERTY(Transient)
	int32 MovementSpeedAttributeRetryAttempts = 0;

	// 구독별 원본 ASC를 보관해 재연결 도중에도 정확한 대상에서 델리게이트를 해제한다.
	TWeakObjectPtr<UAbilitySystemComponent> DeadTagBoundAbilitySystemComponent;
	FDelegateHandle DeadTagChangedDelegateHandle;

	// 빙결 이전 회전 정책과 고정할 각도, 자신이 잠근 조종자를 함께 보관한다.
	UPROPERTY(Transient)
	bool bFrozenMovementActive = false;

	UPROPERTY(Transient)
	bool bFrozenRotationStateCached = false;

	TWeakObjectPtr<AController> FrozenInputController;

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
	TWeakObjectPtr<UAbilitySystemComponent> StaminaRegenAbilitySystemComponent;

	FDelegateHandle StaminaChangedDelegateHandle;
	FTimerHandle StaminaRegenDelayTimerHandle;
};
