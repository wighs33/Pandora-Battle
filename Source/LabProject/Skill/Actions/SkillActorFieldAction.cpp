#include "Skill/Actions/SkillActorFieldAction.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Skill/Actors/SkillEffectArea.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Skill/SkillGroundProjection.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Skill/Actors/SkillPowerUpActor.h"
#include "Skill/Actors/SkillBlackHoleActor.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Common/CollisionChannels.h"
#include "Common/LabGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffectTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Pandora/PandoraSkillSource.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillActorFieldAction)

namespace
{
bool HasConfiguredFieldTriggerDamage(const USkillDefinition* SkillDataAsset)
    {
       return SkillDataAsset && SkillDataAsset->GetResolvedDamageConfig().GameplayEffectClass != nullptr;
    }

    bool ShouldRepeatFieldTriggerDamage(const USkillDefinition* SkillDataAsset)
    {
       return SkillDataAsset && SkillDataAsset->Damage.bRepeatTriggerDamageWhileOverlapping;
    }

    double GetFieldTriggerDamageInterval(const USkillDefinition* SkillDataAsset)
    {
       return SkillDataAsset ? SkillDataAsset->Damage.TriggerDamageInterval : 0.0;
    }

    bool IsFieldSourceActorTarget(AActor* SourceActor, AActor* DamageSourceActor, AActor* HitActor)
    {
       if (!IsValid(HitActor))
       {
          return false;
       }

       if (HitActor == SourceActor || HitActor == DamageSourceActor)
       {
          return true;
       }

       if (DamageSourceActor)
       {
          if (HitActor == DamageSourceActor->GetOwner()
             || HitActor == DamageSourceActor->GetInstigator()
             || HitActor == DamageSourceActor->GetAttachParentActor())
          {
             return true;
          }
       }

       if (const APawn* SourcePawn = Cast<APawn>(SourceActor))
       {
          if (const APawn* HitPawn = Cast<APawn>(HitActor))
          {
             return SourcePawn == HitPawn
                || (SourcePawn->GetController() && SourcePawn->GetController() == HitPawn->GetController());
          }
       }

       return false;
    }
}

USkillActorFieldAction::USkillActorFieldAction()
{
    Settings.SpawnSocketNames.SetNum(6);
}

void USkillActorFieldAction::OnStart()
{
    const FGameplayAbilityActorInfo* ActorInfo = GetAbility()->GetCurrentActorInfo();

    CleanupFieldTasks();
    SpawnedFieldActors.Reset();
    FieldTriggerComponents.Reset();
    PendingFieldSocketNames.Reset();
    DamagedFieldTriggerActorsBySource.Reset();
    FieldDamageSourceActorsByKey.Reset();
    FieldOverlappingActorsBySource.Reset();
    NextFieldSocketIndex = 0;
    bFieldStarted = false;
    MovementSpeedEffectHandle.Invalidate();

    if (UWorld* World = GetWorld())
    {
       World->GetTimerManager().ClearTimer(FieldSpawnTimerHandle);
       World->GetTimerManager().ClearTimer(FieldRepeatSpawnTimerHandle);
       World->GetTimerManager().ClearTimer(FieldEndTimerHandle);
       World->GetTimerManager().ClearTimer(FieldTriggerDamageTickTimerHandle);
    }

    const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !SkillDataAsset)
    {
       Finish(false);
       return;
    }

    if (!Settings.FieldActorClass)
    {
       Finish(false);
       return;
    }

    StartFieldDurationMovementLockIfAllowed();
    StartWaitFieldMontageTriggerTask();

    if (!GetResolvedFieldMontage())
    {
       TryCommitAndStartField();
       return;
    }

    if (!StartFieldMontageTask())
    {
       TryCommitAndStartField();
       return;
    }
}

void USkillActorFieldAction::OnStop()
{
    CleanupFieldTasks();
    RemoveFieldMovementSpeedIncrease();

    TSet<AActor*> ActorsWithBoundDamageTriggers;
    for (const UPrimitiveComponent* TriggerComponent : FieldTriggerComponents)
    {
       if (TriggerComponent && TriggerComponent->GetOwner())
       {
          ActorsWithBoundDamageTriggers.Add(TriggerComponent->GetOwner());
       }
    }

    UnbindFieldTriggerDamage();

    if (UWorld* World = GetWorld())
    {
       World->GetTimerManager().ClearTimer(FieldSpawnTimerHandle);
       World->GetTimerManager().ClearTimer(FieldRepeatSpawnTimerHandle);
       World->GetTimerManager().ClearTimer(FieldEndTimerHandle);
       World->GetTimerManager().ClearTimer(FieldTriggerDamageTickTimerHandle);
    }
    FieldSpawnTimerHandle.Invalidate();
    FieldRepeatSpawnTimerHandle.Invalidate();
    FieldEndTimerHandle.Invalidate();
    FieldTriggerDamageTickTimerHandle.Invalidate();

    const bool bDestroySpawnedActorsOnAbilityEnd =
       GetAbility()->HasDurationDeadline() || Settings.bDestroySpawnedActorsOnAbilityEnd;
    if (bDestroySpawnedActorsOnAbilityEnd || !SpawnedFieldActors.IsEmpty())
    {
       for (AActor* SpawnedActor : SpawnedFieldActors)
       {
          const bool bForceDestroyForSourceBuffActor = SpawnedActor && SpawnedActor->IsA<ASkillPowerUpActor>();
          const bool bForceDestroyForBoundDamageTrigger =
             SpawnedActor && ActorsWithBoundDamageTriggers.Contains(SpawnedActor);
          const bool bExpiresThroughConfiguredLifeSpan =
             Settings.bUseSpawnedActorLifeSpan
             && Settings.SpawnedActorLifeSpan > 0.0;
          if (SpawnedActor
             && SpawnedActor->HasAuthority()
             && (bDestroySpawnedActorsOnAbilityEnd
                || bForceDestroyForSourceBuffActor
                || (bForceDestroyForBoundDamageTrigger && !bExpiresThroughConfiguredLifeSpan)))
          {
             DestroyFieldActorWhenReplicationIsSafe(SpawnedActor, Settings);
          }
       }
    }

    SpawnedFieldActors.Reset();
    FieldTriggerComponents.Reset();
    PendingFieldSocketNames.Reset();
    DamagedFieldTriggerActorsBySource.Reset();
    FieldDamageSourceActorsByKey.Reset();
    FieldOverlappingActorsBySource.Reset();
    NextFieldSocketIndex = 0;
    bFieldStarted = false;
}

