#include "Skill/Actions/SkillProjectileCastAction.h"

#include "AbilitySystem/Ability/SkillAbility.h"

#include "Abilities/GameplayAbilityTargetActor_SingleLineTrace.h"
#include "Abilities/GameplayAbilityTargetActor_GroundTrace.h"
#include "Abilities/GameplayAbilityTargetActor_Trace.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitConfirmCancel.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Skill/Actors/SkillProjectile.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "AbilitySystem/TargetValidator.h"
#include "AbilitySystem/TargetingActors/GroundTargetActor.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Character/PdPlayer.h"
#include "Common/LabGameplayTags.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillProjectileCastAction)

DEFINE_LOG_CATEGORY_STATIC(LogProjectileAbility, Log, All);

USkillProjectileCastAction::USkillProjectileCastAction()
{
    Settings.FireEventTag = LabGameplayTags::Event_ShootProjectile;
    Settings.ProjectileActorClass = ASkillProjectile::StaticClass();
    Settings.ProjectileSpeed = 2000.0;
    Settings.ProjectileRadius = 50.0;
    Settings.SpawnLocationOffset = FVector(0.0, 0.0, 80.0);
    Settings.MinimumForwardSpawnOffset = 140.0;
    Settings.TargetTraceMaxRange = 999999.0;
    Settings.TargetDecalSize = 512.0;
    Settings.GroundTargetActorClass = AGroundTargetActor::StaticClass();
}

void USkillProjectileCastAction::OnStart()
{
    bEndAfterProjectileFired = false;
    bPausedForPlayerAim = false;
    bPlayerProjectileConfirmed = false;
    bProjectileExecutionRequested = false;
    bProjectileSpawnSucceeded = false;
    bWaitingForPlayerConfirm = false;
    bCleaningUpTargetDataTask = false;
    ReadiedProjectile = nullptr;

    const USkillDefinition* Skill = GetAbility()->GetSourceSkillDataAsset();
    if (!Skill || !Settings.ProjectileActorClass || GetConfiguredProjectileSpeed() <= 0.0f)
    {
        Finish(false);
        return;
    }

    StartProjectileCast();
}

void USkillProjectileCastAction::OnStop()
{
    CleanupAimingState();
    ClearSocketBarrageState(true);
}

void USkillProjectileCastAction::HandleMontageFinished()
{
    if (!bProjectileExecutionRequested)
    {
       if (GetAbility()->HasPlayerController() && !bPlayerProjectileConfirmed)
       {
          if (!bWaitingForPlayerConfirm)
          {
             StartPlayerAiming();
          }
          return;
       }
       FireAtDefaultTarget();
       return;
    }

    if (IsSocketBarrageActive() || ShouldWaitForServerSocketBarrageEnd())
    {
       bSocketBarrageEndAbilityAfterFire = true;
       return;
    }
    FinishCast();
}

void USkillProjectileCastAction::HandleShootProjectileEvent(FGameplayEventData Payload)
{
    static_cast<void>(Payload);

    if (!GetAbility()->HasPlayerController())
    {
        FireAtCurrentTargetOrFallback();
        return;
    }

    if (bPlayerProjectileConfirmed || bPausedForPlayerAim)
    {
        return;
    }

    bPausedForPlayerAim = true;
    PauseProjectileMontageForAiming();
    StartPlayerAiming();
}

void USkillProjectileCastAction::FireAtCurrentTargetOrFallback()
{
    if (!(IsRunning() && GetAbility()->CanRunActions()))
    {
        return;
    }

    AActor* AttackTarget = GetAbility()->GetAttackTargetFromAvatar();
    if (!IsValid(AttackTarget))
    {
        FireAtDefaultTarget();
        return;
    }

    if (ExecuteProjectileShot(AttackTarget->GetActorLocation()))
    {
        TryFinishAfterProjectileFired();
    }
}

void USkillProjectileCastAction::HandleConfirmPressed()
{
    if (!bWaitingForPlayerConfirm)
    {
       return;
    }
    ConfirmPlayerShot();
}

void USkillProjectileCastAction::HandleCancelPressed()
{
    if (!GetAbility()->CanBeCanceled())
    {
       GetAbility()->SetCanBeCanceled(true);
    }
    Finish(false);
}

