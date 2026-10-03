#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "Skill/Actions/SkillAction.h"
#include "AbilitySystem/Ability/SkillAbility.h"
#include "Definition/AbilitySystem/SkillActorFieldSettings.h"
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
    void HandleFieldMontageTriggerEvent(FGameplayEventData Payload);

    UFUNCTION()
    void HandleFieldMontageFinished();

    UFUNCTION()
    void HandleFieldMontageInterrupted();
    void SpawnNextFieldActor();

    UFUNCTION()
    void HandleRepeatedFieldSpawnSequence();
    void HandleFieldTriggerDamageTick();

    UFUNCTION()
    void HandleFieldTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void HandleFieldTriggerEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex);

    UFUNCTION()
    void HandleFieldDurationFinished();

    // Internal Helpers ------------------------------------------------------------------------------------------------
    UAnimMontage* GetResolvedFieldMontage() const;
    FGameplayTag GetResolvedFieldTriggerEventTag() const;
    bool StartFieldMontageTask();
    void StartWaitFieldMontageTriggerTask();
    void TryCommitAndStartField();

    void StartFieldDurationMovementLockIfAllowed();
    bool ShouldSkipFieldDurationMovementLock() const;
    void ApplyFieldMovementSpeedIncrease();
    void RemoveFieldMovementSpeedIncrease();

    TArray<FName> GetConfiguredFieldSocketNames() const;
    void StartFieldSpawnSequence();
    void StartFieldRepeatTimer();
    void FinishFieldSpawnSequence();
    AActor* SpawnFieldActorForSocket(FName SocketName);

    void DestroyFieldActorWhenReplicationIsSafe(
        AActor* SpawnedActor,
        const FSkillActorFieldSettings& FieldSettings) const;

    FTransform ResolveFieldSpawnTransform(FName SocketName) const;
    USkeletalMeshComponent* ResolveFieldSpawnSocketMesh(FName SocketName) const;
    bool AttachSpawnedFieldActorToSocket(AActor* SpawnedActor, FName SocketName) const;
    bool ShouldRepeatFieldSpawnSequence() const;

    UPrimitiveComponent* FindFieldTriggerComponent(AActor* SpawnedActor) const;
    void BindFieldTriggerDamage(AActor* SpawnedActor);
    void UnbindFieldTriggerDamage();
    void StartFieldTriggerDamageTickIfNeeded();

    void ApplyFieldTriggerDamageToExistingOverlaps(
        AActor* DamageSourceActor,
        UPrimitiveComponent* TriggerComponent,
        bool bApplyDamage);

    void ApplyFieldTriggerDamage(
        AActor* DamageSourceActor,
        AActor* HitActor,
        bool bAllowRepeatedDamage = false);

    FGameplayEffectSpecHandle MakeFieldTriggerDamageSpec(
        AActor* DamageSourceActor,
        float DamageMagnitude) const;

    float CalculateFieldTriggerDamageMagnitude() const;
    void TrackFieldTriggerOverlap(AActor* DamageSourceActor, AActor* OtherActor);
    void UntrackFieldTriggerOverlap(AActor* DamageSourceActor, AActor* OtherActor);

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
    TArray<TObjectPtr<UPrimitiveComponent>> FieldTriggerComponents;

    FTimerHandle FieldTriggerDamageTickTimerHandle;
    TMap<FObjectKey, TSet<FObjectKey>> DamagedFieldTriggerActorsBySource;
    TMap<FObjectKey, TWeakObjectPtr<AActor>> FieldDamageSourceActorsByKey;
    TMap<FObjectKey, TArray<TWeakObjectPtr<AActor>>> FieldOverlappingActorsBySource;

    FActiveGameplayEffectHandle MovementSpeedEffectHandle;
};