UAnimMontage* USkillActorFieldAction::GetResolvedFieldMontage() const
{
    const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
    return SkillDataAsset ? SkillDataAsset->Animation.PrimaryMontage.Get() : nullptr;
}

FGameplayTag USkillActorFieldAction::GetResolvedFieldTriggerEventTag() const
{
    const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
    return SkillDataAsset && SkillDataAsset->Animation.PrimaryEventTag.IsValid()
       ? SkillDataAsset->Animation.PrimaryEventTag
       : LabGameplayTags::Event_Montage_Trigger;
}

bool USkillActorFieldAction::StartFieldMontageTask()
{
    UAnimMontage* MontageToPlay = GetResolvedFieldMontage();
    if (!MontageToPlay)
    {
       return false;
    }

    FieldMontageTask = GetAbility()->CreateDefaultMontageAndWaitTask(MontageToPlay);
    if (!FieldMontageTask)
    {
       return false;
    }

    FieldMontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleFieldMontageFinished);
    FieldMontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleFieldMontageFinished);
    FieldMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleFieldMontageInterrupted);
    FieldMontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleFieldMontageInterrupted);
    FieldMontageTask->ReadyForActivation();
    return true;
}

void USkillActorFieldAction::StartWaitFieldMontageTriggerTask()
{
    const FGameplayTag TriggerTag = GetResolvedFieldTriggerEventTag();
    if (!TriggerTag.IsValid())
    {
       return;
    }

    WaitFieldMontageTriggerTask = GetAbility()->CreateWaitGameplayEventTask(TriggerTag);
    if (!WaitFieldMontageTriggerTask)
    {
       return;
    }

    WaitFieldMontageTriggerTask->EventReceived.AddDynamic(this, &ThisClass::HandleFieldMontageTriggerEvent);
    WaitFieldMontageTriggerTask->ReadyForActivation();
}

void USkillActorFieldAction::TryCommitAndStartField()
{
    if (bFieldStarted || !(IsRunning() && GetAbility()->CanRunActions()))
    {
       return;
    }
    bFieldStarted = true;

    AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
    if (!AvatarActor || !AvatarActor->HasAuthority())
    {
       GetAbility()->StartConfiguredDefaultFX();
       if (!GetAbility()->HasDurationDeadline() && !FieldEndTimerHandle.IsValid() && !GetResolvedFieldMontage())
       {
          Finish();
       }
       return;
    }

    if (!GetAbility()->CommitSkill())
    {
       Finish(false);
       return;
    }

    ApplyFieldMovementSpeedIncrease();
    GetAbility()->StartConfiguredDefaultFX();
    GetAbility()->SpawnConfiguredCharacterDecal();
    StartFieldDurationMovementLockIfAllowed();
    StartFieldSpawnSequence();
    if (IsRunning())
    {
       StartFieldRepeatTimer();
    }
}