void USkillProjectileCastAction::StartPlayerAiming()
{
    bWaitingForPlayerConfirm = true;
    if (const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
       SkillDataAsset && SkillDataAsset->Movement.bLockMovementDuringDuration)
    {
       GetAbility()->LockAvatarMovementForAbility();
    }
    SpawnReadiedProjectile();

    if (ConfirmCancelTask)
    {
       ConfirmCancelTask->EndTask();
       ConfirmCancelTask = nullptr;
    }

    if (Settings.bUseGroundTargeting)
    {
       WaitForPlayerTargetData();
       return;
    }

    ConfirmCancelTask = UAbilityTask_WaitConfirmCancel::WaitConfirmCancel(GetAbility());
    if (!ConfirmCancelTask)
    {
       Finish(false);
       return;
    }

    ConfirmCancelTask->OnConfirm.AddDynamic(this, &ThisClass::HandleConfirmPressed);
    ConfirmCancelTask->OnCancel.AddDynamic(this, &ThisClass::HandleCancelPressed);
    ConfirmCancelTask->ReadyForActivation();
}

void USkillProjectileCastAction::StartProjectileCast()
{
    if (!GetAbility()->CommitSkill())
    {
        Finish(false);
        return;
    }

    GetAbility()->LockAvatarMovementForAbility();
    GetAbility()->SpawnConfiguredCharacterDecal();

    if (Settings.FireMode == EProjectileFireMode::Immediate)
    {
        bEndAfterProjectileFired = true;
        if (ExecuteProjectileShot(ResolveDefaultTargetLocation()))
        {
            TryFinishAfterProjectileFired();
        }
        return;
    }

    UAnimMontage* ShootMontage = GetConfiguredShootMontage();
    if (!ShootMontage)
    {
        StartShotWithoutMontage();
        return;
    }

    StartShootProjectileEventTask();

    ShootMontageTask = GetAbility()->CreateDefaultMontageAndWaitTask(ShootMontage);
    if (!ShootMontageTask)
    {
        StartShotWithoutMontage();
        return;
    }

    ShootMontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageFinished);
    ShootMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageFinished);
    ShootMontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageFinished);
    ShootMontageTask->ReadyForActivation();
}

void USkillProjectileCastAction::StartShotWithoutMontage()
{
    bEndAfterProjectileFired = true;

    if (GetAbility()->HasPlayerController())
    {
        StartPlayerAiming();
        return;
    }

    FireAtCurrentTargetOrFallback();
}

void USkillProjectileCastAction::ConfirmPlayerShot()
{
    bWaitingForPlayerConfirm = false;
    bPlayerProjectileConfirmed = true;
    GetAbility()->RestoreAvatarMovementForAbility();

    if (ConfirmCancelTask)
    {
        ConfirmCancelTask->EndTask();
        ConfirmCancelTask = nullptr;
    }

    if (!bPausedForPlayerAim)
    {
        bEndAfterProjectileFired = true;
    }

    const bool bNeedsTargetData = Settings.bUseGroundTargeting
        || Settings.TargetTraceProfile.Name != TEXT("NoCollision");

    if (bNeedsTargetData)
    {
        WaitForPlayerTargetData();
    }
    else if (ExecuteProjectileShot(ResolveDefaultTargetLocation()))
    {
        TryFinishAfterProjectileFired();
    }

    if (bPausedForPlayerAim)
    {
        ResumeProjectileMontageAfterAiming();
        bPausedForPlayerAim = false;
    }
}

bool USkillProjectileCastAction::ExecuteProjectileShot(FVector TargetLocation)
{
    if (bProjectileExecutionRequested)
    {
       return true;
    }

    bProjectileExecutionRequested = true;
    ShootProjectile(TargetLocation);

    const AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
    if (!AvatarActor || !AvatarActor->HasAuthority() || bProjectileSpawnSucceeded)
    {
       return true;
    }

    UE_LOG(
       LogProjectileAbility,
       Error,
       TEXT("Projectile skill %s committed but failed to spawn its authoritative projectile."),
       *GetNameSafe(GetAbility()->GetSourceSkillDataAsset()));
    Finish(false);
    return false;
}

