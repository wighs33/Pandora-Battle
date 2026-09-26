#include "Weapon/MeleeWeapon.h"

#include "ActiveGameplayEffectHandle.h"
#include "Character/CharacterBase.h"
#include "Character/CharacterHitValidation.h"
#include "Common/CollisionChannels.h"
#include "Common/WeaponAnimNotifyNames.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/AbilitySystem/StatusEffectReplicationComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/AbilitySystem/StatusEffectDefinition.h"
#include "Definition/Common/ProjectTagDefinition.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Kismet/KismetSystemLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeleeWeapon)

namespace
{
    constexpr float AttackTraceInterpolationDistance = 5.0f;
}

AMeleeWeapon::AMeleeWeapon()
{
    AttackTraceStart = CreateDefaultSubobject<USceneComponent>(TEXT("TraceStart"));
    AttackTraceStart->SetupAttachment(WeaponMesh);
    AttackTraceStart->bEditableWhenInherited = true;

    AttackTraceEnd = CreateDefaultSubobject<USceneComponent>(TEXT("TraceEnd"));
    AttackTraceEnd->SetupAttachment(WeaponMesh);
    AttackTraceEnd->bEditableWhenInherited = true;
}

void AMeleeWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearSkillSlash();
    Super::EndPlay(EndPlayReason);
}

bool AMeleeWeapon::OnWeaponAnimNotifyTiming(FName NotifyName, APdPlayer* PlayerCharacter)
{
    if (NotifyName == WeaponAnimNotifyNames::StartSkillTrail())
    {
        const bool bHandledByBase = Super::OnWeaponAnimNotifyTiming(NotifyName, PlayerCharacter);
        if (bSkillSlashHitTraceEnabled)
        {
            StartAttackTrace();
        }
        return bHandledByBase || bSkillSlashHitTraceEnabled;
    }

    if (NotifyName == WeaponAnimNotifyNames::StopSkillTrail())
    {
        if (bSkillSlashHitTraceEnabled)
        {
            StopAttackTrace();
        }
        return Super::OnWeaponAnimNotifyTiming(NotifyName, PlayerCharacter);
    }

    if (NotifyName == WeaponAnimNotifyNames::SpawnSkillSlash())
    {
        PlaySkillSlashVisual();
        return bSkillSlashHitTraceEnabled;
    }

    return Super::OnWeaponAnimNotifyTiming(NotifyName, PlayerCharacter);
}

void AMeleeWeapon::SetAttackTraceEnabled(const bool bEnabled)
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

void AMeleeWeapon::StartAttackTrace()
{
    TrackedAttackSectionName = NAME_None;
    StartAttackTraceInternal(true);
}

void AMeleeWeapon::StartAttackTraceForSection(const FName AttackSectionName)
{
    if (AttackSectionName.IsNone())
    {
        return;
    }

    if (TrackedAttackSectionName != AttackSectionName)
    {
        TrackedAttackSectionName = AttackSectionName;
        HitActorsInCurrentAttack.Reset();
        bHasPreviousAttackTraceSegment = false;
    }

    StartAttackTraceInternal(false);
}

void AMeleeWeapon::ResetAttackHitTracking()
{
    TrackedAttackSectionName = NAME_None;
    HitActorsInCurrentAttack.Reset();
}

void AMeleeWeapon::StartAttackTraceInternal(const bool bResetHitActors)
{
    if (!HasAuthority() || !IsCurrentWeaponForOwner() || bAttackTraceActive)
    {
        return;
    }

    bAttackTraceActive = true;
    if (bResetHitActors)
    {
        HitActorsInCurrentAttack.Reset();
    }
    bHasPreviousAttackTraceSegment = false;

    PerformAttackTrace();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            AttackTraceTimerHandle,
            this,
            &ThisClass::PerformAttackTrace,
            FMath::Max(AttackTraceInterval, UE_SMALL_NUMBER),
            true);
    }
}