void USkillActorFieldAction::ApplyFieldMovementSpeedIncrease()
{
    if (MovementSpeedEffectHandle.IsValid())
    {
       return;
    }

    const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
    ACharacterBase* Character = GetAbility()->GetPdCharacterFromActorInfo();
    UPdAbilitySystemComponent* AbilitySystemComponent = GetAbility()->GetPdAbilitySystemComponentFromActorInfo();
    const UGameSettingDefinition* SettingDefinition =
       UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
    const TSubclassOf<UGameplayEffect> MovementSpeedEffectClass =
       SettingDefinition
          ? SettingDefinition->MovementSpeedGameplayEffectClass
          : nullptr;
    if (!SkillDataAsset
       || !SkillDataAsset->Movement.bOverrideMovementSpeedWhileActive
       || !Character
       || !Character->HasAuthority()
       || !AbilitySystemComponent
       || !MovementSpeedEffectClass)
    {
       return;
    }

    const double ConfiguredMovementSpeedIncrease = SkillDataAsset->Movement.MovementSpeedBonusPercent;
    if (ConfiguredMovementSpeedIncrease <= 0.0)
    {
       return;
    }

    FGameplayEffectSpecHandle MovementSpeedSpec =
       GetAbility()->MakeOutgoingGameplayEffectSpec(
          GetAbility()->GetCurrentAbilitySpecHandle(),
          GetAbility()->GetCurrentActorInfo(),
          GetAbility()->GetCurrentActivationInfo(),
          MovementSpeedEffectClass,
          GetAbility()->GetAbilityLevel());
    if (!MovementSpeedSpec.IsValid() || !MovementSpeedSpec.Data.IsValid())
    {
       return;
    }

    MovementSpeedSpec.Data->SetSetByCallerMagnitude(
       LabGameplayTags::Data_MovementSpeed,
       static_cast<float>(ConfiguredMovementSpeedIncrease));
    MovementSpeedEffectHandle = GetAbility()->ApplyGameplayEffectSpecToOwner(
       GetAbility()->GetCurrentAbilitySpecHandle(),
       GetAbility()->GetCurrentActorInfo(),
       GetAbility()->GetCurrentActivationInfo(),
       MovementSpeedSpec);
}

void USkillActorFieldAction::RemoveFieldMovementSpeedIncrease()
{
    if (!MovementSpeedEffectHandle.IsValid())
    {
       return;
    }

    ACharacterBase* Character = GetAbility()->GetPdCharacterFromActorInfo();
    UPdAbilitySystemComponent* AbilitySystemComponent = GetAbility()->GetPdAbilitySystemComponentFromActorInfo();
    if (Character && Character->HasAuthority() && AbilitySystemComponent)
    {
       AbilitySystemComponent->RemoveActiveGameplayEffect(
          MovementSpeedEffectHandle,
          1);
    }

    MovementSpeedEffectHandle.Invalidate();
}

void USkillActorFieldAction::StartFieldDurationMovementLockIfAllowed()
{
    if (ShouldSkipFieldDurationMovementLock())
    {
       return;
    }

    GetAbility()->StartDurationMovementLock();
}

bool USkillActorFieldAction::ShouldSkipFieldDurationMovementLock() const
{
    return Settings.FieldActorClass
       && Settings.FieldActorClass.Get()->IsChildOf(ASkillPowerUpActor::StaticClass());
}

TArray<FName> USkillActorFieldAction::GetConfiguredFieldSocketNames() const
{
    TArray<FName> SocketNames;
    if (!Settings.bUseSpawnSockets)
    {
       SocketNames.Add(NAME_None);
       return SocketNames;
    }

    for (const FName& SocketName : Settings.SpawnSocketNames)
    {
       if (!SocketName.IsNone())
       {
          SocketNames.Add(SocketName);
       }
    }

    if (SocketNames.IsEmpty())
    {
       SocketNames.Add(NAME_None);
    }

    return SocketNames;
}

void USkillActorFieldAction::StartFieldSpawnSequence()
{
    PendingFieldSocketNames = GetConfiguredFieldSocketNames();
    NextFieldSocketIndex = 0;

    if (PendingFieldSocketNames.IsEmpty())
    {
       ScheduleCompletion();
       return;
    }

    SpawnNextFieldActor();
}

void USkillActorFieldAction::StartFieldRepeatTimer()
{
    if (!ShouldRepeatFieldSpawnSequence())
    {
       return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
       return;
    }

    const float RepeatInterval = static_cast<float>(FMath::Max(Settings.RepeatSpawnInterval, 0.1));
    World->GetTimerManager().SetTimer(
       FieldRepeatSpawnTimerHandle,
       this,
       &ThisClass::HandleRepeatedFieldSpawnSequence,
       RepeatInterval,
       true);
}

void USkillActorFieldAction::SpawnNextFieldActor()
{
    if (!PendingFieldSocketNames.IsValidIndex(NextFieldSocketIndex))
    {
       FinishFieldSpawnSequence();
       return;
    }

    SpawnFieldActorForSocket(PendingFieldSocketNames[NextFieldSocketIndex]);

    ++NextFieldSocketIndex;
    if (!PendingFieldSocketNames.IsValidIndex(NextFieldSocketIndex))
    {
       FinishFieldSpawnSequence();
       return;
    }

    const float SpawnInterval = static_cast<float>(FMath::Max(Settings.SpawnInterval, 0.0));
    if (SpawnInterval <= KINDA_SMALL_NUMBER)
    {
       SpawnNextFieldActor();
       return;
    }

    if (UWorld* World = GetWorld())
    {
       World->GetTimerManager().SetTimer(
          FieldSpawnTimerHandle,
          this,
          &ThisClass::SpawnNextFieldActor,
          SpawnInterval,
          false);
    }
}

void USkillActorFieldAction::FinishFieldSpawnSequence()
{
    PendingFieldSocketNames.Reset();
    NextFieldSocketIndex = 0;
    if (SpawnedFieldActors.IsEmpty())
    {
       Finish(false);
       return;
    }
    ScheduleCompletion();
}