void USkillProjectileCastAction::FireAtDefaultTarget()
{
    if (!(IsRunning() && GetAbility()->CanRunActions()))
    {
        return;
    }

    if (GetAbility()->HasPlayerController() && !bPlayerProjectileConfirmed)
    {
        return;
    }

    bWaitingForPlayerConfirm = false;
    bPlayerProjectileConfirmed = true;
    bEndAfterProjectileFired = true;
    GetAbility()->RestoreAvatarMovementForAbility();

    if (ExecuteProjectileShot(ResolveDefaultTargetLocation()))
    {
        TryFinishAfterProjectileFired();
    }
}

void USkillProjectileCastAction::TryFinishAfterProjectileFired()
{
    if (!bEndAfterProjectileFired)
    {
        return;
    }

    if (IsSocketBarrageActive() || ShouldWaitForServerSocketBarrageEnd())
    {
        return;
    }

    FinishCast();
}

void USkillProjectileCastAction::HandleTargetDataValid(const FGameplayAbilityTargetDataHandle& Data)
{
    const FGameplayAbilityActorInfo* ActorInfo = GetAbility()->GetCurrentActorInfo();
    if (!ActorInfo)
    {
        FireAtDefaultTarget();
        return;
    }

    const bool bUsingGroundTargeting = Settings.bUseGroundTargeting;
    const bool bAuthority = ActorInfo->IsNetAuthority();

    if (bUsingGroundTargeting && bWaitingForPlayerConfirm)
    {
        bWaitingForPlayerConfirm = false;
        bPlayerProjectileConfirmed = true;
        GetAbility()->RestoreAvatarMovementForAbility();

        if (!bPausedForPlayerAim)
        {
            bEndAfterProjectileFired = true;
        }
    }

    const FGameplayAbilityTargetData* TargetData = Data.Get(0);
    const FHitResult* ClientHitResult = TargetData ? TargetData->GetHitResult() : nullptr;
    if (!ClientHitResult)
    {
        FireAtDefaultTarget();
        return;
    }

    const FVector TargetDataEndPoint = UAbilitySystemBlueprintLibrary::GetTargetDataEndPoint(Data, 0);
    FVector TargetLocation = FVector::ZeroVector;

    if (bAuthority)
    {
        if (!TryValidateServerProjectileTargetLocation(
            *ClientHitResult,
            TargetDataEndPoint,
            bUsingGroundTargeting,
            TargetLocation))
        {
            FireAtDefaultTarget();
            return;
        }
    }
    else if (!PdTargetValidator::TryResolveTargetDataLocation(
        *ClientHitResult,
        TargetDataEndPoint,
        TargetLocation))
    {
        FireAtDefaultTarget();
        return;
    }
    else if (bUsingGroundTargeting)
    {
        TargetLocation.Z += GetConfiguredProjectileRadius();
    }

    if (!bAuthority && TargetLocation.IsNearlyZero())
    {
        TryResolveProjectileAimTargetLocation(TargetLocation);
    }
    else if (!bAuthority && !bUsingGroundTargeting && ShouldRetargetUsingAim(TargetLocation))
    {
        FVector AimTargetLocation = FVector::ZeroVector;
        if (TryResolveProjectileAimTargetLocation(AimTargetLocation))
        {
            TargetLocation = AimTargetLocation;
        }
    }

    const bool bShotExecuted = ExecuteProjectileShot(TargetLocation);

    if (bUsingGroundTargeting && bPausedForPlayerAim)
    {
        ResumeProjectileMontageAfterAiming();
        bPausedForPlayerAim = false;
    }

    if (bShotExecuted)
    {
        TryFinishAfterProjectileFired();
    }
}

