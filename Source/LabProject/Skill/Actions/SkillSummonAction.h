#pragma once

#include "CoreMinimal.h"
#include "Skill/Actions/SkillAction.h"
#include "AbilitySystem/Ability/SkillAbility.h"
#include "Definition/AbilitySystem/SkillSummonSettings.h"
#include "TimerManager.h"
#include "SkillSummonAction.generated.h"

class AActor;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitDelay;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UNiagaraComponent;
class USkillTriggerDamage;
struct FGameplayEffectSpecHandle;
struct FSkillSummonSettings;

/**
 * 소환물의 등장, 상승, 피해 활성화와 수명을 관리한다.
 * 소환물 트리거에 겹친 대상의 피해 시점은 USkillTriggerDamage가 정하고, 상승을 마친 뒤에 켠다.
 */
UCLASS(meta = (DisplayName = "Summon"))
class LABPROJECT_API USkillSummonAction : public USkillAction
{
	GENERATED_BODY()

public:
	/** 소환물의 등장, 상승, 피해 활성화와 수명을 관리한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ShowOnlyInnerProperties))
	FSkillSummonSettings Settings;

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnStart() override;

	virtual void OnStop() override;

private:
	void HandleSummonRiseTick();
	void EnableSummonTriggerDamage();
	bool ApplySummonTriggerDamage(AActor* DamageSourceActor, AActor* HitActor);

	UFUNCTION()
	void HandleSummonMontageTriggerEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleSummonMontageFinished();

	UFUNCTION()
	void HandleSummonMontageInterrupted();

	UFUNCTION()
	void HandleSummonDurationFinished();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	const FSkillSummonSettings* GetSummonConfig() const;
	UAnimMontage* GetResolvedSummonMontage() const;
	FGameplayTag GetResolvedMontageTriggerEventTag() const;
	void StartWaitSummonMontageTriggerTask();
	bool StartSummonMontageTask();
	void TryCommitAndStartSummon();
	bool SpawnSummonedActor();
	void ConfigureSummonedActorReplication(AActor* SummonedActor, const FSkillSummonSettings& SummonConfig) const;
	void ForceSummonedActorNetUpdate(AActor* SummonedActor, const FSkillSummonSettings& SummonConfig) const;
	FTransform ResolveFinalSummonTransform() const;
	FVector ProjectSummonLocationToGround(const FVector& CandidateLocation) const;
	void DeactivateSummonNiagara(AActor* SummonedActor) const;
	void ActivateSummonNiagara(AActor* SummonedActor) const;
	void FindConfiguredNiagaraComponents(AActor* SummonedActor, TArray<UNiagaraComponent*>& OutComponents) const;
	void StartSummonRise();
	void FinishSummonRiseAndActivateLaser();
	void StartSummonLifetimeTimerOrEnd();
	float ResolveSummonActiveDuration() const;
	float ResolveSummonLifetimeTimerDuration() const;
	void BindSummonTriggerDamage(AActor* SummonedActor);
	FGameplayEffectSpecHandle MakeSummonTriggerDamageSpec(float DamageMagnitude) const;
	float CalculateSummonTriggerDamageMagnitude() const;
	void CleanupSummonTasks();

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> SummonMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitSummonMontageTriggerTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> SummonDurationTask;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> SpawnedSummonActor;

	UPROPERTY(Transient)
	TObjectPtr<USkillTriggerDamage> SummonTriggerDamage;

	FVector SummonRiseStartLocation = FVector::ZeroVector;
	FVector SummonRiseFinalLocation = FVector::ZeroVector;
	FRotator SummonRiseFinalRotation = FRotator::ZeroRotator;
	float SummonRiseStartTime = 0.0f;
	FTimerHandle SummonRiseTimerHandle;
	FTimerHandle SummonTriggerDamageDelayTimerHandle;
	bool bSummonStarted = false;
	bool bSummonRiseFinished = false;
};