AActor* USkillActorFieldAction::SpawnFieldActorForSocket(const FName SocketName)
{
    AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
    UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
    if (!AvatarActor || !AvatarActor->HasAuthority() || !World || !Settings.FieldActorClass)
    {
       return nullptr;
    }

    const FTransform SpawnTransform = ResolveFieldSpawnTransform(SocketName);
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = AvatarActor;
    SpawnParams.Instigator = Cast<APawn>(AvatarActor);
    SpawnParams.SpawnCollisionHandlingOverride = Settings.SpawnCollisionHandling;

    AActor* SpawnedActor = World->SpawnActorDeferred<AActor>(
       Settings.FieldActorClass,
       SpawnTransform,
       SpawnParams.Owner,
       SpawnParams.Instigator,
       SpawnParams.SpawnCollisionHandlingOverride);

    if (!SpawnedActor && SpawnParams.SpawnCollisionHandlingOverride != ESpawnActorCollisionHandlingMethod::AlwaysSpawn)
    {
       SpawnedActor = World->SpawnActorDeferred<AActor>(
          Settings.FieldActorClass,
          SpawnTransform,
          SpawnParams.Owner,
          SpawnParams.Instigator,
          ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    }

    if (!SpawnedActor)
    {
       return nullptr;
    }

    const bool bShouldReplicateSpawnedActor =
       Settings.bForceReplicateSpawnedActor || SpawnedActor->IsA<ASkillEffectArea>();

    EEnum_Direction PandoraLoadoutDirection = EEnum_Direction::Center;
    if (const UPandoraSkillSource* PandoraSource = GetAbility()->GetPandoraSkillSource())
    {
       PandoraLoadoutDirection = PandoraSource->GetLoadoutDirection();
    }

    if (ASkillBlackHoleActor* BlackHoleActor = Cast<ASkillBlackHoleActor>(SpawnedActor))
    {
       const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
       FSkillGameplayEffectConfig FinishDamageConfig;
       FinishDamageConfig.MagnitudeDataTag = LabGameplayTags::Data_Damage;
       if (SkillDataAsset)
       {
          FinishDamageConfig = SkillDataAsset->GetResolvedDamageConfig();
       }

       BlackHoleActor->ConfigureFromFieldSettings(
          Settings,
          FinishDamageConfig,
          FMath::Max(GetAbility()->GetAbilityLevel(), 1),
          PandoraLoadoutDirection);
    }

    if (ASkillPowerUpActor* PowerUpActor = Cast<ASkillPowerUpActor>(SpawnedActor))
    {
       PowerUpActor->ConfigurePresentationSettings(Settings.PowerUpPresentation);
    }

    if (ASkillEffectArea* EffectArea = Cast<ASkillEffectArea>(SpawnedActor))
    {
       EffectArea->SetSourceActor(AvatarActor);
       EffectArea->SetIgnoreSourceActor(Settings.bIgnoreSourceActor || Settings.bEffectAreaIgnoreSourceActor);
       EffectArea->SetAffectEnemiesOnly(Settings.bEffectAreaAffectEnemiesOnly);
       EffectArea->SetSourcePandoraLoadoutDirection(PandoraLoadoutDirection);
    }

    UGameplayStatics::FinishSpawningActor(SpawnedActor, SpawnTransform);

    if (bShouldReplicateSpawnedActor)
    {
       // SpawnActorDeferred returns before the actor is initialized. SetReplicates
       // cannot register an actor with the net driver in that state, so enabling it
       // before FinishSpawning can silently miss the actor's first replication.
       SpawnedActor->SetReplicates(true);
       SpawnedActor->SetReplicateMovement(true);
    }

    AttachSpawnedFieldActorToSocket(SpawnedActor, SocketName);

    if (bShouldReplicateSpawnedActor)
    {
       SpawnedActor->ForceNetUpdate();
    }

    float RequestedLifeSpan = Settings.bUseSpawnedActorLifeSpan && Settings.SpawnedActorLifeSpan > 0.0
       ? static_cast<float>(Settings.SpawnedActorLifeSpan)
       : SpawnedActor->GetLifeSpan();

    if (RequestedLifeSpan > 0.0f)
    {
       if (bShouldReplicateSpawnedActor)
       {
          RequestedLifeSpan = FMath::Max(
             RequestedLifeSpan,
             static_cast<float>(FMath::Max(Settings.MinimumReplicatedActorLifetime, 0.0)));
       }
       SpawnedActor->SetLifeSpan(RequestedLifeSpan);
    }

    SpawnedFieldActors.Add(SpawnedActor);
    BindFieldTriggerDamage(SpawnedActor);
    return SpawnedActor;
}

void USkillActorFieldAction::DestroyFieldActorWhenReplicationIsSafe(
    AActor* SpawnedActor,
    const FSkillActorFieldSettings& FieldSettings) const
{
    if (!SpawnedActor || !SpawnedActor->HasAuthority())
    {
       return;
    }

    const float MinimumReplicatedLifetime = SpawnedActor->GetIsReplicated()
       ? static_cast<float>(FMath::Max(FieldSettings.MinimumReplicatedActorLifetime, 0.0))
       : 0.0f;
    const float RemainingReplicationLifetime = FMath::Max(
       MinimumReplicatedLifetime - SpawnedActor->GetGameTimeSinceCreation(),
       0.0f);
    if (!GetAbility()->HasDurationDeadline() && RemainingReplicationLifetime > KINDA_SMALL_NUMBER)
    {
       // Damage delegates have already been removed. Keep only the replicated
       // actor alive long enough for a cold client to load and instantiate it.
       SpawnedActor->ForceNetUpdate();
       SpawnedActor->SetLifeSpan(RemainingReplicationLifetime);
       return;
    }

    SpawnedActor->Destroy();
}

FTransform USkillActorFieldAction::ResolveFieldSpawnTransform(const FName SocketName) const
{
    AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
    if (!AvatarActor)
    {
       return FTransform::Identity;
    }

    FTransform BaseTransform = AvatarActor->GetActorTransform();
    if (const ACharacterBase* Character = Cast<ACharacterBase>(AvatarActor))
    {
       const USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
       if (Settings.bUseSpawnSockets && CharacterMesh && !SocketName.IsNone() && CharacterMesh->DoesSocketExist(SocketName))
       {
          BaseTransform = CharacterMesh->GetSocketTransform(SocketName, RTS_World);
       }
       else if (!Settings.bUseSpawnSockets)
       {
          FVector FeetLocation = Character->GetActorLocation();
          if (const UCapsuleComponent* CapsuleComponent = Character->GetCapsuleComponent())
          {
             FeetLocation.Z -= CapsuleComponent->GetScaledCapsuleHalfHeight();
          }
          BaseTransform.SetLocation(FeetLocation);
       }
    }

    FVector SpawnLocation = BaseTransform.GetLocation()
       + (Settings.bUseSpawnSockets
          ? BaseTransform.GetRotation().RotateVector(Settings.SpawnLocationOffset)
          : Settings.SpawnLocationOffset);
    const FRotator SpawnRotation = BaseTransform.Rotator() + Settings.SpawnRotationOffset;

    if (Settings.bProjectSpawnToGround)
    {
       UWorld* World = AvatarActor->GetWorld();
       const FVector TraceBaseLocation =
          Settings.bUseSpawnSockets ? BaseTransform.GetLocation() : AvatarActor->GetActorLocation();

       TArray<AActor*> ActorsToIgnore;
       PdSkillGroundProjection::AddIgnoredActorAndAttachments(ActorsToIgnore, AvatarActor);

       PdSkillGroundProjection::FGroundProjectionResult GroundProjection;
       if (PdSkillGroundProjection::TryProjectToGround(
          World,
          TraceBaseLocation,
          Settings.GroundTraceChannel,
          Settings.GroundTraceStartHeight,
          Settings.GroundTraceDepth,
          ActorsToIgnore,
          GroundProjection))
       {
          SpawnLocation = GroundProjection.Location
             + (Settings.bUseSpawnSockets
                ? BaseTransform.GetRotation().RotateVector(Settings.SpawnLocationOffset)
                : Settings.SpawnLocationOffset);
       }
    }

    return FTransform(SpawnRotation, SpawnLocation, BaseTransform.GetScale3D());
}

USkeletalMeshComponent* USkillActorFieldAction::ResolveFieldSpawnSocketMesh(const FName SocketName) const
{
    if (SocketName.IsNone())
    {
       return nullptr;
    }

    const ACharacterBase* Character = Cast<ACharacterBase>(GetAbility()->GetAvatarActorFromActorInfo());
    USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;
    return CharacterMesh && CharacterMesh->DoesSocketExist(SocketName) ? CharacterMesh : nullptr;
}

bool USkillActorFieldAction::AttachSpawnedFieldActorToSocket(AActor* SpawnedActor, const FName SocketName) const
{
    if (!SpawnedActor
       || !Settings.bUseSpawnSockets
       || !Settings.bAttachSpawnedActorToSocket
       || SocketName.IsNone())
    {
       return false;
    }

    USkeletalMeshComponent* CharacterMesh = ResolveFieldSpawnSocketMesh(SocketName);
    if (!CharacterMesh)
    {
       return false;
    }

    SpawnedActor->AttachToComponent(CharacterMesh, FAttachmentTransformRules::KeepWorldTransform, SocketName);
    return true;
}

bool USkillActorFieldAction::ShouldRepeatFieldSpawnSequence() const
{
    return GetAbility()->GetSourceSkillDataAsset()
       && Settings.bRepeatSpawnSequence
       && GetAbility()->HasDurationDeadline()
       && GetAbility()->GetRemainingDuration() > 0.0f
       && Settings.RepeatSpawnInterval > 0.0;
}

UPrimitiveComponent* USkillActorFieldAction::FindFieldTriggerComponent(AActor* SpawnedActor) const
{
    if (!SpawnedActor)
    {
       return nullptr;
    }

    const FName TriggerComponentName = Settings.TriggerComponentName;

    TArray<UPrimitiveComponent*> PrimitiveComponents;
    SpawnedActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
    if (PrimitiveComponents.IsEmpty())
    {
       return nullptr;
    }

    if (!TriggerComponentName.IsNone())
    {
       for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
       {
          if (PrimitiveComponent && PrimitiveComponent->GetFName() == TriggerComponentName)
          {
             return PrimitiveComponent;
          }
       }

       for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
       {
          if (PrimitiveComponent && PrimitiveComponent->ComponentHasTag(TriggerComponentName))
          {
             return PrimitiveComponent;
          }
       }

       for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
       {
          if (PrimitiveComponent && PrimitiveComponent->GetName().Contains(TriggerComponentName.ToString()))
          {
             return PrimitiveComponent;
          }
       }
    }

    for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
    {
       if (PrimitiveComponent
          && PrimitiveComponent->GetGenerateOverlapEvents()
          && PrimitiveComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
       {
          return PrimitiveComponent;
       }
    }

    return PrimitiveComponents[0];
}

void USkillActorFieldAction::BindFieldTriggerDamage(AActor* SpawnedActor)
{
    if (!SpawnedActor || !SpawnedActor->HasAuthority())
    {
       return;
    }

    if (!HasConfiguredFieldTriggerDamage(GetAbility()->GetSourceSkillDataAsset()))
    {
       return;
    }

    if (SpawnedActor->IsA<ASkillBlackHoleActor>() && Settings.bBlackHoleApplyFinishAreaDamage)
    {
       return;
    }

    UPrimitiveComponent* TriggerComponent = FindFieldTriggerComponent(SpawnedActor);
    if (!TriggerComponent)
    {
       return;
    }

    // Actor field trigger volumes are gameplay-only overlap queries. Keeping them
    // as WorldDynamic lets weapon object traces hit the volume and then resolve its
    // owning character as the damage target.
    TriggerComponent->SetCollisionProfileName(TEXT("Custom"));
    TriggerComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerComponent->SetCollisionObjectType(LabCollisionChannels::OverlapBox());
    TriggerComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    TriggerComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    TriggerComponent->SetCollisionResponseToChannel(LabCollisionChannels::HitableBody(), ECR_Overlap);

    // Hide only the gameplay collision primitive. Propagating this state from a
    // root trigger also hides attached particle/Niagara components.
    TriggerComponent->SetHiddenInGame(true, false);
    TriggerComponent->SetGenerateOverlapEvents(true);
    TriggerComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleFieldTriggerBeginOverlap);
    TriggerComponent->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandleFieldTriggerEndOverlap);
    FieldTriggerComponents.AddUnique(TriggerComponent);
    TriggerComponent->UpdateOverlaps();

    ApplyFieldTriggerDamageToExistingOverlaps(
       SpawnedActor,
       TriggerComponent,
       Settings.bDamageExistingOverlapsOnSpawn);

    StartFieldTriggerDamageTickIfNeeded();
}

