#pragma once

#include "CoreMinimal.h"
#include "Common/WeaponDefinitionData.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "WeaponBase.generated.h"

class AArrowProjectileBase;
class ACharacterBase;
class APdPlayer;
class UAnimMontage;
class UGameplayEffect;
class UItemDefinition;
class UNiagaraComponent;
class UNiagaraSystem;
class UPrimitiveComponent;
class USceneComponent;
class USkeletalMeshComponent;
class UStatusEffectDefinition;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API AWeaponBase : public AActor
{
    GENERATED_BODY()

public:
    AWeaponBase();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Optional melee hooks. AMeleeWeapon overrides these while generic callers stay weapon-agnostic.
    UFUNCTION(BlueprintCallable, Category = "!Weapon|Collision")
    void SetBeginOverlapEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "!Weapon|Collision")
    virtual void StartAttackTrace();

    virtual void StartAttackTraceForSection(FName AttackSectionName);
    virtual void ResetAttackHitTracking();

    UFUNCTION(BlueprintCallable, Category = "!Weapon|Collision")
    virtual void StopAttackTrace();

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
        float AdditionalDamageDelay = 0.12f);

    virtual void PlaySkillSlashVisual();
    virtual void ClearSkillSlash();

    UFUNCTION(BlueprintCallable, Category = "!Weapon|Trace")
    virtual void SetTemporaryAttackTraceEndZMultiplier(UObject* SourceObject, float Multiplier);

    UFUNCTION(BlueprintCallable, Category = "!Weapon|Trace")
    virtual void ClearTemporaryAttackTraceEndZMultiplier(UObject* SourceObject);

    UFUNCTION(BlueprintPure, Category = "!Weapon|Trace")
    virtual float GetTemporaryAttackTraceEndZMultiplier() const;

    // Common weapon animation / presentation
    bool PlayWeaponAttackMontage(FName StartingSection = NAME_None);

    UFUNCTION(BlueprintCallable, Category = "!Weapon|Animation")
    bool JumpToWeaponMontageSectionAndResume(FName SectionName);

    UFUNCTION(BlueprintCallable, Category = "!Weapon|Animation")
    void StopWeaponMontage(float BlendOutTime = 0.1f);

    UFUNCTION(BlueprintCallable, Category = "!Weapon|VFX")
    bool HasSkillWeaponTrailComponent() const;

    UFUNCTION(BlueprintCallable, Category = "!Weapon|VFX")
    bool StartSkillWeaponTrail(UNiagaraSystem* TrailSystem);

    bool StartSkillWeaponTrail();

    UFUNCTION(BlueprintCallable, Category = "!Weapon|VFX")
    void StopSkillWeaponTrail();

    void PlayComboWindowStartEffect(UNiagaraSystem* EffectSystem);

    void InitializeFromItemDefinition(const UItemDefinition* InItemDefinition);

    // Query helpers
    bool SupportsAimInput() const;
    bool CanUseRangedWeapon(const ACharacterBase* AttackingCharacter, bool bRequirePlayerAim) const;
    FGameplayTag GetAimCrosshairWidgetTag() const;
    const FWeaponAimCameraSettings& GetAimCameraSettings() const;
    virtual bool ShouldTriggerHitReactOnDamage() const;

    // Input commands
    virtual bool HandleAimStart(APdPlayer* PlayerCharacter);
    virtual void HandleAimEnd(APdPlayer* PlayerCharacter);
    virtual bool HandlePrimaryAttack(APdPlayer* PlayerCharacter);
    virtual bool HandleAIPrimaryAttack(ACharacterBase* AttackingCharacter, AActor* TargetActor);
    virtual bool HandleAIPrimaryAttackAtLocation(
        ACharacterBase* AttackingCharacter,
        AActor* TargetActor,
        const FVector& TargetLocation);
    virtual bool SupportsAutomaticFire() const;
    virtual float GetAutomaticFireInterval() const;

    // Animation notify callback
    virtual bool OnWeaponAnimNotifyTiming(FName NotifyName, APdPlayer* PlayerCharacter);

    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "!Weapon")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "!Weapon")
    TObjectPtr<USkeletalMeshComponent> WeaponMesh;

protected:
    UFUNCTION(NetMulticast, Reliable)
    void MulticastStartSkillWeaponTrail(UNiagaraSystem* TrailSystem);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastStopSkillWeaponTrail();

    const UItemDefinition* GetSourceItemDefinition() const;

    bool CanServerUseRangedWeapon(
        const ACharacterBase* AttackingCharacter,
        bool bRequirePlayerAim) const;

    bool TryGetOwnerMeshSocketLocation(
        const ACharacterBase* Character,
        FName SocketName,
        FVector& OutLocation) const;

    bool ResolveServerAimViewPoint(
        const APdPlayer* PlayerCharacter,
        const FVector& RequestedViewLocation,
        const FVector& RequestedViewDirection,
        FVector& OutViewLocation,
        FVector& OutViewDirection) const;

    bool ResolveAimTargetBeyondLaunchPoint(
        const FVector& ViewLocation,
        const FVector& ViewDirection,
        const FVector& LaunchStartLocation,
        float TraceRange,
        const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
        const TArray<AActor*>& ActorsToIgnore,
        EDrawDebugTrace::Type DebugDrawType,
        FVector& OutTargetLocation,
        FHitResult* OutAimHitResult = nullptr) const;

    float GetWeaponAttackSpeedPlayRate() const;
    virtual UAnimMontage* GetConfiguredWeaponMontage() const;
    virtual FName GetConfiguredPrimaryAttackResumeWeaponMontageSectionName() const;

    ACharacterBase* GetOwningCharacter() const;
    bool IsCurrentWeaponForOwner() const;
    bool IsAttackDebugVisualizationEnabled() const;
    bool PlayConfiguredWeaponMontage(FName StartingSection, float PlayRate);
    bool ApplyDamageFromAuthoritativeRangedTrace(const FHitResult& HitResult);
    virtual bool ApplyDamageToTarget(AActor* TargetActor);
    bool ApplySkillWeaponTrailVisual(bool bActivate);
    UNiagaraComponent* ResolveSkillTrailComponent() const;

private:
    friend class AArrowProjectileBase;

    bool ApplyDamageFromAuthoritativeProjectileImpact(
        AActor* HitActor,
        const UPrimitiveComponent* HitComponent,
        const AArrowProjectileBase* ProjectileSource);

protected:
    UPROPERTY(Replicated, Transient)
    TObjectPtr<UItemDefinition> SourceItemDefinition;

    UPROPERTY(Transient)
    TObjectPtr<UNiagaraSystem> ActiveSkillTrailSystem;

    // Shared by melee, bow, and gun traces.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Weapon|Trace|Debug")
    bool bDrawAttackTraceDebug = false;
};
