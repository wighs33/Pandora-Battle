#include "Weapon/WeaponBase.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Character/CharacterHitValidation.h"
#include "Character/PdPlayer.h"
#include "Common/LabGameplayTags.h"
#include "Common/WeaponAnimNotifyNames.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Player/CombatComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Settings/GameSettingsSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Item/ArrowProjectileBase.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WeaponBase)

namespace
{
    const FName SkillTrailComponentName(TEXT("SkillTrailComponent"));
    const FName SkillTrailComponentDisplayName(TEXT("Skill Trail Component"));
    const FName SkillTrailName(TEXT("SkillTrail"));

    bool IsNamedSkillTrailComponent(const UNiagaraComponent* NiagaraComponent)
    {
        if (!NiagaraComponent)
        {
            return false;
        }

        return NiagaraComponent->GetFName() == SkillTrailComponentName
            || NiagaraComponent->GetFName() == SkillTrailComponentDisplayName
            || NiagaraComponent->GetFName() == SkillTrailName
            || NiagaraComponent->ComponentHasTag(SkillTrailComponentName)
            || NiagaraComponent->ComponentHasTag(SkillTrailComponentDisplayName)
            || NiagaraComponent->ComponentHasTag(SkillTrailName);
    }
}

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

void AWeaponBase::SetBeginOverlapEnabled(const bool bEnabled)
{
    if (bEnabled)
    {
        StartAttackTrace();
    }
    else
    {
        StopAttackTrace();
    }
}

void AWeaponBase::StartAttackTrace()
{
}

void AWeaponBase::StartAttackTraceForSection(FName AttackSectionName)
{
    static_cast<void>(AttackSectionName);
}

void AWeaponBase::ResetAttackHitTracking()
{
}

void AWeaponBase::StopAttackTrace()
{
}

void AWeaponBase::ConfigureSkillSlash(
    UNiagaraSystem* SlashSystem,
    const FVector& SlashScale,
    const FVector& SlashSpawnLocationOffset,
    FName SlashSpawnSocketName,
    const FRotator& SlashSpawnRotationOffset,
    float AttackTraceEndMultiplier,
    bool bEnableHitTrace,
    TSubclassOf<UGameplayEffect> AdditionalDamageEffectClass,
    FGameplayTag AdditionalDamageDataTag,
    float AdditionalDamageMagnitude,
    int32 AdditionalDamageLevel,
    UObject* AdditionalDamageSourceObject,
    const FGameplayEffectSpecHandle& DebuffEffectSpecHandle,
    UStatusEffectDefinition* StatusEffectDefinition,
    float AdditionalDamageDelay)
{
    static_cast<void>(SlashSystem);
    static_cast<void>(SlashScale);
    static_cast<void>(SlashSpawnLocationOffset);
    static_cast<void>(SlashSpawnSocketName);
    static_cast<void>(SlashSpawnRotationOffset);
    static_cast<void>(AttackTraceEndMultiplier);
    static_cast<void>(bEnableHitTrace);
    static_cast<void>(AdditionalDamageEffectClass);
    static_cast<void>(AdditionalDamageDataTag);
    static_cast<void>(AdditionalDamageMagnitude);
    static_cast<void>(AdditionalDamageLevel);
    static_cast<void>(AdditionalDamageSourceObject);
    static_cast<void>(DebuffEffectSpecHandle);
    static_cast<void>(StatusEffectDefinition);
    static_cast<void>(AdditionalDamageDelay);
}

void AWeaponBase::PlaySkillSlashVisual()
{
}

void AWeaponBase::ClearSkillSlash()
{
}

void AWeaponBase::SetTemporaryAttackTraceEndZMultiplier(UObject* SourceObject, float Multiplier)
{
    static_cast<void>(SourceObject);
    static_cast<void>(Multiplier);
}

void AWeaponBase::ClearTemporaryAttackTraceEndZMultiplier(UObject* SourceObject)
{
    static_cast<void>(SourceObject);
}

