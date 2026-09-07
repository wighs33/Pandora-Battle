#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "TimerManager.h"
#include "UObject/ObjectKey.h"
#include "StaticAbility.generated.h"

class AActor;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UPrimitiveComponent;
class USkeletalMeshComponent;
struct FGameplayEffectSpecHandle;
struct FSkillStaticSettings;

UCLASS(Blueprintable)
class LABPROJECT_API UStaticAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UStaticAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void OnAbilityEnding() override;

private:
	const FSkillStaticSettings* GetStaticSettings() const;
	UAnimMontage* GetResolvedStaticMontage() const;
	FGameplayTag GetResolvedStaticTriggerEventTag() const;
	bool StartStaticMontageTask();
	void StartWaitStaticMontageTriggerTask();
	void TryCommitAndStartStatic();
	bool StartStaticDurationTimerFromSkillStart();
	void StartStaticDurationMovementLockIfAllowed();
	bool ShouldSkipStaticDurationMovementLock() const;
	void ApplyStaticMovementSpeedIncrease();
	void RemoveStaticMovementSpeedIncrease();
	TArray<FName> GetConfiguredStaticSocketNames() const;
	void StartStaticSpawnSequence();
	void StartStaticRepeatAndEndTimers();
	void SpawnNextStaticActor();
	void FinishStaticSpawnSequence();
	AActor* SpawnStaticActorForSocket(FName SocketName, int32 SocketIndex, int32 SocketCount);
	void DestroyStaticActorWhenReplicationIsSafe(AActor* SpawnedActor, const FSkillStaticSettings& StaticSettings) const;
	FTransform ResolveStaticSpawnTransform(FName SocketName) const;
	USkeletalMeshComponent* ResolveStaticSpawnSocketMesh(FName SocketName) const;
	bool AttachSpawnedStaticActorToSocket(AActor* SpawnedActor, FName SocketName) const;
	bool ShouldRepeatStaticSpawnSequence() const;
	UPrimitiveComponent* FindStaticTriggerComponent(AActor* SpawnedActor) const;
	void BindStaticTriggerDamage(AActor* SpawnedActor);
	void UnbindStaticTriggerDamage();
	void StartStaticTriggerDamageTickIfNeeded();
	void HandleStaticTriggerDamageTick();
	void ApplyStaticTriggerDamageToExistingOverlaps(AActor* DamageSourceActor, UPrimitiveComponent* TriggerComponent, bool bApplyDamage);
	void ApplyStaticTriggerDamage(AActor* DamageSourceActor, AActor* HitActor, bool bAllowRepeatedDamage = false);
	FGameplayEffectSpecHandle MakeStaticTriggerDamageSpec(AActor* DamageSourceActor, float DamageMagnitude) const;
	float CalculateStaticTriggerDamageMagnitude() const;
	void TrackStaticTriggerOverlap(AActor* DamageSourceActor, AActor* OtherActor);
	void UntrackStaticTriggerOverlap(AActor* DamageSourceActor, AActor* OtherActor);
	void ScheduleStaticAbilityEnd();
	void CleanupStaticTasks();

	UFUNCTION()
	void HandleStaticMontageTriggerEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleStaticMontageFinished();

	UFUNCTION()
	void HandleStaticMontageInterrupted();

	UFUNCTION()
	void HandleStaticDurationFinished();

	UFUNCTION()
	void HandleRepeatedStaticSpawnSequence();

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

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> StaticMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitStaticMontageTriggerTask;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedStaticActors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> StaticTriggerComponents;

	TArray<FName> PendingStaticSocketNames;
	FTimerHandle StaticSpawnTimerHandle;
	FTimerHandle StaticRepeatSpawnTimerHandle;
	FTimerHandle StaticEndTimerHandle;
	FTimerHandle StaticTriggerDamageTickTimerHandle;
	TMap<FObjectKey, TSet<FObjectKey>> DamagedStaticTriggerActorsBySource;
	TMap<FObjectKey, TWeakObjectPtr<AActor>> StaticDamageSourceActorsByKey;
	TMap<FObjectKey, TArray<TWeakObjectPtr<AActor>>> StaticOverlappingActorsBySource;
	int32 NextStaticSocketIndex = 0;
	bool bStaticStarted = false;

	FActiveGameplayEffectHandle MovementSpeedEffectHandle;
};
