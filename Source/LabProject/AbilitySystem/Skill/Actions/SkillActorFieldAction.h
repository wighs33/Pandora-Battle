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
    // Public API ------------------------------------------------------------------------------------------------------
    USkillActorFieldAction();

protected:
    // Event Handlers --------------------------------------------------------------------------------------------------
    virtual void OnStart() override;
    virtual void OnStop() override;

private:
    UFUNCTION()
    void HandleStaticMontageTriggerEvent(FGameplayEventData Payload);

    UFUNCTION()
    void HandleStaticMontageFinished();

    UFUNCTION()
    void HandleStaticMontageInterrupted();
    void SpawnNextStaticActor();

    UFUNCTION()
    void HandleRepeatedStaticSpawnSequence();
    void HandleStaticTriggerDamageTick();

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

    UFUNCTION()
    void HandleStaticDurationFinished();

    // Internal Helpers ------------------------------------------------------------------------------------------------
    UAnimMontage* GetResolvedStaticMontage() const;
    FGameplayTag GetResolvedStaticTriggerEventTag() const;
    bool StartStaticMontageTask();
    void StartWaitStaticMontageTriggerTask();
    void TryCommitAndStartStatic();

    void StartStaticDurationMovementLockIfAllowed();
    bool ShouldSkipStaticDurationMovementLock() const;
    void ApplyStaticMovementSpeedIncrease();
    void RemoveStaticMovementSpeedIncrease();

    TArray<FName> GetConfiguredStaticSocketNames() const;
    void StartStaticSpawnSequence();
    void StartStaticRepeatTimer();
    void FinishStaticSpawnSequence();
    AActor* SpawnStaticActorForSocket(FName SocketName);

    void DestroyStaticActorWhenReplicationIsSafe(
        AActor* SpawnedActor,
        const FSkillStaticSettings& StaticSettings) const;

    FTransform ResolveStaticSpawnTransform(FName SocketName) const;
    USkeletalMeshComponent* ResolveStaticSpawnSocketMesh(FName SocketName) const;
    bool AttachSpawnedStaticActorToSocket(AActor* SpawnedActor, FName SocketName) const;
    bool ShouldRepeatStaticSpawnSequence() const;

    UPrimitiveComponent* FindStaticTriggerComponent(AActor* SpawnedActor) const;
    void BindStaticTriggerDamage(AActor* SpawnedActor);
    void UnbindStaticTriggerDamage();
    void StartStaticTriggerDamageTickIfNeeded();

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

    void ScheduleCompletion();
    void CleanupStaticTasks();

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ShowOnlyInnerProperties))
    FSkillStaticSettings Settings;

private:
    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> StaticMontageTask;

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitStaticMontageTriggerTask;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> SpawnedStaticActors;

    TArray<FName> PendingStaticSocketNames;

    FTimerHandle StaticSpawnTimerHandle;
    FTimerHandle StaticRepeatSpawnTimerHandle;
    FTimerHandle StaticEndTimerHandle;

    int32 NextStaticSocketIndex = 0;
    bool bStaticStarted = false;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UPrimitiveComponent>> StaticTriggerComponents;

    FTimerHandle StaticTriggerDamageTickTimerHandle;
    TMap<FObjectKey, TSet<FObjectKey>> DamagedStaticTriggerActorsBySource;
    TMap<FObjectKey, TWeakObjectPtr<AActor>> StaticDamageSourceActorsByKey;
    TMap<FObjectKey, TArray<TWeakObjectPtr<AActor>>> StaticOverlappingActorsBySource;

    FActiveGameplayEffectHandle MovementSpeedEffectHandle;
};