void AMeleeWeapon::StopAttackTrace()
{
    bAttackTraceActive = false;
    bHasPreviousAttackTraceSegment = false;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(AttackTraceTimerHandle);
    }

    AttackTraceTimerHandle.Invalidate();
}

void AMeleeWeapon::ConfigureSkillSlash(
    UNiagaraSystem* SlashSystem,
    const FVector& SlashScale,
    const FVector& SlashSpawnLocationOffset,
    const FName SlashSpawnSocketName,
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
    ActiveSkillSlashSystem = SlashSystem;
    ActiveSkillSlashScale = SlashScale.IsNearlyZero() ? FVector::OneVector : SlashScale;
    ActiveSkillSlashSpawnLocationOffset = SlashSpawnLocationOffset;
    ActiveSkillSlashSpawnSocketName = SlashSpawnSocketName;
    ActiveSkillSlashSpawnRotationOffset = SlashSpawnRotationOffset;
    ActiveSkillAttackTraceEndMultiplier = FMath::Max(AttackTraceEndMultiplier, 1.0f);
    bSkillSlashHitTraceEnabled = bEnableHitTrace;

    ClearActiveSkillAdditionalDamage();
    ActiveSkillDebuffEffectSpecHandle = DebuffEffectSpecHandle;
    ActiveSkillStatusEffectDefinition = StatusEffectDefinition;

    if (AdditionalDamageEffectClass && AdditionalDamageMagnitude > 0.0f)
    {
        ActiveSkillAdditionalDamageEffectClass = AdditionalDamageEffectClass;
        ActiveSkillAdditionalDamageDataTag = AdditionalDamageDataTag;
        ActiveSkillAdditionalDamageMagnitude = FMath::Max(AdditionalDamageMagnitude, 0.0f);
        ActiveSkillAdditionalDamageLevel = FMath::Max(AdditionalDamageLevel, 1);
        ActiveSkillAdditionalDamageSourceObject = AdditionalDamageSourceObject;
        ActiveSkillAdditionalDamageDelay = FMath::Max(AdditionalDamageDelay, 0.0f);
    }
}

void AMeleeWeapon::PlaySkillSlashVisual()
{
    const ACharacterBase* OwningCharacter = GetOwningCharacter();
    const bool bCanPredictForOwningClient = !HasAuthority()
        && OwningCharacter
        && OwningCharacter->IsLocallyControlled();

    if ((!HasAuthority() && !bCanPredictForOwningClient)
        || !IsCurrentWeaponForOwner()
        || !bSkillSlashHitTraceEnabled
        || !ActiveSkillSlashSystem)
    {
        return;
    }

    SpawnSkillSlashNiagara();
}

void AMeleeWeapon::ClearSkillSlash()
{
    StopAttackTrace();
    ActiveSkillSlashSystem = nullptr;
    ActiveSkillSlashScale = FVector::OneVector;
    ActiveSkillSlashSpawnLocationOffset = FVector::ZeroVector;
    ActiveSkillSlashSpawnSocketName = NAME_None;
    ActiveSkillSlashSpawnRotationOffset = FRotator::ZeroRotator;
    ActiveSkillAttackTraceEndMultiplier = 1.0f;
    bSkillSlashHitTraceEnabled = false;
    ClearActiveSkillAdditionalDamage();
    ActiveSkillDebuffEffectSpecHandle = FGameplayEffectSpecHandle();
    ActiveSkillStatusEffectDefinition = nullptr;
}

void AMeleeWeapon::SetTemporaryAttackTraceEndZMultiplier(UObject* SourceObject, const float Multiplier)
{
    if (!SourceObject)
    {
        return;
    }

    const FObjectKey SourceKey(SourceObject);
    const float ClampedMultiplier = FMath::Max(Multiplier, 1.0f);
    if (ClampedMultiplier <= 1.0f)
    {
        TemporaryAttackTraceEndZMultipliers.Remove(SourceKey);
        return;
    }

    TemporaryAttackTraceEndZMultipliers.Add(SourceKey, ClampedMultiplier);
}

