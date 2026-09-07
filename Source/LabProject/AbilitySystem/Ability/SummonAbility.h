#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "TimerManager.h"
#include "UObject/ObjectKey.h"
#include "SummonAbility.generated.h"

class AActor;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitDelay;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UNiagaraComponent;
class UPrimitiveComponent;
struct FGameplayEffectSpecHandle;
struct FSummonSkillConfig;

UCLASS(Blueprintable)
class LABPROJECT_API USummonAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	USummonAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void OnAbilityEnding() override;

private:
	const FSummonSkillConfig* GetSummonConfig() const;
	UAnimMontage* GetResolvedSummonMontage() const;
	FGameplayTag GetResolvedMontageTriggerEventTag() const;
	void StartWaitSummonMontageTriggerTask();
	bool StartSummonMontageTask();
	void TryCommitAndStartSummon();
	bool SpawnSummonedActor();
	void ConfigureSummonedActorReplication(AActor* SummonedActor, const FSummonSkillConfig& SummonConfig) const;
	void ForceSummonedActorNetUpdate(AActor* SummonedActor, const FSummonSkillConfig& SummonConfig) const;
	FTransform ResolveFinalSummonTransform() const;
	FVector ProjectSummonLocationToGround(const FVector& CandidateLocation) const;
	void DeactivateSummonNiagara(AActor* SummonedActor) const;
	void ActivateSummonNiagara(AActor* SummonedActor) const;
	void FindConfiguredNiagaraComponents(AActor* SummonedActor, TArray<UNiagaraComponent*>& OutComponents) const;
	void StartSummonRise();
	void HandleSummonRiseTick();
	void FinishSummonRiseAndActivateLaser();
	void StartSummonLifetimeTimerOrEnd();
	float ResolveSummonActiveDuration() const;
	float ResolveSummonLifetimeTimerDuration() const;
	void BindSummonTriggerDamage(AActor* SummonedActor);
	void UnbindSummonTriggerDamage();
	UPrimitiveComponent* FindSummonTriggerComponent(AActor* SummonedActor) const;
	void EnableSummonTriggerDamage();
	void DisableSummonTriggerDamage();
	void StartSummonTriggerDamageTickIfNeeded();
	void HandleSummonTriggerDamageTick();
	void ApplySummonTriggerDamageToExistingOverlaps();
	void ApplySummonTriggerDamage(AActor* HitActor, bool bAllowRepeatedDamage = false);
	FGameplayEffectSpecHandle MakeSummonTriggerDamageSpec(float DamageMagnitude) const;
	float CalculateSummonTriggerDamageMagnitude() const;
	void TrackSummonTriggerOverlap(AActor* OtherActor);
	void UntrackSummonTriggerOverlap(AActor* OtherActor);
	void CleanupSummonTasks();

	UFUNCTION()
	void HandleSummonMontageTriggerEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleSummonMontageFinished();

	UFUNCTION()
	void HandleSummonMontageInterrupted();

	UFUNCTION()
	void HandleSummonDurationFinished();

	UFUNCTION()
	void HandleSummonTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleSummonTriggerEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> SummonMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitSummonMontageTriggerTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> SummonDurationTask;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> SpawnedSummonActor;

	UPROPERTY(Transient)
	TWeakObjectPtr<UPrimitiveComponent> SummonTriggerComponent;

	FVector SummonRiseStartLocation = FVector::ZeroVector;
	FVector SummonRiseFinalLocation = FVector::ZeroVector;
	FRotator SummonRiseFinalRotation = FRotator::ZeroRotator;
	float SummonRiseStartTime = 0.0f;
	FTimerHandle SummonRiseTimerHandle;
	FTimerHandle SummonTriggerDamageDelayTimerHandle;
	FTimerHandle SummonTriggerDamageTickTimerHandle;
	TSet<FObjectKey> DamagedSummonTriggerActors;
	TArray<TWeakObjectPtr<AActor>> SummonOverlappingActors;
	bool bSummonStarted = false;
	bool bSummonRiseFinished = false;
	bool bSummonTriggerDamageActive = false;
};