void USkillActorFieldAction::UnbindFieldTriggerDamage()
{
    for (UPrimitiveComponent* TriggerComponent : FieldTriggerComponents)
    {
       if (TriggerComponent)
       {
          TriggerComponent->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandleFieldTriggerBeginOverlap);
          TriggerComponent->OnComponentEndOverlap.RemoveDynamic(this, &ThisClass::HandleFieldTriggerEndOverlap);
          TriggerComponent->SetGenerateOverlapEvents(false);
          TriggerComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
       }
    }
    FieldDamageSourceActorsByKey.Reset();
    FieldOverlappingActorsBySource.Reset();
}

void USkillActorFieldAction::StartFieldTriggerDamageTickIfNeeded()
{
    if (FieldTriggerDamageTickTimerHandle.IsValid())
    {
       return;
    }

    const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
    const bool bRepeatDamage = ShouldRepeatFieldTriggerDamage(SkillDataAsset);
    const double TriggerDamageInterval = GetFieldTriggerDamageInterval(SkillDataAsset);
    if (!bRepeatDamage || !HasConfiguredFieldTriggerDamage(SkillDataAsset))
    {
       return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
       return;
    }

    // 소환 액터와 동일하게, 0 이하의 설정도 타이머 생성 시 최소 0.05초로 보정한다.
    const float DamageInterval = static_cast<float>(FMath::Max(TriggerDamageInterval, 0.05));
    World->GetTimerManager().SetTimer(
       FieldTriggerDamageTickTimerHandle,
       this,
       &ThisClass::HandleFieldTriggerDamageTick,
       DamageInterval,
       true);
}

