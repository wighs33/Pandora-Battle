#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "AttackAbility.generated.h"

class UGameplayEffect;
class UAbilityTask_WaitInputPress;
class AWeaponBase;

UCLASS(Blueprintable)
class LABPROJECT_API UAttackAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UAttackAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	const FGameplayTag& GetJumpSectionEventTag() const { return JumpSectionEventTag; }
	FName GetNextAttackSectionName() const;
	bool RequestNextComboInput();
	bool TryConsumeLateComboInput();
	bool RequestJumpToSection(FName RequestedSectionName);

protected:
	// Timing hooks
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void OnAbilityEnding() override;

	// Delegate callbacks
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

	// State helpers
	void CleanupAttackState();
	void FinalizeAttackDamageWindowClose();
	void ResetAttackInputState();
	void WaitForContinueInput();
	void RestartAttackAfterMontage();

	// Query helpers
	AWeaponBase* GetCurrentWeaponActor() const;
	FName GetCurrentAttackSectionName() const;
	bool IsAttackSectionNameValid(FName SectionName) const;
	bool IsAITargetInComboRange(const TCHAR* Context) const;

	// Action helpers
	bool FaceCurrentAttackTarget(const TCHAR* Context) const;
	void RequestAIChaseTarget(const TCHAR* Context) const;
	void SetCurrentWeaponBeginOverlapEnabled(bool bEnabled, FName AttackSectionName = NAME_None) const;
	void ResetAttackDamageHitTracking() const;
	void SetCurrentComboDamageMultiplier(float DamageMultiplier) const;
	float CalculateCurrentComboDamageMultiplier() const;
	void PlayConfiguredComboWindowStartEffect();
	bool QueueBufferedComboTransition();
	bool TryJumpToSection(FName SectionName);

	// =================================================================================================================

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

	// =================================================================================================================

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

	// =================================================================================================================

	UPROPERTY(Transient)
	bool bCanReceiveAttackInput = false;

	UPROPERTY(Transient)
	bool bReachedJumpSectionTiming = false;

	UPROPERTY(Transient)
	FName BufferedJumpSectionName = NAME_None;

	UPROPERTY(Transient)
	FName QueuedFromSectionName = NAME_None;

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