void AMeleeWeapon::ClearTemporaryAttackTraceEndZMultiplier(UObject* SourceObject)
{
    if (SourceObject)
    {
        TemporaryAttackTraceEndZMultipliers.Remove(FObjectKey(SourceObject));
    }
}

float AMeleeWeapon::GetTemporaryAttackTraceEndZMultiplier() const
{
    float ActiveMultiplier = 1.0f;
    for (const TPair<FObjectKey, float>& Entry : TemporaryAttackTraceEndZMultipliers)
    {
        ActiveMultiplier = FMath::Max(ActiveMultiplier, Entry.Value);
    }
    return ActiveMultiplier;
}

void AMeleeWeapon::MulticastSpawnSkillSlashNiagara_Implementation(
    UNiagaraSystem* SlashSystem,
    const FVector& SpawnLocation,
    const FRotator& SpawnRotation,
    const FVector& SpawnScale)
{
    if (!SlashSystem || ConsumeMatchingPredictedSkillSlash(SlashSystem, SpawnLocation))
    {
        return;
    }

    SpawnSkillSlashNiagaraLocal(SlashSystem, SpawnLocation, SpawnRotation, SpawnScale);
}

bool AMeleeWeapon::CanDamageMeleeTracedHit(const FHitResult& HitResult) const
{
    const ACharacterBase* TargetCharacter = PdCharacterHitValidation::ResolveMeleeWeaponDamageHit(
        HitResult.GetActor(),
        HitResult.GetComponent());
    const ACharacterBase* SourceCharacter = GetOwningCharacter();
    if (!TargetCharacter || !SourceCharacter || TargetCharacter == SourceCharacter)
    {
        return false;
    }

    if (!SourceCharacter->CanDamageCharacterByTeam(TargetCharacter))
    {
        return false;
    }

    return !HitActorsInCurrentAttack.Contains(TargetCharacter);
}

FVector AMeleeWeapon::GetAttackTraceEndLocation(const FVector& TraceStartLocation) const
{
    const FVector RawTraceEndLocation = AttackTraceEnd
        ? AttackTraceEnd->GetComponentLocation()
        : TraceStartLocation;
    const float TraceEndZMultiplier = GetTemporaryAttackTraceEndZMultiplier();

    FVector TraceVector = RawTraceEndLocation - TraceStartLocation;
    if (!TraceVector.IsNearlyZero() && TraceEndZMultiplier > 1.0f)
    {
        TraceVector.Z *= TraceEndZMultiplier;
    }

    const FVector ZAdjustedTraceEndLocation = TraceStartLocation + TraceVector;
    if (!bSkillSlashHitTraceEnabled || ActiveSkillAttackTraceEndMultiplier <= 1.0f)
    {
        return ZAdjustedTraceEndLocation;
    }

    const FVector SkillTraceVector = ZAdjustedTraceEndLocation - TraceStartLocation;
    return SkillTraceVector.IsNearlyZero()
        ? ZAdjustedTraceEndLocation
        : TraceStartLocation + SkillTraceVector * ActiveSkillAttackTraceEndMultiplier;
}