void USkillActorFieldAction::HandleFieldTriggerDamageTick()
{
    if (!ShouldRepeatFieldTriggerDamage(GetAbility()->GetSourceSkillDataAsset()))
    {
       return;
    }

    TArray<FObjectKey> DamageSourceKeys;
    FieldOverlappingActorsBySource.GetKeys(DamageSourceKeys);

    for (const FObjectKey& SourceKey : DamageSourceKeys)
    {
       TWeakObjectPtr<AActor>* DamageSourcePtr = FieldDamageSourceActorsByKey.Find(SourceKey);
       AActor* DamageSourceActor = DamageSourcePtr ? DamageSourcePtr->Get() : nullptr;
       if (!IsValid(DamageSourceActor))
       {
          FieldDamageSourceActorsByKey.Remove(SourceKey);
          FieldOverlappingActorsBySource.Remove(SourceKey);
          continue;
       }

       TArray<TWeakObjectPtr<AActor>>* OverlappingActors = FieldOverlappingActorsBySource.Find(SourceKey);
       if (!OverlappingActors)
       {
          FieldDamageSourceActorsByKey.Remove(SourceKey);
          continue;
       }

       TArray<TWeakObjectPtr<AActor>> DamageTargets;
       DamageTargets.Reserve(OverlappingActors->Num());
       for (int32 ActorIndex = OverlappingActors->Num() - 1; ActorIndex >= 0; --ActorIndex)
       {
          AActor* OverlappingActor = (*OverlappingActors)[ActorIndex].Get();
          if (!IsValid(OverlappingActor))
          {
             OverlappingActors->RemoveAtSwap(ActorIndex);
             continue;
          }

          DamageTargets.Add(OverlappingActor);
       }

       if (OverlappingActors->IsEmpty())
       {
          FieldDamageSourceActorsByKey.Remove(SourceKey);
          FieldOverlappingActorsBySource.Remove(SourceKey);
          continue;
       }

       for (const TWeakObjectPtr<AActor>& TargetPtr : DamageTargets)
       {
          AActor* OverlappingActor = TargetPtr.Get();
          if (!IsValid(DamageSourceActor) || !IsValid(OverlappingActor))
          {
             continue;
          }

          const TArray<TWeakObjectPtr<AActor>>* CurrentOverlappingActors = FieldOverlappingActorsBySource.Find(SourceKey);
          if (!CurrentOverlappingActors
             || !CurrentOverlappingActors->ContainsByPredicate(
                [OverlappingActor](const TWeakObjectPtr<AActor>& ExistingActor)
                {
                   return ExistingActor.Get() == OverlappingActor;
                }))
          {
             continue;
          }

          ApplyFieldTriggerDamage(DamageSourceActor, OverlappingActor, true);
       }
    }
}

