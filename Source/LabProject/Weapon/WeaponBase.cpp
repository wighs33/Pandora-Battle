#include "Weapon/WeaponBase.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Character/CharacterHitValidation.h"
#include "Common/WeaponAnimNotifyNames.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Player/CombatComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Settings/GameSettingsSubsystem.h"
#include "Item/ArrowProjectileBase.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WeaponBase)

AWeaponBase::AWeaponBase()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
    WeaponMesh->SetupAttachment(SceneRoot);
    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;
    DOREPLIFETIME_WITH_PARAMS_FAST(AWeaponBase, SourceItemDefinition, Params);
}

bool AWeaponBase::PlayWeaponAttackMontage(FName StartingSection)
{
    return PlayConfiguredWeaponMontage(StartingSection, GetWeaponAttackSpeedPlayRate());
}

bool AWeaponBase::PlayConfiguredWeaponMontage(FName StartingSection, float PlayRate)
{
    if (!WeaponMesh)
    {
        return false;
    }

    UAnimInstance* AnimInstance = WeaponMesh->GetAnimInstance();
    UAnimMontage* Montage = GetConfiguredWeaponMontage();
    if (!AnimInstance || !Montage)
    {
        return false;
    }

    const float SafePlayRate = FMath::Max(0.01f, PlayRate);
    if (AnimInstance->Montage_Play(Montage, SafePlayRate) <= 0.0f)
    {
        return false;
    }

    if (!StartingSection.IsNone())
    {
        AnimInstance->Montage_JumpToSection(StartingSection, Montage);
    }

    return true;
}

bool AWeaponBase::JumpToWeaponMontageSectionAndResume(FName SectionName)
{
    if (!WeaponMesh)
    {
        return false;
    }

    UAnimInstance* AnimInstance = WeaponMesh->GetAnimInstance();
    UAnimMontage* Montage = GetConfiguredWeaponMontage();
    if (!AnimInstance || !Montage)
    {
        return false;
    }

    if (!SectionName.IsNone())
    {
        AnimInstance->Montage_JumpToSection(SectionName, Montage);
    }

    AnimInstance->Montage_Resume(Montage);
    return true;
}

void AWeaponBase::StopWeaponMontage(float BlendOutTime)
{
    if (!WeaponMesh)
    {
        return;
    }

    UAnimInstance* AnimInstance = WeaponMesh->GetAnimInstance();
    UAnimMontage* Montage = GetConfiguredWeaponMontage();
    if (!AnimInstance || !Montage)
    {
        return;
    }

    AnimInstance->Montage_Stop(BlendOutTime, Montage);
}

bool AWeaponBase::HasSkillWeaponTrailComponent() const
{
    return ResolveSkillTrailComponent() != nullptr;
}

bool AWeaponBase::StartSkillWeaponTrail(UNiagaraSystem* TrailSystem)
{
    if (!TrailSystem)
    {
        return false;
    }

    ActiveSkillTrailSystem = TrailSystem;
    if (!ResolveSkillTrailComponent())
    {
        return false;
    }

    if (HasAuthority())
    {
        MulticastStartSkillWeaponTrail(TrailSystem);
        return true;
    }

    return ApplySkillWeaponTrailVisual(true);
}

void AWeaponBase::StopSkillWeaponTrail()
{
    if (HasAuthority())
    {
        MulticastStopSkillWeaponTrail();
        return;
    }

    ApplySkillWeaponTrailVisual(false);
}