void AMeleeWeapon::PerformAttackTrace()
{
    if (!HasAuthority() || !AttackTraceStart || !AttackTraceEnd)
    {
        return;
    }

    if (!IsCurrentWeaponForOwner())
    {
        StopAttackTrace();
        return;
    }

    const FVector TraceStartLocation = AttackTraceStart->GetComponentLocation();
    const FVector TraceEndLocation = GetAttackTraceEndLocation(TraceStartLocation);
    if (TraceStartLocation.Equals(TraceEndLocation, KINDA_SMALL_NUMBER))
    {
        return;
    }

    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(LabCollisionChannels::HitableBody()));

    TArray<TEnumAsByte<EObjectTypeQuery>> CapsuleObjectTypes;
    CapsuleObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);
    if (AActor* ParentActor = GetAttachParentActor())
    {
        ActorsToIgnore.Add(ParentActor);
    }
    if (APawn* OwnerInstigator = GetInstigator())
    {
        ActorsToIgnore.Add(OwnerInstigator);
    }
    if (ACharacterBase* SourceCharacter = GetOwningCharacter())
    {
        ActorsToIgnore.Add(SourceCharacter);
    }

    TArray<FVector> DebugStartLocations;
    TArray<FVector> DebugEndLocations;
    TArray<FHitResult> DebugHitResults;

    auto TraceAttackLine = [this, &ObjectTypes, &CapsuleObjectTypes, &ActorsToIgnore,
        &DebugStartLocations, &DebugEndLocations, &DebugHitResults](
        const FVector& LineStart,
        const FVector& LineEnd)
    {
        TArray<FHitResult> HitResults;
        const float TraceRadius = FMath::Clamp(AttackTraceRadius, 0.0f, 20.0f);

        if (TraceRadius > UE_SMALL_NUMBER)
        {
            UKismetSystemLibrary::SphereTraceMultiForObjects(
                this, LineStart, LineEnd, TraceRadius, ObjectTypes, false, ActorsToIgnore,
                EDrawDebugTrace::None, HitResults, true, FLinearColor::Red, FLinearColor::Green, 0.1f);
        }
        else
        {
            UKismetSystemLibrary::LineTraceMultiForObjects(
                this, LineStart, LineEnd, ObjectTypes, false, ActorsToIgnore,
                EDrawDebugTrace::None, HitResults, true, FLinearColor::Red, FLinearColor::Green, 0.1f);
        }

        TArray<FHitResult> CapsuleHitResults;
        if (TraceRadius > UE_SMALL_NUMBER)
        {
            UKismetSystemLibrary::SphereTraceMultiForObjects(
                this, LineStart, LineEnd, TraceRadius, CapsuleObjectTypes, false, ActorsToIgnore,
                EDrawDebugTrace::None, CapsuleHitResults, true, FLinearColor::Red, FLinearColor::Green, 0.1f);
        }
        else
        {
            UKismetSystemLibrary::LineTraceMultiForObjects(
                this, LineStart, LineEnd, CapsuleObjectTypes, false, ActorsToIgnore,
                EDrawDebugTrace::None, CapsuleHitResults, true, FLinearColor::Red, FLinearColor::Green, 0.1f);
        }

        HitResults.Append(CapsuleHitResults);
        DebugStartLocations.Add(LineStart);
        DebugEndLocations.Add(LineEnd);
        DebugHitResults.Append(HitResults);

        for (const FHitResult& HitResult : HitResults)
        {
            ACharacterBase* TargetCharacter = PdCharacterHitValidation::ResolveMeleeWeaponDamageHit(
                HitResult.GetActor(),
                HitResult.GetComponent());
            if (!TargetCharacter || !CanDamageMeleeTracedHit(HitResult))
            {
                continue;
            }

            HitActorsInCurrentAttack.Add(TargetCharacter);
            ApplyDamageFromAuthoritativeMeleeTrace(HitResult);
        }
    };

    if (!bHasPreviousAttackTraceSegment)
    {
        TraceAttackLine(TraceStartLocation, TraceEndLocation);

        if (IsAttackDebugVisualizationEnabled())
        {
            MulticastDrawInterpolatedAttackTraceDebug(DebugStartLocations, DebugEndLocations, DebugHitResults);
        }

        PreviousAttackTraceStartLocation = TraceStartLocation;
        PreviousAttackTraceEndLocation = TraceEndLocation;
        bHasPreviousAttackTraceSegment = true;
        return;
    }

    const float StartTravelDistance = FVector::Distance(PreviousAttackTraceStartLocation, TraceStartLocation);
    const float EndTravelDistance = FVector::Distance(PreviousAttackTraceEndLocation, TraceEndLocation);
    const float MaxTravelDistance = FMath::Max(StartTravelDistance, EndTravelDistance);
    const int32 InterpolationCount = FMath::Max(
        1,
        FMath::CeilToInt(MaxTravelDistance / AttackTraceInterpolationDistance));

    for (int32 InterpolationIndex = 1; InterpolationIndex <= InterpolationCount; ++InterpolationIndex)
    {
        const float Alpha = static_cast<float>(InterpolationIndex) / static_cast<float>(InterpolationCount);
        const FVector InterpolatedStartLocation = FMath::Lerp(
            PreviousAttackTraceStartLocation,
            TraceStartLocation,
            Alpha);
        const FVector InterpolatedEndLocation = FMath::Lerp(
            PreviousAttackTraceEndLocation,
            TraceEndLocation,
            Alpha);
        TraceAttackLine(InterpolatedStartLocation, InterpolatedEndLocation);
    }

    if (IsAttackDebugVisualizationEnabled())
    {
        MulticastDrawInterpolatedAttackTraceDebug(DebugStartLocations, DebugEndLocations, DebugHitResults);
    }

    PreviousAttackTraceStartLocation = TraceStartLocation;
    PreviousAttackTraceEndLocation = TraceEndLocation;
    bHasPreviousAttackTraceSegment = true;
}