void USkillActorFieldAction::ApplyFieldTriggerDamageToExistingOverlaps(AActor* DamageSourceActor, UPrimitiveComponent* TriggerComponent, const bool bApplyDamage)
{
    if (!DamageSourceActor || !TriggerComponent)
    {
       return;
    }

    TArray<AActor*> OverlappingActors;
    TriggerComponent->GetOverlappingActors(OverlappingActors, ACharacterBase::StaticClass());
    for (AActor* OverlappingActor : OverlappingActors)
    {
       TrackFieldTriggerOverlap(DamageSourceActor, OverlappingActor);
       if (bApplyDamage)
       {
          ApplyFieldTriggerDamage(DamageSourceActor, OverlappingActor);
       }
    }
}

void USkillActorFieldAction::TrackFieldTriggerOverlap(AActor* DamageSourceActor, AActor* OtherActor)
{
    if (!IsValid(DamageSourceActor) || !IsValid(OtherActor) || !OtherActor->IsA<ACharacterBase>())
    {
       return;
    }

    const FObjectKey SourceKey(DamageSourceActor);
    FieldDamageSourceActorsByKey.FindOrAdd(SourceKey) = DamageSourceActor;

    TArray<TWeakObjectPtr<AActor>>& OverlappingActors = FieldOverlappingActorsBySource.FindOrAdd(SourceKey);
    for (const TWeakObjectPtr<AActor>& ExistingActor : OverlappingActors)
    {
       if (ExistingActor.Get() == OtherActor)
       {
          return;
       }
    }

    OverlappingActors.Add(OtherActor);
}

void USkillActorFieldAction::UntrackFieldTriggerOverlap(AActor* DamageSourceActor, AActor* OtherActor)
{
    if (!DamageSourceActor || !OtherActor)
    {
       return;
    }

    const FObjectKey SourceKey(DamageSourceActor);
    TArray<TWeakObjectPtr<AActor>>* OverlappingActors = FieldOverlappingActorsBySource.Find(SourceKey);
    if (!OverlappingActors)
    {
       return;
    }

    OverlappingActors->RemoveAllSwap(
       [OtherActor](const TWeakObjectPtr<AActor>& ExistingActor)
       {
          return !ExistingActor.IsValid() || ExistingActor.Get() == OtherActor;
       });

    if (OverlappingActors->IsEmpty())
    {
       FieldOverlappingActorsBySource.Remove(SourceKey);
       FieldDamageSourceActorsByKey.Remove(SourceKey);
    }
}

void USkillActorFieldAction::ApplyFieldTriggerDamage(AActor* DamageSourceActor, AActor* HitActor, const bool bAllowRepeatedDamage)
{
    AActor* SourceActor = GetAbility()->GetAvatarActorFromActorInfo();
    if (!SourceActor || !SourceActor->HasAuthority() || !IsValid(DamageSourceActor) || !IsValid(HitActor))
    {
       return;
    }

    if (Settings.bIgnoreSourceActor && IsFieldSourceActorTarget(SourceActor, DamageSourceActor, HitActor))
    {
       return;
    }

    TSet<FObjectKey>& DamagedActorsForSource = DamagedFieldTriggerActorsBySource.FindOrAdd(FObjectKey(DamageSourceActor));
    const FObjectKey HitActorKey(HitActor);
    if (!bAllowRepeatedDamage && DamagedActorsForSource.Contains(HitActorKey))
    {
       return;
    }

    UAbilitySystemComponent* SourceASC = nullptr;
    UAbilitySystemComponent* TargetASC = nullptr;
    if (!ResolveDamageableCharacterTarget(SourceActor, HitActor, SourceASC, TargetASC))
    {
       return;
    }

    const FGameplayEffectSpecHandle DamageSpecHandle =
       MakeFieldTriggerDamageSpec(DamageSourceActor, CalculateFieldTriggerDamageMagnitude());
    if (!DamageSpecHandle.IsValid())
    {
       return;
    }

    ApplyDamageWithConfiguredStatus(*SourceASC, *TargetASC, *DamageSpecHandle.Data);
    if (!bAllowRepeatedDamage)
    {
       DamagedActorsForSource.Add(HitActorKey);
    }
}