void AWeaponBase::PlayComboWindowStartEffect(UNiagaraSystem* EffectSystem)
{
    if (!EffectSystem || GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    ACharacterBase* OwningCharacter = GetOwningCharacter();
    if (!OwningCharacter || !OwningCharacter->IsPlayerControlled() || !OwningCharacter->IsLocallyControlled())
    {
        return;
    }

    USceneComponent* EffectAttachComponent = OwningCharacter->GetRootComponent();
    if (!EffectAttachComponent)
    {
        return;
    }

    UNiagaraFunctionLibrary::SpawnSystemAttached(
        EffectSystem,
        EffectAttachComponent,
        NAME_None,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        EAttachLocation::KeepRelativeOffset,
        true,
        true,
        ENCPoolMethod::AutoRelease,
        true);
}

void AWeaponBase::InitializeFromItemDefinition(const UItemDefinition* InItemDefinition)
{
    SourceItemDefinition = const_cast<UItemDefinition*>(InItemDefinition);
    if (HasAuthority())
    {
        MARK_PROPERTY_DIRTY_FROM_NAME(AWeaponBase, SourceItemDefinition, this);
    }
}

bool AWeaponBase::ShouldTriggerHitReactOnDamage() const
{
    return true;
}

// 근접 공격은 Ability에서 처리하므로 직접 발사 API의 기본값은 미지원이다.
bool AWeaponBase::HandlePrimaryAttack(APdPlayer*)
{
    return false;
}

bool AWeaponBase::HandleAIPrimaryAttack(ACharacterBase*, AActor*)
{
    return false;
}

bool AWeaponBase::HandleAIPrimaryAttackAtLocation(ACharacterBase*, AActor*, const FVector&)
{
    return false;
}

bool AWeaponBase::SupportsAutomaticFire() const
{
    return false;
}

float AWeaponBase::GetAutomaticFireInterval() const
{
    return 0.0f;
}

bool AWeaponBase::OnWeaponAnimNotifyTiming(FName NotifyName, APdPlayer*)
{
    if (NotifyName == WeaponAnimNotifyNames::StartSkillTrail())
    {
        return ActiveSkillTrailSystem ? StartSkillWeaponTrail(ActiveSkillTrailSystem) : false;
    }

    if (NotifyName == WeaponAnimNotifyNames::StopSkillTrail())
    {
        if (ActiveSkillTrailSystem)
        {
            StopSkillWeaponTrail();
        }
        return true;
    }

    return false;
}

const UItemDefinition* AWeaponBase::GetSourceItemDefinition() const
{
    return SourceItemDefinition.Get();
}

bool AWeaponBase::TryGetOwnerMeshSocketLocation(
    const ACharacterBase* Character,
    FName SocketName,
    FVector& OutLocation) const
{
    const USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;
    if (!CharacterMesh || SocketName.IsNone() || !CharacterMesh->DoesSocketExist(SocketName))
    {
        return false;
    }

    OutLocation = CharacterMesh->GetSocketLocation(SocketName);
    return true;
}

float AWeaponBase::GetWeaponAttackSpeedPlayRate() const
{
    const ACharacterBase* CharacterOwner = Cast<ACharacterBase>(GetOwner());
    const UPdAbilitySystemComponent* AbilitySystemComponent = CharacterOwner
        ? Cast<UPdAbilitySystemComponent>(CharacterOwner->GetAbilitySystemComponent())
        : nullptr;
    const UBasicAttributeSet* AttributeSet = AbilitySystemComponent
        ? AbilitySystemComponent->GetSet<UBasicAttributeSet>()
        : nullptr;

    const float AttackSpeedPercent = AttributeSet ? FMath::Max(AttributeSet->GetAttackSpeed(), 0.0f) : 0.0f;
    return FMath::Max(0.01f, 1.0f + AttackSpeedPercent * 0.01f);
}

UAnimMontage* AWeaponBase::GetConfiguredWeaponMontage() const
{
    return nullptr;
}

void AWeaponBase::MulticastStartSkillWeaponTrail_Implementation(UNiagaraSystem* TrailSystem)
{
    ActiveSkillTrailSystem = TrailSystem;
    ApplySkillWeaponTrailVisual(true);
}

void AWeaponBase::MulticastStopSkillWeaponTrail_Implementation()
{
    ApplySkillWeaponTrailVisual(false);
}

ACharacterBase* AWeaponBase::GetOwningCharacter() const
{
    if (ACharacterBase* OwnerCharacter = Cast<ACharacterBase>(GetOwner()))
    {
        return OwnerCharacter;
    }

    return Cast<ACharacterBase>(GetAttachParentActor());
}

bool AWeaponBase::IsCurrentWeaponForOwner() const
{
    const ACharacterBase* SourceCharacter = GetOwningCharacter();
    const UEquipmentComponent* EquipmentComponent = SourceCharacter
        ? SourceCharacter->GetEquipmentComponent()
        : nullptr;
    return EquipmentComponent && EquipmentComponent->GetCurrentWeaponActor() == this;
}

bool AWeaponBase::IsAttackDebugVisualizationEnabled() const
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
    return false;
#else
    if (!bDrawAttackTraceDebug)
    {
        return false;
    }

    const UGameSettingDefinition* SettingDefinition = UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
    return SettingDefinition && SettingDefinition->bDrawAttackDebugVisualization;
#endif
}

bool AWeaponBase::ApplySkillWeaponTrailVisual(const bool bActivate)
{
    UNiagaraComponent* TrailComponent = ResolveSkillTrailComponent();
    if (!TrailComponent)
    {
        return false;
    }

    if (bActivate && !ActiveSkillTrailSystem)
    {
        return false;
    }

    if (bActivate)
    {
        TrailComponent->SetAsset(ActiveSkillTrailSystem, false);
        TrailComponent->SetAutoActivate(true);
        TrailComponent->SetVisibility(true, true);
        TrailComponent->Activate(true);
    }
    else
    {
        TrailComponent->Deactivate();
        TrailComponent->SetVisibility(false, true);
        TrailComponent->SetAutoActivate(false);
        ActiveSkillTrailSystem = nullptr;
    }

    return true;
}

bool AWeaponBase::ApplyDamageFromAuthoritativeProjectileImpact(
    AActor* HitActor,
    const UPrimitiveComponent* HitComponent,
    const AArrowProjectileBase* ProjectileSource)
{
    if (!HasAuthority() || !IsCurrentWeaponForOwner() || !IsValid(ProjectileSource))
    {
        return false;
    }

    const ACharacterBase* SourceCharacter = GetOwningCharacter();
    const bool bOwnedBySourceCharacter = ProjectileSource->GetOwner() == SourceCharacter
        || ProjectileSource->GetInstigator() == SourceCharacter;
    if (!SourceCharacter || !bOwnedBySourceCharacter)
    {
        return false;
    }

    ACharacterBase* TargetCharacter = PdCharacterHitValidation::ResolveWeaponDamageHit(HitActor, HitComponent);
    return TargetCharacter && ApplyDamageToTarget(TargetCharacter);
}

bool AWeaponBase::ApplyDamageToTarget(AActor* TargetActor)
{
    if (!HasAuthority())
    {
        return false;
    }

    ACharacterBase* SourceCharacter = GetOwningCharacter();
    ACharacterBase* TargetCharacter = Cast<ACharacterBase>(TargetActor);
    if (!SourceCharacter || !TargetCharacter || SourceCharacter == TargetCharacter)
    {
        return false;
    }

    UCombatComponent* CombatComponent = SourceCharacter->GetCombatComponent();
    return CombatComponent && CombatComponent->ApplyWeaponDamageToTarget(TargetCharacter);
}

UNiagaraComponent* AWeaponBase::ResolveSkillTrailComponent() const
{
    return Cast<UNiagaraComponent>(SkillTrailComponent.GetComponent(const_cast<AWeaponBase*>(this)));
}
