#pragma once

#include "CoreMinimal.h"
#include "Weapon/WeaponBase.h"
#include "UObject/ObjectKey.h"
#include "MeleeWeapon.generated.h"

class UGameplayEffect;
class UNiagaraSystem;
class UStatusEffectDefinition;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API AMeleeWeapon : public AWeaponBase
{
    GENERATED_BODY()

public:
    AMeleeWeapon();

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual void StartAttackTrace() override;
    virtual void StartAttackTraceForSection(FName AttackSectionName) override;
    virtual void ResetAttackHitTracking() override;
    virtual void StopAttackTrace() override;

    virtual void ConfigureSkillSlash(
        UNiagaraSystem* SlashSystem,
        const FVector& SlashScale,
        const FVector& SlashSpawnLocationOffset,
        FName SlashSpawnSocketName,
        const FRotator& SlashSpawnRotationOffset,
        float AttackTraceEndMultiplier,
        bool bEnableHitTrace,
        TSubclassOf<UGameplayEffect> AdditionalDamageEffectClass = nullptr,
        FGameplayTag AdditionalDamageDataTag = FGameplayTag(),
        float AdditionalDamageMagnitude = 0.0f,
        int32 AdditionalDamageLevel = 1,
        UObject* AdditionalDamageSourceObject = nullptr,
        const FGameplayEffectSpecHandle& DebuffEffectSpecHandle = FGameplayEffectSpecHandle(),
        UStatusEffectDefinition* StatusEffectDefinition = nullptr,
        float AdditionalDamageDelay = 0.12f) override;

    virtual void PlaySkillSlashVisual() override;
    virtual void ClearSkillSlash() override;

    virtual void SetTemporaryAttackTraceEndZMultiplier(UObject* SourceObject, float Multiplier) override;
    virtual void ClearTemporaryAttackTraceEndZMultiplier(UObject* SourceObject) override;
    virtual float GetTemporaryAttackTraceEndZMultiplier() const override;

    virtual bool OnWeaponAnimNotifyTiming(FName NotifyName, APdPlayer* PlayerCharacter) override;

    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "!Weapon|Trace")
    TObjectPtr<USceneComponent> AttackTraceStart;

    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "!Weapon|Trace")
    TObjectPtr<USceneComponent> AttackTraceEnd;

protected:
    UFUNCTION(NetMulticast, Unreliable)
    void MulticastSpawnSkillSlashNiagara(
        UNiagaraSystem* SlashSystem,
        const FVector& SpawnLocation,
        const FRotator& SpawnRotation,
        const FVector& SpawnScale);

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastDrawInterpolatedAttackTraceDebug(
        const TArray<FVector>& StartLocations,
        const TArray<FVector>& EndLocations,
        const TArray<FHitResult>& Hits);

    virtual bool ApplyDamageToTarget(AActor* TargetActor) override;

private:
    void StartAttackTraceInternal(bool bResetHitActors);
    void PerformAttackTrace();

    bool CanDamageMeleeTracedHit(const FHitResult& HitResult) const;
    FVector GetAttackTraceEndLocation(const FVector& TraceStartLocation) const;
    bool ApplyDamageFromAuthoritativeMeleeTrace(const FHitResult& HitResult);

    bool HasActiveSkillAdditionalDamage() const;
    void ApplyActiveSkillAdditionalDamageToTarget(ACharacterBase* TargetCharacter);
    void ApplySkillDebuffToTarget(
        ACharacterBase* TargetCharacter,
        const FGameplayEffectSpecHandle& DebuffEffectSpecHandle,
        UStatusEffectDefinition* StatusEffectDefinition);

    void ApplySkillAdditionalDamageToTarget(
        ACharacterBase* TargetCharacter,
        TSubclassOf<UGameplayEffect> DamageEffectClass,
        FGameplayTag DamageDataTag,
        float DamageMagnitude,
        int32 DamageLevel,
        UObject* DamageSourceObject,
        const FGameplayEffectSpecHandle& DebuffEffectSpecHandle,
        UStatusEffectDefinition* StatusEffectDefinition);

    void ClearActiveSkillAdditionalDamage();

    void SpawnSkillSlashNiagara();
    bool SpawnSkillSlashNiagaraLocal(
        UNiagaraSystem* SlashSystem,
        const FVector& SpawnLocation,
        const FRotator& SpawnRotation,
        const FVector& SpawnScale);

    bool ConsumeMatchingPredictedSkillSlash(
        UNiagaraSystem* SlashSystem,
        const FVector& SpawnLocation);

    void DrawInterpolatedAttackTraceDebug(
        const TArray<FVector>& StartLocations,
        const TArray<FVector>& EndLocations,
        const TArray<FHitResult>& Hits) const;

