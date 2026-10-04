#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "Skill/Actions/SkillAction.h"
#include "AbilitySystem/Ability/SkillAbility.h"
#include "Definition/AbilitySystem/SkillActorFieldSettings.h"
#include "TimerManager.h"
#include "SkillActorFieldAction.generated.h"

class AActor;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class USkillTriggerDamage;
struct FGameplayEffectSpecHandle;

/**
 * 배치 액터의 생성 순서와 반복 생성, 수명을 관리한다.
 * 배치 위치는 PdSkillFieldPlacement가, 트리거에 겹친 대상의 피해 시점은 USkillTriggerDamage가 정한다.
 */
UCLASS(meta = (DisplayName = "Actor Field"))
class LABPROJECT_API USkillActorFieldAction : public USkillAction
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	USkillActorFieldAction();

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnStart() override;
	virtual void OnStop() override;

private:
	UFUNCTION()
	void HandleFieldMontageTriggerEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleFieldMontageFinished();

	UFUNCTION()
	void HandleFieldMontageInterrupted();
	void SpawnNextFieldActor();

	UFUNCTION()
	void HandleRepeatedFieldSpawnSequence();

	UFUNCTION()
	void HandleFieldDurationFinished();

	bool ApplyFieldTriggerDamage(AActor* DamageSourceActor, AActor* HitActor);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	UAnimMontage* GetResolvedFieldMontage() const;
	FGameplayTag GetResolvedFieldTriggerEventTag() const;
	bool StartFieldMontageTask();
	void StartWaitFieldMontageTriggerTask();
	void TryCommitAndStartField();

	void StartFieldDurationMovementLockIfAllowed();
	bool ShouldSkipFieldDurationMovementLock() const;

	void StartFieldSpawnSequence();
	void StartFieldRepeatTimer();
	void FinishFieldSpawnSequence();
	AActor* SpawnFieldActorForSocket(FName SocketName);

	void DestroyFieldActorWhenReplicationIsSafe(
		AActor* SpawnedActor,
		const FSkillActorFieldSettings& FieldSettings) const;

	bool ShouldRepeatFieldSpawnSequence() const;

	void BindFieldTriggerDamage(AActor* SpawnedActor);

	FGameplayEffectSpecHandle MakeFieldTriggerDamageSpec(
		AActor* DamageSourceActor,
		float DamageMagnitude) const;

	float CalculateFieldTriggerDamageMagnitude() const;

	void ScheduleCompletion();
	void CleanupFieldTasks();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ShowOnlyInnerProperties))
	FSkillActorFieldSettings Settings;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> FieldMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitFieldMontageTriggerTask;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedFieldActors;

	TArray<FName> PendingFieldSocketNames;

	FTimerHandle FieldSpawnTimerHandle;
	FTimerHandle FieldRepeatSpawnTimerHandle;
	FTimerHandle FieldEndTimerHandle;

	int32 NextFieldSocketIndex = 0;
	bool bFieldStarted = false;

	UPROPERTY(Transient)
	TObjectPtr<USkillTriggerDamage> FieldTriggerDamage;

	FActiveGameplayEffectHandle MovementSpeedEffectHandle;
};