float AWeaponBase::GetTemporaryAttackTraceEndZMultiplier() const
{
    return 1.0f;
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

bool AWeaponBase::StartSkillWeaponTrail()
{
    return StartSkillWeaponTrail(ActiveSkillTrailSystem);
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

bool AWeaponBase::SupportsAimInput() const
{
    const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
    return ItemDefinition && ItemDefinition->WeaponData.Aim.bSupportsInput;
}

bool AWeaponBase::CanUseRangedWeapon(const ACharacterBase* AttackingCharacter, const bool bRequirePlayerAim) const
{
    if (!SupportsAimInput()
        || !AttackingCharacter
        || AttackingCharacter != GetOwningCharacter()
        || !IsCurrentWeaponForOwner()
        || AttackingCharacter->IsStatusFrozen())
    {
        return false;
    }

    const UPdAbilitySystemComponent* AbilitySystemComponent = AttackingCharacter->GetPdAbilitySystemComponent();
    if (!AbilitySystemComponent
        || AbilitySystemComponent->GetNumericAttribute(UBasicAttributeSet::GetHealthAttribute()) <= 0.0f
        || AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::State_Dead)
        || AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::Status_Frostbite)
        || AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::State_Movement_Airborne)
        || AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::GameplayAbility_AOEAttack_Active)
        || AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::GameplayAbility_ShootProjectile_Active))
    {
        return false;
    }

    const UCharacterMovementComponent* MovementComponent = AttackingCharacter->GetCharacterMovement();
    if (MovementComponent && MovementComponent->IsFalling())
    {
        return false;
    }

    if (!bRequirePlayerAim)
    {
        return true;
    }

    const APdPlayer* PlayerCharacter = Cast<APdPlayer>(AttackingCharacter);
    return PlayerCharacter && PlayerCharacter->IsWeaponAimActive();
}

bool AWeaponBase::CanServerUseRangedWeapon(const ACharacterBase* AttackingCharacter, const bool bRequirePlayerAim) const
{
    return HasAuthority() && CanUseRangedWeapon(AttackingCharacter, bRequirePlayerAim);
}

FGameplayTag AWeaponBase::GetAimCrosshairWidgetTag() const
{
    const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
    return ItemDefinition ? ItemDefinition->WeaponData.Aim.CrosshairWidgetTag : FGameplayTag();
}

const FWeaponAimCameraSettings& AWeaponBase::GetAimCameraSettings() const
{
    if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
    {
        return ItemDefinition->WeaponData.Aim.CameraSettings;
    }

    static const FWeaponAimCameraSettings DefaultAimCameraSettings;
    return DefaultAimCameraSettings;
}

bool AWeaponBase::ShouldTriggerHitReactOnDamage() const
{
    return true;
}

bool AWeaponBase::HandleAimStart(APdPlayer* PlayerCharacter)
{
    if (!CanUseRangedWeapon(PlayerCharacter, false))
    {
        return false;
    }

    PlayerCharacter->SetWeaponAimActive(true, GetAimCameraSettings());
    return true;
}

void AWeaponBase::HandleAimEnd(APdPlayer* PlayerCharacter)
{
    if (!SupportsAimInput() || !PlayerCharacter)
    {
        return;
    }

    PlayerCharacter->SetWeaponAimActive(false, GetAimCameraSettings());
}

bool AWeaponBase::HandlePrimaryAttack(APdPlayer* PlayerCharacter)
{
    static_cast<void>(PlayerCharacter);
    return false;
}

bool AWeaponBase::HandleAIPrimaryAttack(ACharacterBase* AttackingCharacter, AActor* TargetActor)
{
    static_cast<void>(AttackingCharacter);
    static_cast<void>(TargetActor);
    return false;
}

