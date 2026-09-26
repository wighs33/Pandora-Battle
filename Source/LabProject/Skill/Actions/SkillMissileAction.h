#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Skill/Actions/SkillAction.h"
#include "Definition/AbilitySystem/SkillMissileSettings.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "SkillMissileAction.generated.h"

class AActor;
class AGameplayAbilityTargetActor;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitTargetData;
class UAnimMontage;

/** 표적을 추적하는 미사일 연출과 지연·반복 피해를 관리한다. */
UCLASS(meta = (DisplayName = "Missile"))
class LABPROJECT_API USkillMissileAction : public USkillAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ShowOnlyInnerProperties))
	FMissileSkillConfig Settings;

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnStart() override;
	virtual void OnStop() override;

private:
	void HandleMissileDurationFinished();
	void HandleMissileTargetTrackingTick();
	void HandleDamageDelayFinished();
	void HandleDamageTick();

	UFUNCTION()
	void HandleMissileMontageTriggerEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleMissileMontageFinished();

	UFUNCTION()
	void HandleMissileMontageInterrupted();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void StartWaitMissileMontageTriggerTask();
	bool StartMissileMontageTask();
	UAnimMontage* GetResolvedMissileMontage() const;
	FGameplayTag GetResolvedMontageTriggerEventTag() const;

	void TryLaunchMissile();
	void LaunchMissile();

	bool ResolveTargetAimLocation(AActor* TargetActor, FVector& OutAimLocation) const;
	bool IsEligibleMissileTargetActor(const AActor* TargetActor) const;
	bool IsMissileTargetLocationWithinRange(const FVector& TargetLocation) const;

	FVector GetMissileTargetingOrigin() const;

	void StartMissilePresentation();

	void StartMissileDurationTimer();

	void StartMissileTargetTracking();
	void StopMissileTargetTracking();
	void RefreshMissileTargets();

	void StartDamageSequence();

	void ApplyMissileDamageTick(float TickDamageMagnitude);
	void ApplyEffectToHitActor(AActor* HitActor, float TickDamageMagnitude);

	FGameplayEffectSpecHandle MakeDamageEffectSpec(float DamageMagnitude) const;

	float CalculateDamageRadius() const;
	float CalculateDamageMagnitudePerTick() const;
	int32 CalculateDamageTickCount() const;
	float CalculateDamageApplicationDuration() const;
	float CalculateDamageInterval() const;

	float CalculateMissileDuration() const;

	void MarkDamageSequenceFinished();
	void TryFinishAfterWork();
	void FinishMissile(bool bWasCancelled);

	void DrawDebugTargetingRange() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MissileMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitMissileMontageTriggerTask;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> ActiveMissileTargetActors;

	UPROPERTY(Transient)
	bool bMissileLaunched = false;

	FTimerHandle MissileDurationTimerHandle;
	FTimerHandle MissileTargetTrackingTimerHandle;
	FTimerHandle DamageDelayTimerHandle;
	FTimerHandle DamageTickTimerHandle;

	int32 DamageTicksApplied = 0;
	int32 PlannedDamageTickCount = 0;

	bool bMissileDurationFinished = true;
	bool bDamageSequenceFinished = true;
};