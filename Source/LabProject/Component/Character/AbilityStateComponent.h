#pragma once

#include "Components/ActorComponent.h"
#include "Component/Character/AbilitySystemReadySubscription.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "AbilityStateComponent.generated.h"

class ACharacterBase;
class AController;
class UAbilitySystemComponent;
class UActorComponent;
class UCharacterMovementComponent;
class UEquipmentComponent;
class UGameSettingDefinition;
class UPdAbilitySystemComponent;
struct FOnAttributeChangeData;

/**
 * 캐릭터와 ASC를 연결하고 GAS 상태를 이동·빙결·사망 처리에 반영한다.
 * 능력 부여 정보는 ASC가, 사망 연출은 사망 컴포넌트가 소유한다.
 *
 * ASC 연결이 끝나면 준비 알림을 보낸다. 연결은 캐릭터 초기화·빙의·PlayerState 복제 시점에만 시도하며,
 * 아직 PlayerState가 없으면 다음 시점의 호출이 다시 연결한다. 준비를 기다리는 쪽은 이 알림을 구독한다.
 */
UCLASS(ClassGroup = (Character), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UAbilityStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UAbilityStateComponent();

	// 캐릭터 초기화·빙의 변경·종료 시 연결 관리.
	void CaptureBaseMovementSpeed();
	void InitializeAbilitySystemActorInfo();
	void ClearAbilitySystemActorInfo();

	// ASC 준비 계약. 준비되지 않았으면 nullptr을 반환한다.
	UPdAbilitySystemComponent* GetReadyAbilitySystemComponent() const;
	bool IsAbilitySystemReady() const { return GetReadyAbilitySystemComponent() != nullptr; }
	FDelegateHandle RegisterOnAbilitySystemReady(const FPdAbilitySystemReadyDelegate::FDelegate& Delegate);
	void UnregisterOnAbilitySystemReady(FDelegateHandle Handle);
	/** 이 캐릭터가 ASC의 Avatar에서 빠지기 직전(빙의 해제·Pawn 교체·종료)에 보낸다. 준비 알림을 보낸 ASC에 대해서만 보낸다. */
	FDelegateHandle RegisterOnAbilitySystemReleased(const FPdAbilitySystemReadyDelegate::FDelegate& Delegate);
	void UnregisterOnAbilitySystemReleased(FDelegateHandle Handle);

	// 캐릭터 Tick·이동 모드 변경·리스폰에서 호출하는 상태 반영.
	void MaintainFrozenRotationLock();
	void RefreshAirborneGameplayTag();

	bool IsFrozen() const { return bFrozenMovementActive; }
	bool NeedsCharacterTick() const { return bFrozenMovementActive; }
	void ClearFrozenStateForRespawn();
	void ApplyMovementSpeedFromAttribute();
	void RestoreCachedRotationSettings(UCharacterMovementComponent* MovementComponent) const;

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleMovementAttributesChanged(const FOnAttributeChangeData& Data);
	void OnDeadTagChanged(FGameplayTag CallbackTag, int32 NewCount);
	void OnFrozenTagChanged(FGameplayTag CallbackTag, int32 NewCount);
	void HandleStaminaChanged(const FOnAttributeChangeData& Data);
	void ApplyStaminaRegenEffect();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	ACharacterBase* GetCharacterOwner() const;

	// 속성 기반 이동속도와 저스태미나 연출.
	void BindMovementSpeedAttributeToASC(UAbilitySystemComponent* AbilitySystemComponent);
	void UnbindMovementSpeedAttribute();
	void RefreshLowStaminaEffectComponent(float StaminaPercent, const UGameSettingDefinition* SettingDefinition);
	UActorComponent* ResolveLowStaminaEffectComponent(FName ComponentName);

	// 사망·빙결 태그 구독과 상태 복구.
	void BindDeadTagEvent(UAbilitySystemComponent* AbilitySystemComponent);
	void UnbindDeadTagEvent();
	void BindFrozenTagEvent(UAbilitySystemComponent* AbilitySystemComponent);
	void UnbindFrozenTagEvent();

	// 서버의 소비 후 지연 스태미나 회복.
	void BindStaminaRegenToASC(UAbilitySystemComponent* AbilitySystemComponent);
	void UnbindStaminaRegenFromASC();
	void RemoveStaminaRegenEffects();
	float GetCurrentMaxStamina() const;

private:
	// PlayerState 연결이 먼저 끊겨도 자신이 연결했던 ASC를 정확히 해제한다.
	TWeakObjectPtr<UPdAbilitySystemComponent> BoundAbilitySystemComponent;

	FPdAbilitySystemReadyDelegate OnAbilitySystemReady;
	FPdAbilitySystemReadyDelegate OnAbilitySystemReleased;

	UPROPERTY(Transient)
	float BaseMaxWalkSpeed = 450.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> MovementAttributesAbilitySystemComponent;

	FDelegateHandle MovementSpeedAttributeChangedDelegateHandle;
	FDelegateHandle MovementStaminaAttributeChangedDelegateHandle;
	FDelegateHandle MovementMaxStaminaAttributeChangedDelegateHandle;
	TWeakObjectPtr<UEquipmentComponent> MovementEquipmentComponent;
	FDelegateHandle WeaponDefinitionChangedDelegateHandle;
	TWeakObjectPtr<UActorComponent> LowStaminaEffectComponent;
	FName CachedLowStaminaEffectComponentName = NAME_None;

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
