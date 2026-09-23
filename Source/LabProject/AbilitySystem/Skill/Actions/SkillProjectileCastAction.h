#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystem/Skill/SkillAction.h"
#include "Definition/AbilitySystem/SkillProjectileSettings.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "SkillProjectileCastAction.generated.h"

class AGameplayAbilityTargetActor;
class AProjectileBase;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitConfirmCancel;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitTargetData;
class UAnimMontage;
class UGameplayEffect;
class UStatusEffectDefinition;

/** 조준 확정, 차징과 소켓 연속 발사를 포함한 투사체 시전. */
UCLASS(meta = (DisplayName = "Projectile Cast"))
class LABPROJECT_API USkillProjectileCastAction : public USkillAction
{
    GENERATED_BODY()

public:
    USkillProjectileCastAction();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ShowOnlyInnerProperties))
    FSkillProjectileSettings Settings;

    UFUNCTION(BlueprintCallable, Category = "Skill|Projectile")
    FVector GetSpawnLocation() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Skill|Projectile")
    void ShootProjectile(FVector TargetLocation);

protected:
    virtual void OnStart() override;
    virtual void OnStop() override;

private:
    // 시전 흐름
    void StartProjectileCast();
    void StartShotWithoutMontage();
    void StartPlayerAiming();
    void ConfirmPlayerShot();
    void FireAtCurrentTargetOrFallback();
    void FireAtDefaultTarget();
    bool ExecuteProjectileShot(FVector TargetLocation);
    void TryFinishAfterProjectileFired();
    void FinishCast();

    // 몽타주 / 입력
    void StartShootProjectileEventTask();
    void PauseProjectileMontageForAiming();
    void ResumeProjectileMontageAfterAiming();

    UFUNCTION()
    void HandleMontageFinished();

    UFUNCTION()
    void HandleShootProjectileEvent(FGameplayEventData Payload);

    UFUNCTION()
    void HandleConfirmPressed();

    UFUNCTION()
    void HandleCancelPressed();

    // 타기팅
    void WaitForPlayerTargetData();

    UFUNCTION()
    void HandleTargetDataValid(const FGameplayAbilityTargetDataHandle& Data);

    UFUNCTION()
    void HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data);

    bool TryValidateServerProjectileTargetLocation(
        const FHitResult& ClientHitResult,
        const FVector& TargetDataEndPoint,
        bool bUsingGroundTargeting,
        FVector& OutValidatedLocation) const;

    bool ShouldRetargetUsingAim(const FVector& TargetLocation) const;
    bool TryResolveProjectileAimTargetLocation(FVector& OutTargetLocation) const;
    FVector ResolveDefaultTargetLocation() const;

    // Projectile
    FVector GetSpawnLocationForSocket(FName SocketName) const;
    AProjectileBase* SpawnReadiedProjectile();
    void DestroyReadiedProjectile();
    FGameplayEffectSpecHandle MakeDamageEffectSpec(float ChargeDamageAlpha = 1.0f) const;
    FGameplayEffectSpecHandle MakeStatusEffectSpec() const;

    // Socket Barrage
    bool TryStartSocketBarrage(FVector TargetLocation);
    AProjectileBase* SpawnPreparedSocketBarrageProjectile(FName SocketName);
    void LaunchSocketBarrageProjectile(AProjectileBase* Projectile);
    FVector ResolveSocketBarrageLaunchTargetLocation(const FVector& ProjectileLocation) const;
    FVector ResolveCharacterTargetSocketProjectileTargetLocation(const FVector& FromLocation) const;
    void FireNextSocketBarrageProjectile();
    void ClearSocketBarrageState(bool bDestroyPendingProjectiles);
    bool IsSocketBarrageActive() const;
    bool ShouldWaitForServerSocketBarrageEnd() const;

    // 설정
    UAnimMontage* GetConfiguredShootMontage() const;
    UStatusEffectDefinition* GetConfiguredStatusEffectDataAsset() const;
    TSubclassOf<UGameplayEffect> GetConfiguredStatusEffectClass() const;
    float GetConfiguredStatusEffectLevel() const;
    float GetConfiguredProjectileSpeed() const;
    float GetConfiguredProjectileRadius() const;
    float GetConfiguredProjectileArcHeight() const;
    float GetConfiguredProjectileArcGravityScale() const;
    FGameplayTag GetConfiguredShootProjectileEventTag() const;
    TArray<FName> GetConfiguredProjectileSocketNames() const;
    float GetConfiguredProjectileSocketFireInterval() const;
    FName GetConfiguredSpawnSocketName() const;
    float GetConfiguredMinimumForwardSpawnOffset() const;
    bool IsConfiguredReadiedProjectileChargeGrowthEnabled() const;
    float GetConfiguredReadiedProjectileScaleDuration() const;
    void ApplyConfiguredProjectileVisuals(AProjectileBase* Projectile) const;
    void ApplyConfiguredProjectileTrajectory(AProjectileBase* Projectile) const;
    void ApplyConfiguredProjectileImpactPersistence(AProjectileBase* Projectile) const;
    void ApplyConfiguredStatusEffect(AProjectileBase* Projectile) const;
    void ApplyReadiedProjectileScaleGrowth(AProjectileBase* Projectile) const;

    // 일반 조준
    float GetConfiguredTargetTraceMaxRange() const;
    float GetConfiguredMinimumTargetDistanceFromSpawn() const;
    bool GetConfiguredDrawTargetTraceDebug() const;

    // 지면 조준
    TSubclassOf<AGameplayAbilityTargetActor> GetConfiguredGroundTargetActorClass() const;
    float GetConfiguredGroundTargetingMaxRange() const;
    float GetConfiguredGroundTargetingTraceStartHeight() const;
    float GetConfiguredGroundTargetingTraceDepth() const;
    float GetConfiguredGroundTargetingCollisionRadius() const;
    float GetConfiguredGroundTargetingCollisionHeight() const;
    bool GetConfiguredDrawGroundTargetingDebug() const;
    float GetConfiguredGroundTargetingDecalSize() const;
    float GetConfiguredGroundTargetingDecalFinalSize() const;
    float CalculateConfiguredImpactAreaDamageRadius(float ChargeDamageAlpha) const;
    bool TryBuildGroundTargetingDecalGrowth(float& OutStartSize, float& OutTargetSize, float& OutDuration) const;

    // 정리
    void CleanupAimingState();

    // 시전 상태
    UPROPERTY(Transient)
    bool bEndAfterProjectileFired = false;

    UPROPERTY(Transient)
    bool bWaitingForPlayerConfirm = false;

    UPROPERTY(Transient)
    bool bPausedForPlayerAim = false;

    UPROPERTY(Transient)
    bool bPlayerProjectileConfirmed = false;

    UPROPERTY(Transient)
    bool bProjectileExecutionRequested = false;

    UPROPERTY(Transient)
    bool bProjectileSpawnSucceeded = false;

    UPROPERTY(Transient)
    bool bCleaningUpTargetDataTask = false;

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_WaitConfirmCancel> ConfirmCancelTask;

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_WaitGameplayEvent> ShootProjectileEventTask;

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> ShootMontageTask;

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_WaitTargetData> TargetDataTask;

    UPROPERTY(Transient)
    TObjectPtr<AProjectileBase> ReadiedProjectile;

    // Barrage 상태
    UPROPERTY(Transient)
    TArray<FName> SocketBarrageSocketNames;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AProjectileBase>> SocketBarrageProjectiles;

    UPROPERTY(Transient)
    FVector SocketBarrageTargetLocation = FVector::ZeroVector;

    UPROPERTY(Transient)
    bool bSocketBarrageUseCharacterTargetSocket = false;

    FTimerHandle SocketBarrageTimerHandle;
    int32 NextSocketBarrageProjectileIndex = 0;
    bool bSocketBarrageEndAbilityAfterFire = false;
};