bool USkillProjectileCastAction::TryValidateServerProjectileTargetLocation(
    const FHitResult& ClientHitResult,
    const FVector& TargetDataEndPoint,
    const bool bUsingGroundTargeting,
    FVector& OutValidatedLocation) const
{
    AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
    UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
    if (!AvatarActor || !AvatarActor->HasAuthority() || !World)
    {
       return false;
    }

    FVector RequestedLocation = FVector::ZeroVector;
    if (!PdTargetValidator::TryResolveTargetDataLocation(
       ClientHitResult,
       TargetDataEndPoint,
       RequestedLocation))
    {
       return false;
    }

    const FVector CharacterLocation = AvatarActor->GetActorLocation();
    if (bUsingGroundTargeting)
    {
       PdTargetValidator::FGroundTargetValidationParams ValidationParams;
       ValidationParams.MaxRange = GetConfiguredGroundTargetingMaxRange();
       ValidationParams.GroundTraceStartHeight = GetConfiguredGroundTargetingTraceStartHeight();
       ValidationParams.GroundTraceDepth = GetConfiguredGroundTargetingTraceDepth();
       ValidationParams.LineOfSightProfileName = Settings.GroundTargetingTraceProfile.Name;

       PdTargetValidator::FValidatedGroundTarget ValidatedTarget;
       if (!PdTargetValidator::ValidateGroundTarget(
          World,
          AvatarActor,
          CharacterLocation,
          RequestedLocation,
          ValidationParams,
          ValidatedTarget))
       {
          return false;
       }

       const FVector GroundNormal = ValidatedTarget.Normal.IsNearlyZero()
          ? FVector::UpVector
          : ValidatedTarget.Normal.GetSafeNormal();
       OutValidatedLocation = ValidatedTarget.Location
          + GroundNormal * GetConfiguredProjectileRadius();
       return true;
    }

    PdTargetValidator::FPointTargetValidationParams ValidationParams;
    ValidationParams.MaxRange = GetConfiguredTargetTraceMaxRange();
    ValidationParams.LineOfSightProfileName = Settings.TargetTraceProfile.Name;

    PdTargetValidator::FValidatedPointTarget ValidatedTarget;
    if (!PdTargetValidator::ValidatePointTarget(
       World,
       AvatarActor,
       CharacterLocation,
       GetSpawnLocation(),
       RequestedLocation,
       ValidationParams,
       ValidatedTarget))
    {
       return false;
    }

    OutValidatedLocation = ValidatedTarget.Location;
    return true;
}

void USkillProjectileCastAction::HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data)
{
    static_cast<void>(Data);

    if (bCleaningUpTargetDataTask)
    {
        return;
    }

    if (GetAbility()->HasPlayerController())
    {
        if (!bPlayerProjectileConfirmed)
        {
            Finish(false);
            return;
        }

        if (ShouldWaitForServerSocketBarrageEnd())
        {
            return;
        }

        FireAtDefaultTarget();
        return;
    }

    if (!bProjectileExecutionRequested)
    {
        FireAtDefaultTarget();
        return;
    }

    TryFinishAfterProjectileFired();
}

void USkillProjectileCastAction::StartShootProjectileEventTask()
{
    const FGameplayTag ConfiguredShootEventTag = GetConfiguredShootProjectileEventTag();
    if (!ConfiguredShootEventTag.IsValid())
    {
       return;
    }

    if (ShootProjectileEventTask)
    {
       ShootProjectileEventTask->EndTask();
       ShootProjectileEventTask = nullptr;
    }

    ShootProjectileEventTask = GetAbility()->CreateWaitGameplayEventTask(ConfiguredShootEventTag);
    if (!ShootProjectileEventTask)
    {
       return;
    }

    ShootProjectileEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleShootProjectileEvent);
    ShootProjectileEventTask->ReadyForActivation();
}

