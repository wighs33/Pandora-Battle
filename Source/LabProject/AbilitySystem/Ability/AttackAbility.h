#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "Interface/ComboAttackInterface.h"
#include "AttackAbility.generated.h"

class UGameplayEffect;
class UAbilityTask_WaitInputPress;
class ACharacterBase;
class AWeaponBase;
struct FAttackData;

UCLASS(Blueprintable)
class LABPROJECT_API UAttackAbility : public UPdGameplayAbility, public IComboAttackInterface
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

public:
	// Interface Implementations ---------------------------------------------------------------------------------------
	virtual FName GetNextAttackSectionName() const override;
	virtual bool RequestNextComboInput() override;
	virtual bool TryConsumeLateComboInput() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UAttackAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	bool RequestJumpToSection(FName RequestedSectionName);

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnAbilityEnding() override;

	UFUNCTION()
	void OnAttackMontageCompleted();

	UFUNCTION()
	void OnAttackMontageInterrupted();

	UFUNCTION()
	void OnAttackMontageCancelled();

	UFUNCTION()
	void OnJumpSectionTiming(FGameplayEventData Payload);

	UFUNCTION()
	void OnComboInputWindowOpened(FGameplayEventData Payload);

	UFUNCTION()
	void OnComboInputWindowClosed(FGameplayEventData Payload);

	UFUNCTION()
	void OnAttackDamageWindowOpened(FGameplayEventData Payload);

	UFUNCTION()
	void OnAttackDamageWindowClosed(FGameplayEventData Payload);

	UFUNCTION()
	void OnContinueInputPressed(float TimeWaited);
	void FinalizeAttackDamageWindowClose();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void CleanupAttackState();
	void ResetAttackInputState();
	/** 장착 무기의 공격 데이터, 무기가 없으면 맨손 공격 데이터, 그래도 없으면 적의 기본 공격 데이터를 고른다. */
	static bool ResolveAttackData(const ACharacterBase& Character, FAttackData& OutAttackData);
	/** 새 공격을 시작할 때 콤보 입력 창·피해 창·재시작 예약을 처음 상태로 돌린다. */
	void ResetComboState();
	/** 몽타주가 보내는 입력 창·피해 창 열림·닫힘과 구간 이동 이벤트를 기다린다. 태그가 비어 있는 이벤트는 기다리지 않는다. */
	void ListenForAttackWindowEvents();
	void WaitForContinueInput();
	void RestartAttackAfterMontage();

	AWeaponBase* GetCurrentWeaponActor() const;
	FName GetCurrentAttackSectionName() const;
	bool IsAttackSectionNameValid(FName SectionName) const;
	bool IsAITargetInComboRange() const;

	bool FaceCurrentAttackTarget() const;
	void RequestAIChaseTarget() const;
	void SetCurrentWeaponTraceEnabled(bool bEnabled, FName AttackSectionName = NAME_None) const;
	void ResetAttackDamageHitTracking() const;
	void SetCurrentComboDamageMultiplier(float DamageMultiplier) const;
	float CalculateCurrentComboDamageMultiplier() const;
	void PlayConfiguredComboWindowStartEffect();
	bool QueueBufferedComboTransition();
	bool TryJumpToSection(FName SectionName);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Event",
		meta = (Categories = "GameplayEvent", DisplayName = "Combo Input Window Open Event"))
	FGameplayTag AttackInputWindowStartEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Event",
		meta = (Categories = "GameplayEvent", DisplayName = "Combo Input Window Close Event"))
	FGameplayTag AttackInputWindowEndEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Event",
		meta = (Categories = "GameplayEvent", DisplayName = "Attack Damage Window Open Event"))
	FGameplayTag AttackDamageWindowStartEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Event",
		meta = (Categories = "GameplayEvent", DisplayName = "Attack Damage Window Close Event"))
	FGameplayTag AttackDamageWindowEndEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Event", meta = (Categories = "GameplayEvent"))
	FGameplayTag JumpSectionEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Effect")
	TSubclassOf<UGameplayEffect> AttackingEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|AI")
	bool bAIAlwaysContinueCombo = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|AI")
	bool bAIIgnoreComboRange = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|AI")
	bool bAIFaceTargetWhileAttacking = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Combo",
		meta = (ClampMin = "1.0", UIMin = "1.0", DisplayName = "Damage Multiplier Per Combo Step"))
	float ComboDamageMultiplierPerStep = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Network",
		meta = (ClampMin = "0.0", ClampMax = "0.15", ForceUnits = "s",
			DisplayName = "Attack Damage Window Close Grace"))
	float AttackDamageWindowCloseGraceSeconds = 0.05f;

	UPROPERTY(Transient)
	bool bCanReceiveAttackInput = false;

	UPROPERTY(Transient)
	bool bReachedJumpSectionTiming = false;

	UPROPERTY(Transient)
	FName BufferedJumpSectionName = NAME_None;

	UPROPERTY(Transient)
	bool bBufferedComboCostCommitted = false;

	UPROPERTY(Transient)
	bool bComboInputConsumedForCurrentWindow = false;

	UPROPERTY(Transient)
	FName ComboInputWindowSectionName = NAME_None;

	UPROPERTY(Transient)
	bool bAttackDamageWindowActive = false;

	UPROPERTY(Transient)
	FName ActiveAttackDamageWindowSectionName = NAME_None;

	UPROPERTY(Transient)
	bool bRestartAttackAfterMontage = false;

	UPROPERTY(Transient)
	FName LastComboWindowEffectSectionName = NAME_None;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitInputPress> WaitInputPressTask;

	// 실행 키는 GAS가 관리하고, 자연 종료 직후의 보정 가능 시간만 능력 인스턴스가 기억한다.
	double LastUnconsumedComboWindowCloseTime = -1.0;
	double LateComboInputExpiresAt = -1.0;
	FTimerHandle AttackDamageWindowCloseTimerHandle;
};