bool AMeleeWeapon::HasActiveSkillAdditionalDamage() const
{
    return ActiveSkillAdditionalDamageEffectClass && ActiveSkillAdditionalDamageMagnitude > 0.0f;
}

void AMeleeWeapon::ApplyActiveSkillAdditionalDamageToTarget(ACharacterBase* TargetCharacter)
{
    if (!HasAuthority() || !HasActiveSkillAdditionalDamage())
    {
        return;
    }

    const TWeakObjectPtr<ACharacterBase> TargetWeak(TargetCharacter);
    const TSubclassOf<UGameplayEffect> DamageEffectClass = ActiveSkillAdditionalDamageEffectClass;
    const FGameplayTag DamageDataTag = ActiveSkillAdditionalDamageDataTag;
    const float DamageMagnitude = ActiveSkillAdditionalDamageMagnitude;
    const int32 DamageLevel = ActiveSkillAdditionalDamageLevel;
    const TWeakObjectPtr<UObject> DamageSourceObjectWeak(ActiveSkillAdditionalDamageSourceObject.Get());
    const FGameplayEffectSpecHandle DebuffEffectSpecHandle = ActiveSkillDebuffEffectSpecHandle;
    const TWeakObjectPtr<UStatusEffectDefinition> StatusEffectDefinitionWeak(ActiveSkillStatusEffectDefinition.Get());
    const float DamageDelay = FMath::Max(ActiveSkillAdditionalDamageDelay, 0.0f);

    if (DamageDelay > 0.0f)
    {
        if (UWorld* World = GetWorld())
        {
            FTimerHandle DelayHandle;
            World->GetTimerManager().SetTimer(
                DelayHandle,
                FTimerDelegate::CreateWeakLambda(this,
                    [this, TargetWeak, DamageEffectClass, DamageDataTag, DamageMagnitude, DamageLevel,
                        DamageSourceObjectWeak, DebuffEffectSpecHandle, StatusEffectDefinitionWeak]()
                    {
                        ApplySkillAdditionalDamageToTarget(
                            TargetWeak.Get(),
                            DamageEffectClass,
                            DamageDataTag,
                            DamageMagnitude,
                            DamageLevel,
                            DamageSourceObjectWeak.Get(),
                            DebuffEffectSpecHandle,
                            StatusEffectDefinitionWeak.Get());
                    }),
                DamageDelay,
                false);
            return;
        }
    }

    ApplySkillAdditionalDamageToTarget(
        TargetCharacter,
        DamageEffectClass,
        DamageDataTag,
        DamageMagnitude,
        DamageLevel,
        DamageSourceObjectWeak.Get(),
        DebuffEffectSpecHandle,
        StatusEffectDefinitionWeak.Get());
}