bool AWeaponBase::HandleAIPrimaryAttackAtLocation(
    ACharacterBase* AttackingCharacter,
    AActor* TargetActor,
    const FVector& TargetLocation)
{
    static_cast<void>(AttackingCharacter);
    static_cast<void>(TargetActor);
    static_cast<void>(TargetLocation);
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

bool AWeaponBase::OnWeaponAnimNotifyTiming(FName NotifyName, APdPlayer* PlayerCharacter)
{
    static_cast<void>(PlayerCharacter);

    if (NotifyName == WeaponAnimNotifyNames::StartSkillTrail())
    {
        return ActiveSkillTrailSystem ? StartSkillWeaponTrail() : false;
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

bool AWeaponBase::ResolveServerAimViewPoint(
    const APdPlayer* PlayerCharacter,
    const FVector& RequestedViewLocation,
    const FVector& RequestedViewDirection,
    FVector& OutViewLocation,
    FVector& OutViewDirection) const
{
    if (!PlayerCharacter)
    {
        return false;
    }

    const FVector RequestedDirection = RequestedViewDirection.GetSafeNormal();
    const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
    const float MaxAcceptedViewDistance = ItemDefinition
        ? ItemDefinition->WeaponData.Aim.MaxAcceptedServerViewDistance
        : 0.0f;

    if (!RequestedDirection.IsNearlyZero()
        && MaxAcceptedViewDistance > 0.0f
        && FVector::DistSquared(RequestedViewLocation, PlayerCharacter->GetActorLocation()) <= FMath::Square(MaxAcceptedViewDistance))
    {
        OutViewLocation = RequestedViewLocation;
        OutViewDirection = RequestedDirection;
        return true;
    }

    return PlayerCharacter->GetWeaponAimViewPoint(OutViewLocation, OutViewDirection);
}

bool AWeaponBase::ResolveAimTargetBeyondLaunchPoint(
    const FVector& ViewLocation,
    const FVector& ViewDirection,
    const FVector& LaunchStartLocation,
    float TraceRange,
    const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
    const TArray<AActor*>& ActorsToIgnore,
    EDrawDebugTrace::Type DebugDrawType,
    FVector& OutTargetLocation,
    FHitResult* OutAimHitResult) const
{
    if (OutAimHitResult)
    {
        *OutAimHitResult = FHitResult();
    }

    const FVector SafeViewDirection = ViewDirection.GetSafeNormal();
    if (SafeViewDirection.IsNearlyZero() || TraceRange <= 0.0f)
    {
        return false;
    }

    const float CameraToLaunchDistance = FVector::Distance(ViewLocation, LaunchStartLocation);
    const FVector AimTraceEnd = ViewLocation + SafeViewDirection * (CameraToLaunchDistance + TraceRange);
    FVector AimTraceStart = ViewLocation;
    TArray<AActor*> AimActorsToIgnore = ActorsToIgnore;

    constexpr int32 MaxSkippedAimObstructions = 16;
    constexpr float AimTraceAdvanceDistance = 2.0f;
    for (int32 AttemptIndex = 0; AttemptIndex < MaxSkippedAimObstructions; ++AttemptIndex)
    {
        FHitResult HitResult;
        const bool bHit = UKismetSystemLibrary::LineTraceSingleForObjects(
            this,
            AimTraceStart,
            AimTraceEnd,
            ObjectTypes,
            false,
            AimActorsToIgnore,
            DebugDrawType,
            HitResult,
            true,
            FLinearColor::Red,
            FLinearColor::Green,
            5.0f);

        if (!bHit)
        {
            OutTargetLocation = AimTraceEnd;
            return true;
        }

        const FVector HitLocation = HitResult.Location;
        const float HitForwardDistanceFromLaunch = FVector::DotProduct(HitLocation - LaunchStartLocation, SafeViewDirection);
        const bool bHitIsBehindLaunchPlane = HitForwardDistanceFromLaunch <= AimTraceAdvanceDistance;
        const bool bCharacterNonMeshHit = PdCharacterHitValidation::IsCharacterRelatedNonMeshHit(
            HitResult.GetActor(),
            HitResult.GetComponent());

        if (!bHitIsBehindLaunchPlane && !bCharacterNonMeshHit)
        {
            OutTargetLocation = HitLocation;
            if (OutAimHitResult)
            {
                *OutAimHitResult = HitResult;
            }
            return true;
        }

        if (bHitIsBehindLaunchPlane && IsValid(HitResult.GetActor()))
        {
            AimActorsToIgnore.AddUnique(HitResult.GetActor());
        }

        const float RemainingTraceDistance = FVector::DotProduct(AimTraceEnd - HitLocation, SafeViewDirection);
        if (RemainingTraceDistance <= AimTraceAdvanceDistance)
        {
            break;
        }

        AimTraceStart = HitLocation + SafeViewDirection * AimTraceAdvanceDistance;
    }

    OutTargetLocation = AimTraceEnd;
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

FName AWeaponBase::GetConfiguredPrimaryAttackResumeWeaponMontageSectionName() const
{
    return NAME_None;
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

UNiagaraComponent* AWeaponBase::ResolveSkillTrailComponent() const
{
    TArray<UNiagaraComponent*> NiagaraComponents;
    GetComponents(NiagaraComponents);

    UNiagaraComponent* SingleNiagaraComponent = nullptr;
    UNiagaraComponent* SingleNiagaraWithAsset = nullptr;
    int32 NiagaraComponentCount = 0;
    int32 NiagaraWithAssetCount = 0;

    for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
    {
        if (!NiagaraComponent)
        {
            continue;
        }

        ++NiagaraComponentCount;
        if (NiagaraComponentCount == 1)
        {
            SingleNiagaraComponent = NiagaraComponent;
        }

        if (NiagaraComponent->GetAsset())
        {
            ++NiagaraWithAssetCount;
            if (NiagaraWithAssetCount == 1)
            {
                SingleNiagaraWithAsset = NiagaraComponent;
            }
        }

        if (IsNamedSkillTrailComponent(NiagaraComponent))
        {
            return NiagaraComponent;
        }
    }

    if (NiagaraWithAssetCount == 1)
    {
        return SingleNiagaraWithAsset;
    }

    return NiagaraComponentCount == 1 ? SingleNiagaraComponent : nullptr;
}

bool AWeaponBase::ApplyDamageFromAuthoritativeRangedTrace(const FHitResult& HitResult)
{
    if (!HasAuthority() || !IsCurrentWeaponForOwner())
    {
        return false;
    }

    ACharacterBase* TargetCharacter = PdCharacterHitValidation::ResolveWeaponDamageHit(
        HitResult.GetActor(),
        HitResult.GetComponent());
    return TargetCharacter && ApplyDamageToTarget(TargetCharacter);
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
