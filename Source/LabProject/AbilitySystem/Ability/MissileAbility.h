#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "MissileAbility.generated.h"

class AActor;
class AGameplayAbilityTargetActor;
struct FMissileSkillConfig;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitTargetData;
class UAnimMontage;
class UGameplayEffect;

UCLASS(Blueprintable)
class LABPROJECT_API UMissileAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UMissileAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	void StartWaitMissileMontageTriggerTask();
	bool StartMissileMontageTask();
	UAnimMontage* GetResolvedMissileMontage() const;
	FGameplayTag GetResolvedMontageTriggerEventTag() const;
	void LaunchMissileFromResolvedTarget();
	bool ResolveMissileTargetLocation(FVector& OutTargetLocation) const;
	bool ResolveForwardGroundTargetLocation(FVector& OutGroundLocation) const;
	void StartTargeting();
	void ConfirmMissileAtLocation(const FVector& TargetLocation);
	void LaunchMissile();
	void StartMissilePresentation();
	void StartMissileDurationTimer();
	void HandleMissileDurationFinished();
	void StartMissileTargetTracking();
	void StopMissileTargetTracking();
	void HandleMissileTargetTrackingTick();
	void RefreshMissileTargets();
	void StartDamageSequence();
	void HandleDamageDelayFinished();
	void HandleDamageTick();
	void ApplyMissileDamageTick(float TickDamageMagnitude);
	void ApplyEffectToHitActor(AActor* HitActor, float TickDamageMagnitude);
	void ConfigureSpawnedTargetActor(AGameplayAbilityTargetActor* SpawnedActor);
	FGameplayAbilityTargetingLocationInfo MakeTargetStartLocation();
	AActor* FindAutoTargetActor() const;
	bool TryGetAutoTargetGroundLocation(FVector& OutGroundLocation) const;
	bool TryGetAttackTargetGroundLocation(FVector& OutGroundLocation) const;
	bool ResolveTargetAimLocation(AActor* TargetActor, FVector& OutAimLocation) const;
	bool IsEligibleMissileTargetActor(const AActor* TargetActor) const;
	bool TryValidateServerMissileActorTarget(AActor* TargetActor, FVector& OutTargetLocation) const;
	bool TryValidateServerMissileTargetData(
		const FGameplayAbilityTargetDataHandle& Data,
		FVector& OutTargetLocation,
		AActor*& OutTargetActor) const;
	bool IsMissileTargetLocationWithinRange(const FVector& TargetLocation) const;
	FVector ClampMissileTargetLocationToRange(const FVector& TargetLocation) const;
	FVector GetMissileTargetingOrigin() const;
	FVector GetMissileTraceStartLocation() const;
	AActor* ResolveTargetDataActor(const FGameplayAbilityTargetDataHandle& Data) const;
	FVector ResolveTargetDataLocation(const FGameplayAbilityTargetDataHandle& Data) const;
	FGameplayEffectSpecHandle MakeDamageEffectSpec(float DamageMagnitude) const;
	const FMissileSkillConfig* GetMissileConfig() const;
	float CalculateMissileDuration() const;
	float CalculateDamageRadius() const;
	float CalculateDamageMagnitudePerTick() const;
	int32 CalculateDamageTickCount() const;
	float CalculateDamageApplicationDuration() const;
	float CalculateDamageInterval() const;
	void DrawDebugTargetingRange() const;
	void MarkDamageSequenceFinished();
	void TryFinishMissileAbilityAfterWork();
	void FinishMissileAbility(bool bWasCancelled);

	UFUNCTION()
	void HandleTargetDataValid(const FGameplayAbilityTargetDataHandle& Data);

	UFUNCTION()
	void HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data);

	UFUNCTION()
	void HandleMissileMontageTriggerEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleMissileMontageFinished();

	UFUNCTION()
	void HandleMissileMontageInterrupted();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MissileMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitMissileMontageTriggerTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitTargetData> WaitTargetDataTask;

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