void AMeleeWeapon::ApplySkillAdditionalDamageToTarget(
    ACharacterBase* TargetCharacter,
    TSubclassOf<UGameplayEffect> DamageEffectClass,
    FGameplayTag DamageDataTag,
    float DamageMagnitude,
    int32 DamageLevel,
    UObject* DamageSourceObject,
    const FGameplayEffectSpecHandle& DebuffEffectSpecHandle,
    UStatusEffectDefinition* StatusEffectDefinition)
{
    if (!HasAuthority() || !DamageEffectClass || DamageMagnitude <= 0.0f)
    {
        return;
    }

    ACharacterBase* SourceCharacter = GetOwningCharacter();
    if (!SourceCharacter || !TargetCharacter || SourceCharacter == TargetCharacter)
    {
        return;
    }

    UPdAbilitySystemComponent* SourceASC = SourceCharacter->GetPdAbilitySystemComponent();
    UPdAbilitySystemComponent* TargetASC = TargetCharacter->GetPdAbilitySystemComponent();
    if (!SourceASC || !TargetASC)
    {
        return;
    }

    FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
    EffectContext.AddInstigator(SourceCharacter, this);
    EffectContext.AddSourceObject(DamageSourceObject ? DamageSourceObject : static_cast<UObject*>(this));

    FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(
        DamageEffectClass,
        FMath::Max(DamageLevel, 1),
        EffectContext);
    if (!DamageSpecHandle.IsValid() || !DamageSpecHandle.Data.IsValid())
    {
        return;
    }

    if (!DamageDataTag.IsValid())
    {
        DamageDataTag = UProjectTagDefinition::GetDefaultConfig()->GetSetByCallerDamageMagnitudeTag();
    }
    if (!DamageDataTag.IsValid())
    {
        return;
    }

    DamageSpecHandle.Data->SetSetByCallerMagnitude(DamageDataTag, DamageMagnitude);
    const FActiveGameplayEffectHandle AppliedHandle = SourceASC->ApplyGameplayEffectSpecToTarget(
        *DamageSpecHandle.Data.Get(),
        TargetASC);
    if (AppliedHandle.WasSuccessfullyApplied())
    {
        ApplySkillDebuffToTarget(TargetCharacter, DebuffEffectSpecHandle, StatusEffectDefinition);
    }
}

void AMeleeWeapon::ClearActiveSkillAdditionalDamage()
{
    ActiveSkillAdditionalDamageEffectClass = nullptr;
    ActiveSkillAdditionalDamageDataTag = FGameplayTag();
    ActiveSkillAdditionalDamageMagnitude = 0.0f;
    ActiveSkillAdditionalDamageLevel = 1;
    ActiveSkillAdditionalDamageSourceObject = nullptr;
    ActiveSkillAdditionalDamageDelay = 0.12f;
}