FGameplayEffectSpecHandle USkillActorFieldAction::MakeFieldTriggerDamageSpec(AActor* DamageSourceActor, const float DamageMagnitude) const
{
    const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
    const FSkillGameplayEffectConfig TriggerDamage = SkillDataAsset ? SkillDataAsset->GetResolvedDamageConfig() : FSkillGameplayEffectConfig();
    if (!SkillDataAsset || !TriggerDamage.GameplayEffectClass || DamageMagnitude <= 0.0f)
    {
       return FGameplayEffectSpecHandle();
    }

    return GetAbility()->MakeConfiguredDamageEffectSpec(TriggerDamage, DamageMagnitude, DamageSourceActor);
}

float USkillActorFieldAction::CalculateFieldTriggerDamageMagnitude() const
{
    const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
    return SkillDataAsset
       ? GetAbility()->CalculateDamageMagnitude(
          SkillDataAsset->GetResolvedDamageConfig())
       : 0.0f;
}

void USkillActorFieldAction::ScheduleCompletion()
{
    if (ShouldRepeatFieldSpawnSequence())
    {
       return;
    }

    const float EndDelay = static_cast<float>(FMath::Max(Settings.TriggerActiveDurationAfterLastSpawn, 0.0));

    if (GetAbility()->HasDurationDeadline())
    {
       return;
    }

    if (EndDelay <= KINDA_SMALL_NUMBER)
    {
       Finish();
       return;
    }

    if (UWorld* World = GetWorld())
    {
       World->GetTimerManager().SetTimer(
          FieldEndTimerHandle,
          this,
          &ThisClass::HandleFieldDurationFinished,
          EndDelay,
          false);
    }
}

void USkillActorFieldAction::HandleRepeatedFieldSpawnSequence()
{
    if (!ShouldRepeatFieldSpawnSequence())
    {
       return;
    }

    if (!PendingFieldSocketNames.IsEmpty())
    {
       return;
    }

    StartFieldSpawnSequence();
}

void USkillActorFieldAction::CleanupFieldTasks()
{
    if (FieldMontageTask)
    {
       FieldMontageTask->EndTask();
       FieldMontageTask = nullptr;
    }

    if (WaitFieldMontageTriggerTask)
    {
       WaitFieldMontageTriggerTask->EndTask();
       WaitFieldMontageTriggerTask = nullptr;
    }
}

void USkillActorFieldAction::HandleFieldMontageTriggerEvent(FGameplayEventData Payload)
{
    static_cast<void>(Payload);

    TryCommitAndStartField();
}

void USkillActorFieldAction::HandleFieldMontageFinished()
{
    FieldMontageTask = nullptr;

    if (bFieldStarted)
    {
       const AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
       if ((!AvatarActor || !AvatarActor->HasAuthority()) && !GetAbility()->HasDurationDeadline() && !FieldEndTimerHandle.IsValid())
       {
          Finish();
       }
       return;
    }

    TryCommitAndStartField();
}

void USkillActorFieldAction::HandleFieldMontageInterrupted()
{
    FieldMontageTask = nullptr;

    if (bFieldStarted)
    {
       const AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
       if ((!AvatarActor || !AvatarActor->HasAuthority()) && !GetAbility()->HasDurationDeadline() && !FieldEndTimerHandle.IsValid())
       {
          Finish();
       }
       return;
    }

    TryCommitAndStartField();
    const AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
    if (bFieldStarted && (!AvatarActor || !AvatarActor->HasAuthority()))
    {
       Finish();
    }
}

void USkillActorFieldAction::HandleFieldDurationFinished()
{
    Finish();
}

void USkillActorFieldAction::HandleFieldTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    const int32 OtherBodyIndex,
    const bool bFromSweep,
    const FHitResult& SweepResult)
{
    static_cast<void>(OtherComp);
    static_cast<void>(OtherBodyIndex);
    static_cast<void>(bFromSweep);
    static_cast<void>(SweepResult);

    AActor* DamageSourceActor = OverlappedComponent ? OverlappedComponent->GetOwner() : nullptr;
    TrackFieldTriggerOverlap(DamageSourceActor, OtherActor);
    ApplyFieldTriggerDamage(DamageSourceActor, OtherActor);
}

void USkillActorFieldAction::HandleFieldTriggerEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    const int32 OtherBodyIndex)
{
    static_cast<void>(OtherComp);
    static_cast<void>(OtherBodyIndex);

    AActor* DamageSourceActor = OverlappedComponent ? OverlappedComponent->GetOwner() : nullptr;
    UntrackFieldTriggerOverlap(DamageSourceActor, OtherActor);
}
