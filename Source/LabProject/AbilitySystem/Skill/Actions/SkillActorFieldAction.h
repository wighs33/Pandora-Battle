#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "AbilitySystem/Skill/SkillAction.h"
#include "AbilitySystem/Ability/SkillAbility.h"
#include "Definition/AbilitySystem/SkillStaticSettings.h"
#include "TimerManager.h"
#include "UObject/ObjectKey.h"
#include "SkillActorFieldAction.generated.h"

class AActor;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UPrimitiveComponent;
class USkeletalMeshComponent;
struct FGameplayEffectSpecHandle;

/** 배치 액터의 생성 순서와 충돌 피해, 반복 생성 및 수명을 관리한다. */
UCLASS(meta = (DisplayName = "Actor Field"))
class LABPROJECT_API USkillActorFieldAction : public USkillAction
{
    GENERATED_BODY()

public:
    USkillActorFieldAction();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ShowOnlyInnerProperties))
    FSkillStaticSettings Settings;

protected:
    virtual void OnStart() override;
    virtual void OnStop() override;

private:
    // 시전 / 몽타주
    UAnimMontage* GetResolvedStaticMontage() const;
    FGameplayTag GetResolvedStaticTriggerEventTag() const;
    bool StartStaticMontageTask();
    void StartWaitStaticMontageTriggerTask();
    void TryCommitAndStartStatic();

    UFUNCTION()
    void HandleStaticMontageTriggerEvent(FGameplayEventData Payload);

    UFUNCTION()
    void HandleStaticMontageFinished();

    UFUNCTION()
    void HandleStaticMontageInterrupted();

    // 이동
    void StartStaticDurationMovementLockIfAllowed();
    bool ShouldSkipStaticDurationMovementLock() const;
    void ApplyStaticMovementSpeedIncrease();
    void RemoveStaticMovementSpeedIncrease();

    // 액터 생성 / 반복 생성
    TArray<FName> GetConfiguredStaticSocketNames() const;
    void StartStaticSpawnSequence();
    void StartStaticRepeatTimer();
    void SpawnNextStaticActor();
    void FinishStaticSpawnSequence();
    AActor* SpawnStaticActorForSocket(FName SocketName);

    void DestroyStaticActorWhenReplicationIsSafe(
        AActor* SpawnedActor,
        const FSkillStaticSettings& StaticSettings) const;

    FTransform ResolveStaticSpawnTransform(FName SocketName) const;
    USkeletalMeshComponent* ResolveStaticSpawnSocketMesh(FName SocketName) const;
    bool AttachSpawnedStaticActorToSocket(AActor* SpawnedActor, FName SocketName) const;
    bool ShouldRepeatStaticSpawnSequence() const;

    UFUNCTION()
    void HandleRepeatedStaticSpawnSequence();

    // 충돌 피해
    UPrimitiveComponent* FindStaticTriggerComponent(AActor* SpawnedActor) const;
    void BindStaticTriggerDamage(AActor* SpawnedActor);
    void UnbindStaticTriggerDamage();
    void StartStaticTriggerDamageTickIfNeeded();
    void HandleStaticTriggerDamageTick();

    void ApplyStaticTriggerDamageToExistingOverlaps(
        AActor* DamageSourceActor,
        UPrimitiveComponent* TriggerComponent,
        bool bApplyDamage);

    void ApplyStaticTriggerDamage(
        AActor* DamageSourceActor,
        AActor* HitActor,
        bool bAllowRepeatedDamage = false);

    FGameplayEffectSpecHandle MakeStaticTriggerDamageSpec(
        AActor* DamageSourceActor,
        float DamageMagnitude) const;

    float CalculateStaticTriggerDamageMagnitude() const;
    void TrackStaticTriggerOverlap(AActor* DamageSourceActor, AActor* OtherActor);
    void UntrackStaticTriggerOverlap(AActor* DamageSourceActor, AActor* OtherActor);

    UFUNCTION()
    void HandleStaticTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void HandleStaticTriggerEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex);

    // 종료 / 정리
    void ScheduleCompletion();
    void CleanupStaticTasks();

    UFUNCTION()
    void HandleStaticDurationFinished();

    // Task
    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> StaticMontageTask;

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitStaticMontageTriggerTask;

    // Spawn 상태
    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> SpawnedStaticActors;

    TArray<FName> PendingStaticSocketNames;

    FTimerHandle StaticSpawnTimerHandle;
    FTimerHandle StaticRepeatSpawnTimerHandle;
    FTimerHandle StaticEndTimerHandle;

    int32 NextStaticSocketIndex = 0;
    bool bStaticStarted = false;

    // Trigger Damage 상태
    UPROPERTY(Transient)
    TArray<TObjectPtr<UPrimitiveComponent>> StaticTriggerComponents;

    FTimerHandle StaticTriggerDamageTickTimerHandle;
    TMap<FObjectKey, TSet<FObjectKey>> DamagedStaticTriggerActorsBySource;
    TMap<FObjectKey, TWeakObjectPtr<AActor>> StaticDamageSourceActorsByKey;
    TMap<FObjectKey, TArray<TWeakObjectPtr<AActor>>> StaticOverlappingActorsBySource;

    // Buff 상태
    FActiveGameplayEffectHandle MovementSpeedEffectHandle;
};