private:
    UPROPERTY(Transient)
    TSet<TObjectPtr<AActor>> HitActorsInCurrentAttack;

    UPROPERTY(Transient)
    FName TrackedAttackSectionName = NAME_None;

    UPROPERTY(Transient)
    bool bAttackTraceActive = false;

    UPROPERTY(Transient)
    bool bHasPreviousAttackTraceSegment = false;

    UPROPERTY(Transient)
    FVector PreviousAttackTraceStartLocation = FVector::ZeroVector;

    UPROPERTY(Transient)
    FVector PreviousAttackTraceEndLocation = FVector::ZeroVector;

    UPROPERTY(Transient)
    TObjectPtr<UNiagaraSystem> ActiveSkillSlashSystem;

    UPROPERTY(Transient)
    FVector ActiveSkillSlashScale = FVector::OneVector;

    UPROPERTY(Transient)
    FVector ActiveSkillSlashSpawnLocationOffset = FVector::ZeroVector;

    UPROPERTY(Transient)
    FName ActiveSkillSlashSpawnSocketName = NAME_None;

    UPROPERTY(Transient)
    FRotator ActiveSkillSlashSpawnRotationOffset = FRotator::ZeroRotator;

    UPROPERTY(Transient)
    float ActiveSkillAttackTraceEndMultiplier = 1.0f;

    UPROPERTY(Transient)
    bool bSkillSlashHitTraceEnabled = false;

    UPROPERTY(Transient)
    TSubclassOf<UGameplayEffect> ActiveSkillAdditionalDamageEffectClass;

    UPROPERTY(Transient)
    FGameplayTag ActiveSkillAdditionalDamageDataTag;

    UPROPERTY(Transient)
    float ActiveSkillAdditionalDamageMagnitude = 0.0f;

    UPROPERTY(Transient)
    int32 ActiveSkillAdditionalDamageLevel = 1;

    UPROPERTY(Transient)
    TObjectPtr<UObject> ActiveSkillAdditionalDamageSourceObject;

    FGameplayEffectSpecHandle ActiveSkillDebuffEffectSpecHandle;

    UPROPERTY(Transient)
    TObjectPtr<UStatusEffectDefinition> ActiveSkillStatusEffectDefinition;

    TWeakObjectPtr<UNiagaraSystem> PredictedSkillSlashSystem;
    FVector PredictedSkillSlashLocation = FVector::ZeroVector;
    double PredictedSkillSlashWorldTime = -1.0;
    bool bHasPendingPredictedSkillSlash = false;

    UPROPERTY(Transient)
    float ActiveSkillAdditionalDamageDelay = 0.12f;

    TMap<FObjectKey, float> TemporaryAttackTraceEndZMultipliers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Weapon|Trace", meta = (AllowPrivateAccess = "true", ClampMin = "0.001"))
    float AttackTraceInterval = 0.033333f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Weapon|Trace",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "20.0", ForceUnits = "cm"))
    float AttackTraceRadius = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Weapon|Trace|Debug", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float AttackTraceDebugDrawTime = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Weapon|Trace|Debug", meta = (AllowPrivateAccess = "true"))
    FLinearColor AttackTraceDebugTraceColor = FLinearColor::Red;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Weapon|Trace|Debug", meta = (AllowPrivateAccess = "true"))
    FLinearColor AttackTraceDebugHitColor = FLinearColor::Green;

    FTimerHandle AttackTraceTimerHandle;
};