void AMeleeWeapon::SpawnSkillSlashNiagara()
{
    if (!bSkillSlashHitTraceEnabled || !ActiveSkillSlashSystem)
    {
        return;
    }

    const bool bHasTraceComponents = AttackTraceStart && AttackTraceEnd;
    const FVector TraceStartLocation = AttackTraceStart
        ? AttackTraceStart->GetComponentLocation()
        : FVector::ZeroVector;
    const FVector RawTraceEndLocation = AttackTraceEnd
        ? AttackTraceEnd->GetComponentLocation()
        : TraceStartLocation;
    const FVector TraceEndLocation = bHasTraceComponents
        ? GetAttackTraceEndLocation(TraceStartLocation)
        : RawTraceEndLocation;

    FRotator SlashRotation = ActiveSkillSlashSpawnRotationOffset;
    FVector SlashBaseLocation = RawTraceEndLocation;
    FVector SlashLocation = SlashBaseLocation + SlashRotation.RotateVector(ActiveSkillSlashSpawnLocationOffset);
    FVector SlashScale = ActiveSkillSlashScale;
    bool bUsedSocketTransform = false;

    if (!ActiveSkillSlashSpawnSocketName.IsNone())
    {
        ACharacterBase* OwnerCharacter = GetOwningCharacter();
        USkeletalMeshComponent* OwnerMesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
        if (OwnerMesh && OwnerMesh->DoesSocketExist(ActiveSkillSlashSpawnSocketName))
        {
            const FTransform SocketTransform = OwnerMesh->GetSocketTransform(
                ActiveSkillSlashSpawnSocketName,
                RTS_World);
            SlashBaseLocation = SocketTransform.GetLocation();
            SlashLocation = SocketTransform.TransformPosition(ActiveSkillSlashSpawnLocationOffset);
            SlashRotation = (SocketTransform.GetRotation().Rotator() + ActiveSkillSlashSpawnRotationOffset).GetNormalized();
            SlashScale = ActiveSkillSlashScale * SocketTransform.GetScale3D();
            bUsedSocketTransform = true;
        }
    }

    if (!bUsedSocketTransform)
    {
        if (!bHasTraceComponents)
        {
            return;
        }

        FVector TraceVector = RawTraceEndLocation - TraceStartLocation;
        if (TraceVector.IsNearlyZero())
        {
            TraceVector = TraceEndLocation - TraceStartLocation;
        }
        if (TraceVector.IsNearlyZero())
        {
            return;
        }

        SlashRotation = (TraceVector.GetSafeNormal().Rotation() + ActiveSkillSlashSpawnRotationOffset).GetNormalized();
        SlashBaseLocation = RawTraceEndLocation;
        SlashLocation = SlashBaseLocation + SlashRotation.RotateVector(ActiveSkillSlashSpawnLocationOffset);
    }

    if (HasAuthority())
    {
        MulticastSpawnSkillSlashNiagara(ActiveSkillSlashSystem, SlashLocation, SlashRotation, SlashScale);
        return;
    }

    if (SpawnSkillSlashNiagaraLocal(ActiveSkillSlashSystem, SlashLocation, SlashRotation, SlashScale))
    {
        PredictedSkillSlashSystem = ActiveSkillSlashSystem;
        PredictedSkillSlashLocation = SlashLocation;
        PredictedSkillSlashWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
        bHasPendingPredictedSkillSlash = true;
    }
}

bool AMeleeWeapon::SpawnSkillSlashNiagaraLocal(
    UNiagaraSystem* SlashSystem,
    const FVector& SpawnLocation,
    const FRotator& SpawnRotation,
    const FVector& SpawnScale)
{
    if (!SlashSystem || GetNetMode() == NM_DedicatedServer)
    {
        return false;
    }

    const FVector EffectiveScale = SpawnScale.IsNearlyZero() ? FVector::OneVector : SpawnScale;
    return IsValid(UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        this,
        SlashSystem,
        SpawnLocation,
        SpawnRotation,
        EffectiveScale,
        true,
        true,
        ENCPoolMethod::AutoRelease,
        true));
}

bool AMeleeWeapon::ConsumeMatchingPredictedSkillSlash(
    UNiagaraSystem* SlashSystem,
    const FVector& SpawnLocation)
{
    if (HasAuthority() || !bHasPendingPredictedSkillSlash)
    {
        return false;
    }

    const ACharacterBase* OwningCharacter = GetOwningCharacter();
    const UWorld* World = GetWorld();
    const double CurrentWorldTime = World ? World->GetTimeSeconds() : -1.0;
    const bool bPredictionStillRecent = CurrentWorldTime >= 0.0
        && PredictedSkillSlashWorldTime >= 0.0
        && CurrentWorldTime - PredictedSkillSlashWorldTime <= 1.0;
    const bool bMatchesPrediction = OwningCharacter
        && OwningCharacter->IsLocallyControlled()
        && bPredictionStillRecent
        && PredictedSkillSlashSystem.Get() == SlashSystem
        && PredictedSkillSlashLocation.Equals(SpawnLocation, 100.0);

    if (bMatchesPrediction || !bPredictionStillRecent)
    {
        bHasPendingPredictedSkillSlash = false;
        PredictedSkillSlashSystem.Reset();
        PredictedSkillSlashWorldTime = -1.0;
    }

    return bMatchesPrediction;
}

void AMeleeWeapon::MulticastDrawInterpolatedAttackTraceDebug_Implementation(
    const TArray<FVector>& StartLocations,
    const TArray<FVector>& EndLocations,
    const TArray<FHitResult>& Hits)
{
    DrawInterpolatedAttackTraceDebug(StartLocations, EndLocations, Hits);
}

