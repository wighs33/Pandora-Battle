#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

class AArrowProjectileBase;
class ACharacterBase;
class APdPlayer;
class UAnimMontage;
class UItemDefinition;
class UNiagaraComponent;
class UNiagaraSystem;
class UPrimitiveComponent;
class USceneComponent;
class USkeletalMeshComponent;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API AWeaponBase : public AActor
{
    GENERATED_BODY()

public:
    // Engine Overrides ------------------------------------------------------------------------------------------------
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Public API ------------------------------------------------------------------------------------------------------
    AWeaponBase();

    bool PlayWeaponAttackMontage(FName StartingSection = NAME_None);

    UFUNCTION(BlueprintCallable, Category = "!Weapon|Animation")
    bool JumpToWeaponMontageSectionAndResume(FName SectionName);

    UFUNCTION(BlueprintCallable, Category = "!Weapon|Animation")
    void StopWeaponMontage(float BlendOutTime = 0.1f);

    UFUNCTION(BlueprintCallable, Category = "!Weapon|VFX")
    bool HasSkillWeaponTrailComponent() const;

    UFUNCTION(BlueprintCallable, Category = "!Weapon|VFX")
    bool StartSkillWeaponTrail(UNiagaraSystem* TrailSystem);

    UFUNCTION(BlueprintCallable, Category = "!Weapon|VFX")
    void StopSkillWeaponTrail();

    void PlayComboWindowStartEffect(UNiagaraSystem* EffectSystem);

    void InitializeFromItemDefinition(const UItemDefinition* InItemDefinition);

    virtual bool ShouldTriggerHitReactOnDamage() const;
    virtual bool SupportsAutomaticFire() const;
    virtual float GetAutomaticFireInterval() const;

protected:
    // Network RPCs ----------------------------------------------------------------------------------------------------
    UFUNCTION(NetMulticast, Reliable)
    void MulticastStartSkillWeaponTrail(UNiagaraSystem* TrailSystem);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastStopSkillWeaponTrail();

public:
    // Event Handlers --------------------------------------------------------------------------------------------------
    virtual bool HandlePrimaryAttack(APdPlayer* PlayerCharacter);
    virtual bool HandleAIPrimaryAttack(ACharacterBase* AttackingCharacter, AActor* TargetActor);
    virtual bool HandleAIPrimaryAttackAtLocation(
        ACharacterBase* AttackingCharacter,
        AActor* TargetActor,
        const FVector& TargetLocation);

    virtual bool OnWeaponAnimNotifyTiming(FName NotifyName, APdPlayer* PlayerCharacter);

protected:
    // Internal Helpers ------------------------------------------------------------------------------------------------
    const UItemDefinition* GetSourceItemDefinition() const;

    bool TryGetOwnerMeshSocketLocation(
        const ACharacterBase* Character,
        FName SocketName,
        FVector& OutLocation) const;

    float GetWeaponAttackSpeedPlayRate() const;
    virtual UAnimMontage* GetConfiguredWeaponMontage() const;

    ACharacterBase* GetOwningCharacter() const;
    bool IsCurrentWeaponForOwner() const;
    bool IsAttackDebugVisualizationEnabled() const;
    bool PlayConfiguredWeaponMontage(FName StartingSection, float PlayRate);
    virtual bool ApplyDamageToTarget(AActor* TargetActor);
    bool ApplySkillWeaponTrailVisual(bool bActivate);
    UNiagaraComponent* ResolveSkillTrailComponent() const;

private:
    bool ApplyDamageFromAuthoritativeProjectileImpact(
        AActor* HitActor,
        const UPrimitiveComponent* HitComponent,
        const AArrowProjectileBase* ProjectileSource);

public:
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "!Weapon")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "!Weapon")
    TObjectPtr<USkeletalMeshComponent> WeaponMesh;

private:
    friend class AArrowProjectileBase;

protected:
    // Blueprint에서 사용할 Trail 컴포넌트를 명시적으로 지정한다. 이름/태그로 추측하지 않는다.
    UPROPERTY(EditDefaultsOnly, Category = "!Weapon|VFX", meta = (UseComponentPicker, AllowedClasses = "/Script/Niagara.NiagaraComponent"))
    FComponentReference SkillTrailComponent;

    UPROPERTY(Replicated, Transient)
    TObjectPtr<UItemDefinition> SourceItemDefinition;

    UPROPERTY(Transient)
    TObjectPtr<UNiagaraSystem> ActiveSkillTrailSystem;

    // Shared by melee, bow, and gun traces.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Weapon|Trace|Debug")
    bool bDrawAttackTraceDebug = false;
};