void USkillProjectileCastAction::WaitForPlayerTargetData()
{
    const bool bUsingGroundTargeting = Settings.bUseGroundTargeting;
    const TSubclassOf<AGameplayAbilityTargetActor> TargetActorClass = bUsingGroundTargeting
       ? GetConfiguredGroundTargetActorClass()
       : TSubclassOf<AGameplayAbilityTargetActor>(AGameplayAbilityTargetActor_SingleLineTrace::StaticClass());
    if (!TargetActorClass)
    {
       if (GetAbility()->HasPlayerController() && !bPlayerProjectileConfirmed)
       {
          Finish(false);
       }
       else
       {
          FireAtDefaultTarget();
       }
       return;
    }

    const FCollisionProfileName ConfiguredTargetTraceProfile = bUsingGroundTargeting
       ? Settings.GroundTargetingTraceProfile
       : Settings.TargetTraceProfile;

    if (TargetDataTask)
    {
       bCleaningUpTargetDataTask = true;
       TargetDataTask->EndTask();
       bCleaningUpTargetDataTask = false;
       TargetDataTask = nullptr;
    }

    UAbilityTask_WaitTargetData* const PendingTargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(
       GetAbility(),
       NAME_None,
       bUsingGroundTargeting ? EGameplayTargetingConfirmation::UserConfirmed : EGameplayTargetingConfirmation::Instant,
       TargetActorClass);
    TargetDataTask = PendingTargetDataTask;
    if (!PendingTargetDataTask)
    {
       if (GetAbility()->HasPlayerController() && !bPlayerProjectileConfirmed)
       {
          Finish(false);
       }
       else
       {
          FireAtDefaultTarget();
       }
       return;
    }

    PendingTargetDataTask->ValidData.AddDynamic(this, &ThisClass::HandleTargetDataValid);
    PendingTargetDataTask->Cancelled.AddDynamic(this, &ThisClass::HandleTargetDataCancelled);

    if (AGameplayAbilityTargetActor* SpawnedActor =
       GetAbility()->BeginSpawningTargetDataActor(PendingTargetDataTask, TargetActorClass))
    {
       if (AGameplayAbilityTargetActor_Trace* TraceActor = Cast<AGameplayAbilityTargetActor_Trace>(SpawnedActor))
       {
          TraceActor->MaxRange = bUsingGroundTargeting ? GetConfiguredGroundTargetingMaxRange() : GetConfiguredTargetTraceMaxRange();
          TraceActor->TraceProfile = ConfiguredTargetTraceProfile;
          TraceActor->bTraceAffectsAimPitch = bUsingGroundTargeting
             ? Settings.bGroundTargetingTraceAffectsAimPitch
             : Settings.bTraceAffectsAimPitch;
       }

       if (AGameplayAbilityTargetActor_GroundTrace* GroundTraceActor = Cast<AGameplayAbilityTargetActor_GroundTrace>(SpawnedActor))
       {
          GroundTraceActor->CollisionRadius = GetConfiguredGroundTargetingCollisionRadius();
          GroundTraceActor->CollisionHeight = GetConfiguredGroundTargetingCollisionHeight();
       }

       if (AGroundTargetActor* DecalTargetActor = Cast<AGroundTargetActor>(SpawnedActor))
       {
          DecalTargetActor->ConfigureGroundProjection(
             GetConfiguredGroundTargetingTraceStartHeight(),
             GetConfiguredGroundTargetingTraceDepth());
          DecalTargetActor->Decal = Settings.TargetDecal.Get();
          DecalTargetActor->DecalSize = GetConfiguredGroundTargetingDecalSize();
          DecalTargetActor->DecalColor = Settings.TargetDecalColor;

          float DecalStartSize = 0.0f;
          float DecalTargetSize = 0.0f;
          float DecalGrowthDuration = 0.0f;
          if (TryBuildGroundTargetingDecalGrowth(DecalStartSize, DecalTargetSize, DecalGrowthDuration))
          {
             DecalTargetActor->ConfigureDecalGrowth(DecalStartSize, DecalTargetSize, DecalGrowthDuration);
          }
       }

       SpawnedActor->StartLocation = GetAbility()->MakeTargetLocationInfoFromOwnerActor();
       SpawnedActor->bDebug = bUsingGroundTargeting ? GetConfiguredDrawGroundTargetingDebug() : GetConfiguredDrawTargetTraceDebug();
       GetAbility()->FinishSpawningTargetDataActor(PendingTargetDataTask, SpawnedActor);
    }

    // Finishing an instant target actor can synchronously broadcast target data. The callback may end this ability,
    // which cleans up TargetDataTask before FinishSpawningTargetDataActor returns. Only activate the task if this is
    // still the current task and it did not already complete during that callback.
    if (TargetDataTask == PendingTargetDataTask
       && IsValid(PendingTargetDataTask)
       && PendingTargetDataTask->GetState() == EGameplayTaskState::AwaitingActivation)
    {
       PendingTargetDataTask->ReadyForActivation();
    }
}

bool USkillProjectileCastAction::ShouldRetargetUsingAim(const FVector& TargetLocation) const
{
    const FVector SpawnLocation = GetSpawnLocation();
    const float MinimumDistance = FMath::Max(GetConfiguredMinimumTargetDistanceFromSpawn(), 0.0f);
    const bool bTooClose = MinimumDistance > 0.0f
       && FVector::DistSquared(SpawnLocation, TargetLocation) < FMath::Square(MinimumDistance);
    const bool bStronglyDownward = TargetLocation.Z < SpawnLocation.Z - 50.0f
       && FVector::DistSquared2D(SpawnLocation, TargetLocation) < FMath::Square(MinimumDistance);

    return bTooClose || bStronglyDownward;
}