void AMeleeWeapon::DrawInterpolatedAttackTraceDebug(
    const TArray<FVector>& StartLocations,
    const TArray<FVector>& EndLocations,
    const TArray<FHitResult>& Hits) const
{
    UWorld* World = GetWorld();
    if (!IsAttackDebugVisualizationEnabled() || !World)
    {
        return;
    }

    const FColor TraceColor = AttackTraceDebugTraceColor.ToFColor(true);
    const FColor HitColor = AttackTraceDebugHitColor.ToFColor(true);
    const FColor SweepColor = Hits.IsEmpty() ? TraceColor : HitColor;
    const float DrawTime = FMath::Max(0.0f, AttackTraceDebugDrawTime);
    const int32 LineCount = FMath::Min(StartLocations.Num(), EndLocations.Num());

    for (int32 LineIndex = 0; LineIndex < LineCount; ++LineIndex)
    {
        DrawDebugLine(
            World,
            StartLocations[LineIndex],
            EndLocations[LineIndex],
            SweepColor,
            false,
            DrawTime,
            0,
            2.0f);
    }

    for (const FHitResult& Hit : Hits)
    {
        if (!Hit.GetActor())
        {
            continue;
        }

        const FVector HitLocation = Hit.ImpactPoint.IsNearlyZero() ? Hit.Location : Hit.ImpactPoint;
        DrawDebugSphere(World, HitLocation, 8.0f, 12, HitColor, false, DrawTime, 0, 3.0f);
    }
}

bool AMeleeWeapon::ApplyDamageFromAuthoritativeMeleeTrace(const FHitResult& HitResult)
{
    if (!HasAuthority() || !IsCurrentWeaponForOwner())
    {
        return false;
    }

    ACharacterBase* TargetCharacter = PdCharacterHitValidation::ResolveMeleeWeaponDamageHit(
        HitResult.GetActor(),
        HitResult.GetComponent());
    return TargetCharacter && ApplyDamageToTarget(TargetCharacter);
}

void AMeleeWeapon::ApplySkillDebuffToTarget(
    ACharacterBase* TargetCharacter,
    const FGameplayEffectSpecHandle& DebuffEffectSpecHandle,
    UStatusEffectDefinition* StatusEffectDefinition)
{
    if (!HasAuthority()
        || !TargetCharacter
        || !DebuffEffectSpecHandle.IsValid()
        || !DebuffEffectSpecHandle.Data.IsValid())
    {
        return;
    }

    ACharacterBase* SourceCharacter = GetOwningCharacter();
    UPdAbilitySystemComponent* SourceASC = SourceCharacter
        ? SourceCharacter->GetPdAbilitySystemComponent()
        : nullptr;
    UPdAbilitySystemComponent* TargetASC = TargetCharacter->GetPdAbilitySystemComponent();
    if (!SourceASC || !TargetASC || !StatusEffectDefinition || !StatusEffectDefinition->CanStack(TargetASC))
    {
        return;
    }

    const FActiveGameplayEffectHandle AppliedHandle = SourceASC->ApplyGameplayEffectSpecToTarget(
        *DebuffEffectSpecHandle.Data.Get(),
        TargetASC);
    if (!AppliedHandle.WasSuccessfullyApplied())
    {
        return;
    }

    if (UStatusEffectReplicationComponent* ReplicationComponent = TargetCharacter->GetStatusEffectReplicationComponent())
    {
        ReplicationComponent->TrackAppliedStatusEffect(StatusEffectDefinition, AppliedHandle);
    }
}

bool AMeleeWeapon::ApplyDamageToTarget(AActor* TargetActor)
{
    const bool bAppliedDamage = Super::ApplyDamageToTarget(TargetActor);
    if (bAppliedDamage)
    {
        ApplyActiveSkillAdditionalDamageToTarget(Cast<ACharacterBase>(TargetActor));
    }
    return bAppliedDamage;
}