bool USkillProjectileCastAction::TryResolveProjectileAimTargetLocation(FVector& OutTargetLocation) const
{
    const AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
    const float ConfiguredTargetTraceMaxRange = GetConfiguredTargetTraceMaxRange();
    if (!AvatarActor || ConfiguredTargetTraceMaxRange <= 0.0f)
    {
       return false;
    }

    FVector ViewTraceStart = FVector::ZeroVector;
    FVector AimDirection = FVector::ZeroVector;
    if (const APdPlayer* Player = Cast<APdPlayer>(AvatarActor))
    {
       Player->GetWeaponAimViewPoint(ViewTraceStart, AimDirection);
    }

    if (AimDirection.IsNearlyZero())
    {
       ViewTraceStart = GetSpawnLocation();
       AimDirection = AvatarActor->GetActorForwardVector();
    }

    AimDirection = AimDirection.GetSafeNormal();
    if (AimDirection.IsNearlyZero())
    {
       return false;
    }

    const FVector ViewTraceEnd = ViewTraceStart + (AimDirection * ConfiguredTargetTraceMaxRange);
    const FCollisionProfileName ConfiguredTargetTraceProfile = Settings.TargetTraceProfile;
    if (ConfiguredTargetTraceProfile.Name == TEXT("NoCollision"))
    {
       OutTargetLocation = ViewTraceEnd;

       return true;
    }

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(const_cast<AActor*>(AvatarActor));
    ActorsToIgnore.Add(GetAbility()->GetAvatarActorFromActorInfo());

    FHitResult ViewHitResult;
    const bool bHit = UKismetSystemLibrary::LineTraceSingleByProfile(
       this,
       ViewTraceStart,
       ViewTraceEnd,
       ConfiguredTargetTraceProfile.Name,
       false,
       ActorsToIgnore,
       GetConfiguredDrawTargetTraceDebug() ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None,
       ViewHitResult,
       true);

    OutTargetLocation = bHit ? ViewHitResult.Location : ViewTraceEnd;

    return true;
}

FVector USkillProjectileCastAction::ResolveDefaultTargetLocation() const
{
    FVector TargetLocation = FVector::ZeroVector;
    if (TryResolveProjectileAimTargetLocation(TargetLocation))
    {
       return TargetLocation;
    }

    const AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
    if (!AvatarActor)
    {
       return FVector::ForwardVector * GetConfiguredTargetTraceMaxRange();
    }

    return GetSpawnLocation() + (AvatarActor->GetActorForwardVector() * FMath::Max(GetConfiguredTargetTraceMaxRange(), 1000.0f));
}

void USkillProjectileCastAction::PauseProjectileMontageForAiming()
{
    UPdAbilitySystemComponent* AbilitySystemComponent = GetAbility()->GetPdAbilitySystemComponentFromActorInfo();
    if (!AbilitySystemComponent)
    {
       return;
    }

    AbilitySystemComponent->CurrentMontageSetPlayRate(0.0f);

}

void USkillProjectileCastAction::ResumeProjectileMontageAfterAiming()
{
    UPdAbilitySystemComponent* AbilitySystemComponent = GetAbility()->GetPdAbilitySystemComponentFromActorInfo();
    if (!AbilitySystemComponent)
    {
       return;
    }

    AbilitySystemComponent->CurrentMontageSetPlayRate(1.0f);

}

void USkillProjectileCastAction::CleanupAimingState()
{
    GetAbility()->RestoreAvatarMovementForAbility();
    bWaitingForPlayerConfirm = false;
    bPlayerProjectileConfirmed = false;

    if (bPausedForPlayerAim)
    {
       ResumeProjectileMontageAfterAiming();
       bPausedForPlayerAim = false;
    }

    if (ConfirmCancelTask)
    {
       ConfirmCancelTask->EndTask();
       ConfirmCancelTask = nullptr;
    }

    if (ShootProjectileEventTask)
    {
       ShootProjectileEventTask->EndTask();
       ShootProjectileEventTask = nullptr;
    }

    if (ShootMontageTask)
    {
       ShootMontageTask->EndTask();
       ShootMontageTask = nullptr;
    }

    if (TargetDataTask)
    {
       bCleaningUpTargetDataTask = true;
       TargetDataTask->EndTask();
       bCleaningUpTargetDataTask = false;
       TargetDataTask = nullptr;
    }

    DestroyReadiedProjectile();
}

void USkillProjectileCastAction::FinishCast()
{
    Finish();
}
