#include "AbilitySystem/Skill/Actions/SkillSummonAction.h"

#include "Component/AbilitySystem/Ability/AbilityPresentationManager.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/SkillGroundProjection.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "AbilitySystem/Summons/SkillSummonActor.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffectTypes.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillSummonAction)

namespace
{
	const FName SummonTriggerComponentName(TEXT("Box"));

bool ShouldRepeatSummonTriggerDamage(const USkillDefinition* SkillDataAsset)
	{
		return SkillDataAsset && SkillDataAsset->Damage.bRepeatTriggerDamageWhileOverlapping;
	}

	double GetSummonTriggerDamageInterval(const USkillDefinition* SkillDataAsset)
	{
		return SkillDataAsset ? SkillDataAsset->Damage.TriggerDamageInterval : 0.0;
	}

}

void USkillSummonAction::OnStart()
{
	const auto Handle = GetAbility()->GetCurrentAbilitySpecHandle();
	const auto* ActorInfo = GetAbility()->GetCurrentActorInfo();
	const auto ActivationInfo = GetAbility()->GetCurrentActivationInfo();
	const auto* TriggerEventData = &GetContext().EventData;
	static_cast<void>(TriggerEventData);

	SummonMontageTask = nullptr;
	WaitSummonMontageTriggerTask = nullptr;
	SummonDurationTask = nullptr;
	SpawnedSummonActor.Reset();
	SummonTriggerComponent.Reset();
	DamagedSummonTriggerActors.Reset();
	SummonOverlappingActors.Reset();
	SummonRiseStartLocation = FVector::ZeroVector;
	SummonRiseFinalLocation = FVector::ZeroVector;
	SummonRiseFinalRotation = FRotator::ZeroRotator;
	SummonRiseStartTime = 0.0f;
	bSummonStarted = false;
	bSummonRiseFinished = false;
	bSummonTriggerDamageActive = false;

	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	const FSkillSummonSettings* SummonConfig = GetSummonConfig();
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !SkillDataAsset || !SummonConfig)
	{
		Finish(!(true));
		return;
	}

	if (!SummonConfig->SummonedActorClass)
	{
		Finish(!(true));
		return;
	}

GetAbility()->StartDurationMovementLock();

	StartWaitSummonMontageTriggerTask();

	if (!GetResolvedSummonMontage())
	{

		TryCommitAndStartSummon();
		return;
	}

	if (!StartSummonMontageTask())
	{
		TryCommitAndStartSummon();
	}
}

void USkillSummonAction::OnStop()
{
	CleanupSummonTasks();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SummonRiseTimerHandle);
		World->GetTimerManager().ClearTimer(SummonTriggerDamageDelayTimerHandle);
		World->GetTimerManager().ClearTimer(SummonTriggerDamageTickTimerHandle);
	}
	SummonRiseTimerHandle.Invalidate();
	SummonTriggerDamageDelayTimerHandle.Invalidate();
	SummonTriggerDamageTickTimerHandle.Invalidate();

	DisableSummonTriggerDamage();
	UnbindSummonTriggerDamage();

	if (SpawnedSummonActor.IsValid() && SpawnedSummonActor->HasAuthority())
	{
		SpawnedSummonActor->Destroy();
	}

	SpawnedSummonActor.Reset();
	SummonTriggerComponent.Reset();
	DamagedSummonTriggerActors.Reset();
	SummonOverlappingActors.Reset();
}

const FSkillSummonSettings* USkillSummonAction::GetSummonConfig() const
{
	return &Settings;
}

UAnimMontage* USkillSummonAction::GetResolvedSummonMontage() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (SkillDataAsset && SkillDataAsset->Animation.PrimaryMontage)
	{
		return SkillDataAsset->Animation.PrimaryMontage.Get();
	}

	return nullptr;
}

FGameplayTag USkillSummonAction::GetResolvedMontageTriggerEventTag() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (SkillDataAsset && SkillDataAsset->Animation.PrimaryEventTag.IsValid())
	{
		return SkillDataAsset->Animation.PrimaryEventTag;
	}

	return LabGameplayTags::Event_Montage_Trigger;
}

void USkillSummonAction::StartWaitSummonMontageTriggerTask()
{
	const FGameplayTag TriggerTag = GetResolvedMontageTriggerEventTag();
	if (!TriggerTag.IsValid())
	{

		return;
	}

	WaitSummonMontageTriggerTask = GetAbility()->CreateWaitGameplayEventTask(TriggerTag);
	if (!WaitSummonMontageTriggerTask)
	{

		return;
	}

	WaitSummonMontageTriggerTask->EventReceived.AddDynamic(this, &ThisClass::HandleSummonMontageTriggerEvent);
	WaitSummonMontageTriggerTask->ReadyForActivation();
}

bool USkillSummonAction::StartSummonMontageTask()
{
	UAnimMontage* ResolvedMontage = GetResolvedSummonMontage();
	if (!ResolvedMontage)
	{
		return false;
	}

	SummonMontageTask = GetAbility()->CreateDefaultMontageAndWaitTask(ResolvedMontage);
	if (!SummonMontageTask)
	{

		return false;
	}

	SummonMontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleSummonMontageFinished);
	SummonMontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleSummonMontageFinished);
	SummonMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleSummonMontageInterrupted);
	SummonMontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleSummonMontageInterrupted);
	SummonMontageTask->ReadyForActivation();

	return true;
}

void USkillSummonAction::TryCommitAndStartSummon()
{
	if (bSummonStarted || !(IsRunning() && GetAbility()->CanRunActions()))
	{
		return;
	}
	bSummonStarted = true;

	AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
	if (!AvatarActor || !AvatarActor->HasAuthority())
	{

		GetAbility()->GetPresentationManager().StartConfiguredCharacterOverlay(*GetAbility());
		GetAbility()->GetPresentationManager().StartConfiguredDefaultFX(*GetAbility());
		if (!GetResolvedSummonMontage())
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

	GetAbility()->GetPresentationManager().StartConfiguredCharacterOverlay(*GetAbility());
	GetAbility()->GetPresentationManager().StartConfiguredDefaultFX(*GetAbility());
	if (!SpawnSummonedActor())
	{
		Finish(false);
		return;
	}

	GetAbility()->GetPresentationManager().SpawnConfiguredCharacterDecal(*GetAbility());
	GetAbility()->StartDurationMovementLock();
	StartSummonRise();
}

bool USkillSummonAction::SpawnSummonedActor()
{
	const FSkillSummonSettings* SummonConfig = GetSummonConfig();
	AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!SummonConfig || !AvatarActor || !World || !SummonConfig->SummonedActorClass)
	{
		return false;
	}

	const FTransform FinalTransform = ResolveFinalSummonTransform();
	SummonRiseFinalLocation = FinalTransform.GetLocation();
	SummonRiseFinalRotation = FinalTransform.Rotator();
	SummonRiseStartLocation = SummonRiseFinalLocation
		- FVector::UpVector * SummonConfig->GetRiseDistance();

	const FTransform SpawnTransform(SummonRiseFinalRotation, SummonRiseStartLocation);
	AActor* SummonedActor = World->SpawnActorDeferred<AActor>(
		SummonConfig->SummonedActorClass,
		SpawnTransform,
		AvatarActor,
		Cast<APawn>(AvatarActor),
		SummonConfig->SpawnCollisionHandling);
	if (!SummonedActor
		&& SummonConfig->SpawnCollisionHandling
			!= ESpawnActorCollisionHandlingMethod::AlwaysSpawn)
	{
		SummonedActor = World->SpawnActorDeferred<AActor>(
			SummonConfig->SummonedActorClass,
			SpawnTransform,
			AvatarActor,
			Cast<APawn>(AvatarActor),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	}
	if (!SummonedActor)
	{
		return false;
	}

	ConfigureSummonedActorReplication(SummonedActor, *SummonConfig);
	UGameplayStatics::FinishSpawningActor(SummonedActor, SpawnTransform);
	ForceSummonedActorNetUpdate(SummonedActor, *SummonConfig);

	SpawnedSummonActor = SummonedActor;
	BindSummonTriggerDamage(SummonedActor);

	if (SummonConfig->bDeactivateLaserNiagaraOnSpawn)
	{
		DeactivateSummonNiagara(SummonedActor);
	}

	return true;
}

void USkillSummonAction::ConfigureSummonedActorReplication(AActor* SummonedActor, const FSkillSummonSettings& SummonConfig) const
{
	if (!SummonedActor || !SummonConfig.bForceReplicateSpawnedActor)
	{
		return;
	}

	SummonedActor->SetReplicates(true);
	SummonedActor->SetReplicateMovement(true);
}

void USkillSummonAction::ForceSummonedActorNetUpdate(AActor* SummonedActor, const FSkillSummonSettings& SummonConfig) const
{
	if (!SummonedActor || !SummonConfig.bForceReplicateSpawnedActor || !SummonedActor->HasAuthority())
	{
		return;
	}

	SummonedActor->ForceNetUpdate();
}

FTransform USkillSummonAction::ResolveFinalSummonTransform() const
{
	const FSkillSummonSettings* SummonConfig = GetSummonConfig();
	AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
	if (!SummonConfig || !AvatarActor)
	{
		return FTransform::Identity;
	}

	FVector BaseLocation = AvatarActor->GetActorLocation();
	FRotator BaseRotation = AvatarActor->GetActorRotation();
	if (USkeletalMeshComponent* MeshComponent = GetAbility()->GetPdCharacterFromActorInfo() ? GetAbility()->GetPdCharacterFromActorInfo()->GetMesh() : nullptr;
		MeshComponent && !SummonConfig->SpawnSocketName.IsNone() && MeshComponent->DoesSocketExist(SummonConfig->SpawnSocketName))
	{
		BaseLocation = MeshComponent->GetSocketLocation(SummonConfig->SpawnSocketName);
		BaseRotation = MeshComponent->GetSocketRotation(SummonConfig->SpawnSocketName);
	}

	if (SummonConfig->bUseOwnerYawOnly)
	{
		BaseRotation.Pitch = 0.0;
		BaseRotation.Roll = 0.0;
	}

	FVector FinalLocation = BaseLocation
		+ BaseRotation.Vector() * static_cast<float>(SummonConfig->SpawnForwardDistance)
		+ BaseRotation.RotateVector(SummonConfig->SpawnLocationOffset);
	FinalLocation = ProjectSummonLocationToGround(FinalLocation);

	return FTransform(BaseRotation + SummonConfig->SpawnRotationOffset, FinalLocation);
}

FVector USkillSummonAction::ProjectSummonLocationToGround(const FVector& CandidateLocation) const
{
	const FSkillSummonSettings* SummonConfig = GetSummonConfig();
	AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!SummonConfig || !World || !AvatarActor || !SummonConfig->bProjectSpawnToGround)
	{
		return CandidateLocation;
	}

	TArray<AActor*> ActorsToIgnore;
	PdSkillGroundProjection::AddIgnoredActorAndAttachments(ActorsToIgnore, AvatarActor);

	PdSkillGroundProjection::FGroundProjectionResult GroundProjection;
	if (PdSkillGroundProjection::TryProjectToGround(
		World,
		CandidateLocation,
		SummonConfig->GroundTraceChannel,
		SummonConfig->GroundTraceStartHeight,
		SummonConfig->GroundTraceDepth,
		ActorsToIgnore,
		GroundProjection))
	{
		return GroundProjection.Location;
	}

	return CandidateLocation;
}

void USkillSummonAction::DeactivateSummonNiagara(AActor* SummonedActor) const
{
	const FSkillSummonSettings* SummonConfig = GetSummonConfig();
	if (!SummonConfig || !SummonedActor)
	{
		return;
	}

	if (ASkillSummonActor* SkillSummonActor = Cast<ASkillSummonActor>(SummonedActor))
	{
		SkillSummonActor->DeactivateSummonNiagara(
			SummonConfig->LaserNiagaraComponentName,
			SummonConfig->bActivateAllNiagaraComponentsWhenNameNone);
		return;
	}

	TArray<UNiagaraComponent*> NiagaraComponents;
	FindConfiguredNiagaraComponents(SummonedActor, NiagaraComponents);
	for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
	{
		if (NiagaraComponent)
		{
			NiagaraComponent->Deactivate();
		}
	}
}

void USkillSummonAction::ActivateSummonNiagara(AActor* SummonedActor) const
{
	const FSkillSummonSettings* SummonConfig = GetSummonConfig();
	if (!SummonConfig || !SummonedActor)
	{
		return;
	}

	if (ASkillSummonActor* SkillSummonActor = Cast<ASkillSummonActor>(SummonedActor))
	{
		SkillSummonActor->ActivateSummonNiagara(
			SummonConfig->LaserNiagaraComponentName,
			SummonConfig->bResetLaserNiagaraOnActivate,
			SummonConfig->bActivateAllNiagaraComponentsWhenNameNone);
		return;
	}

	TArray<UNiagaraComponent*> NiagaraComponents;
	FindConfiguredNiagaraComponents(SummonedActor, NiagaraComponents);
	if (NiagaraComponents.IsEmpty())
	{
		return;
	}

	for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
	{
		if (NiagaraComponent)
		{
			NiagaraComponent->Activate(SummonConfig->bResetLaserNiagaraOnActivate);
		}
	}

}

void USkillSummonAction::FindConfiguredNiagaraComponents(AActor* SummonedActor, TArray<UNiagaraComponent*>& OutComponents) const
{
	const FSkillSummonSettings* SummonConfig = GetSummonConfig();
	if (!SummonConfig || !SummonedActor)
	{
		return;
	}

	TArray<UNiagaraComponent*> NiagaraComponents;
	SummonedActor->GetComponents<UNiagaraComponent>(NiagaraComponents);
	if (SummonConfig->LaserNiagaraComponentName.IsNone())
	{
		if (SummonConfig->bActivateAllNiagaraComponentsWhenNameNone)
		{
			OutComponents = MoveTemp(NiagaraComponents);
		}
		return;
	}

	for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
	{
		if (!NiagaraComponent)
		{
			continue;
		}

		if (NiagaraComponent->GetFName() == SummonConfig->LaserNiagaraComponentName
			|| NiagaraComponent->ComponentHasTag(SummonConfig->LaserNiagaraComponentName))
		{
			OutComponents.Add(NiagaraComponent);
		}
	}
}

void USkillSummonAction::StartSummonRise()
{
	AActor* SummonedActor = SpawnedSummonActor.Get();
	const FSkillSummonSettings* SummonConfig = GetSummonConfig();
	if (!SummonedActor || !SummonConfig)
	{
		Finish(false);
		return;
	}

	const float RiseDuration = FMath::Max(SummonConfig->GetRiseDuration(), 0.0f);
	if (RiseDuration <= KINDA_SMALL_NUMBER || SummonRiseStartLocation.Equals(SummonRiseFinalLocation))
	{
		FinishSummonRiseAndActivateLaser();
		return;
	}

	UWorld* World = SummonedActor->GetWorld();
	if (!World)
	{
		Finish(false);
		return;
	}

	SummonRiseStartTime = World->GetTimeSeconds();
	const float TickInterval = static_cast<float>(FMath::Clamp(
		SummonConfig->RiseTickInterval,
		0.005,
		static_cast<double>(RiseDuration)));
	World->GetTimerManager().SetTimer(
		SummonRiseTimerHandle,
		this,
		&ThisClass::HandleSummonRiseTick,
		TickInterval,
		true);

	HandleSummonRiseTick();
}

void USkillSummonAction::HandleSummonRiseTick()
{
	AActor* SummonedActor = SpawnedSummonActor.Get();
	const FSkillSummonSettings* SummonConfig = GetSummonConfig();
	UWorld* World = SummonedActor ? SummonedActor->GetWorld() : nullptr;
	if (!SummonedActor || !SummonConfig || !World)
	{
		Finish(false);
		return;
	}

	const float RiseDuration = FMath::Max(SummonConfig->GetRiseDuration(), KINDA_SMALL_NUMBER);
	const float ElapsedTime = World->GetTimeSeconds() - SummonRiseStartTime;
	const float Alpha = FMath::Clamp(ElapsedTime / RiseDuration, 0.0f, 1.0f);
	const FVector NewLocation = FMath::Lerp(SummonRiseStartLocation, SummonRiseFinalLocation, Alpha);
	SummonedActor->SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.0f - KINDA_SMALL_NUMBER)
	{
		FinishSummonRiseAndActivateLaser();
	}
}

void USkillSummonAction::FinishSummonRiseAndActivateLaser()
{
	if (bSummonRiseFinished)
	{
		return;
	}
	bSummonRiseFinished = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SummonRiseTimerHandle);
	}
	SummonRiseTimerHandle.Invalidate();

	AActor* SummonedActor = SpawnedSummonActor.Get();
	const FSkillSummonSettings* SummonConfig = GetSummonConfig();
	if (!SummonedActor || !SummonConfig)
	{
		Finish(false);
		return;
	}

	SummonedActor->SetActorLocationAndRotation(
		SummonRiseFinalLocation,
		SummonRiseFinalRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	ForceSummonedActorNetUpdate(SummonedActor, *SummonConfig);

	ActivateSummonNiagara(SummonedActor);
	ForceSummonedActorNetUpdate(SummonedActor, *SummonConfig);

	if (const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
		SkillDataAsset
		&& SkillDataAsset->GetResolvedDamageConfig().GameplayEffectClass
		&& CalculateSummonTriggerDamageMagnitude() > 0.0f)
	{
		const float TriggerDelay = static_cast<float>(FMath::Max(Settings.TriggerDamageDelay, 0.0));
		if (TriggerDelay > KINDA_SMALL_NUMBER)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					SummonTriggerDamageDelayTimerHandle,
					this,
					&ThisClass::EnableSummonTriggerDamage,
					TriggerDelay,
					false);
			}
		}
		else
		{
			EnableSummonTriggerDamage();
		}
	}

StartSummonLifetimeTimerOrEnd();
}

void USkillSummonAction::StartSummonLifetimeTimerOrEnd()
{
	// Duration 소환은 준비·상승 시간을 포함한 스킬의 공통 종료 시점을 따른다.
	if (GetAbility()->HasDurationDeadline()) return;

	if (SummonDurationTask)
	{
		return;
	}

	const float ActiveDuration = ResolveSummonLifetimeTimerDuration();
	if (ActiveDuration <= KINDA_SMALL_NUMBER)
	{
		Finish();
		return;
	}

	SummonDurationTask = UAbilityTask_WaitDelay::WaitDelay(GetAbility(), ActiveDuration);
	if (!SummonDurationTask)
	{

		Finish();
		return;
	}

	SummonDurationTask->OnFinish.AddDynamic(this, &ThisClass::HandleSummonDurationFinished);
	SummonDurationTask->ReadyForActivation();
}

float USkillSummonAction::ResolveSummonActiveDuration() const
{
	if (GetAbility()->HasDurationDeadline())
	{
		return GetAbility()->GetRemainingDuration();
	}

	const FSkillSummonSettings* SummonConfig = GetSummonConfig();
	return SummonConfig ? static_cast<float>(FMath::Max(SummonConfig->SummonedActorLifeSpan, 0.0)) : 0.0f;
}

float USkillSummonAction::ResolveSummonLifetimeTimerDuration() const
{
	const float ActiveDuration = ResolveSummonActiveDuration();
	const FSkillSummonSettings* SummonConfig = GetSummonConfig();
	if (!SummonConfig || !SummonConfig->bForceReplicateSpawnedActor || !SpawnedSummonActor.IsValid())
	{
		return ActiveDuration;
	}

	return FMath::Max(
		ActiveDuration,
		static_cast<float>(FMath::Max(SummonConfig->MinimumReplicatedActorLifetime, 0.0)));
}

void USkillSummonAction::BindSummonTriggerDamage(AActor* SummonedActor)
{
	if (!SummonedActor || !SummonedActor->HasAuthority())
	{
		return;
	}

	UPrimitiveComponent* TriggerComponent = FindSummonTriggerComponent(SummonedActor);
	if (!TriggerComponent)
	{

		return;
	}

	SummonTriggerComponent = TriggerComponent;
	if (TriggerComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		TriggerComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	TriggerComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerComponent->SetGenerateOverlapEvents(true);
	TriggerComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleSummonTriggerBeginOverlap);
	TriggerComponent->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandleSummonTriggerEndOverlap);
	TriggerComponent->UpdateOverlaps();

}

void USkillSummonAction::UnbindSummonTriggerDamage()
{
	if (UPrimitiveComponent* TriggerComponent = SummonTriggerComponent.Get())
	{
		TriggerComponent->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandleSummonTriggerBeginOverlap);
		TriggerComponent->OnComponentEndOverlap.RemoveDynamic(this, &ThisClass::HandleSummonTriggerEndOverlap);
	}
	SummonOverlappingActors.Reset();
}

UPrimitiveComponent* USkillSummonAction::FindSummonTriggerComponent(AActor* SummonedActor) const
{
	if (!SummonedActor)
	{
		return nullptr;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	SummonedActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent && PrimitiveComponent->GetFName() == SummonTriggerComponentName)
		{
			return PrimitiveComponent;
		}
	}

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent && PrimitiveComponent->ComponentHasTag(SummonTriggerComponentName))
		{
			return PrimitiveComponent;
		}
	}

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent && PrimitiveComponent->GetName().Contains(SummonTriggerComponentName.ToString()))
		{
			return PrimitiveComponent;
		}
	}

	return nullptr;
}

void USkillSummonAction::EnableSummonTriggerDamage()
{
	if (!SpawnedSummonActor.IsValid() || !SpawnedSummonActor->HasAuthority())
	{
		return;
	}

	if (!SummonTriggerComponent.IsValid())
	{
		BindSummonTriggerDamage(SpawnedSummonActor.Get());
	}

	if (!SummonTriggerComponent.IsValid())
	{
		return;
	}

	bSummonTriggerDamageActive = true;
	ApplySummonTriggerDamageToExistingOverlaps();
	StartSummonTriggerDamageTickIfNeeded();

}

void USkillSummonAction::DisableSummonTriggerDamage()
{
	bSummonTriggerDamageActive = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SummonTriggerDamageTickTimerHandle);
	}
	SummonTriggerDamageTickTimerHandle.Invalidate();
}

void USkillSummonAction::StartSummonTriggerDamageTickIfNeeded()
{
	if (SummonTriggerDamageTickTimerHandle.IsValid())
	{
		return;
	}

	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (!bSummonTriggerDamageActive
		|| !ShouldRepeatSummonTriggerDamage(SkillDataAsset)
		|| !SkillDataAsset
		|| !SkillDataAsset->GetResolvedDamageConfig().GameplayEffectClass
		|| CalculateSummonTriggerDamageMagnitude() <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float DamageInterval = static_cast<float>(FMath::Max(GetSummonTriggerDamageInterval(SkillDataAsset), 0.05));
	World->GetTimerManager().SetTimer(
		SummonTriggerDamageTickTimerHandle,
		this,
		&ThisClass::HandleSummonTriggerDamageTick,
		DamageInterval,
		true);

}

void USkillSummonAction::HandleSummonTriggerDamageTick()
{
	if (!bSummonTriggerDamageActive)
	{
		return;
	}

	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	const bool bAllowRepeatedDamage = ShouldRepeatSummonTriggerDamage(SkillDataAsset);
	TArray<TWeakObjectPtr<AActor>> DamageTargets;
	DamageTargets.Reserve(SummonOverlappingActors.Num());
	for (int32 ActorIndex = SummonOverlappingActors.Num() - 1; ActorIndex >= 0; --ActorIndex)
	{
		AActor* OverlappingActor = SummonOverlappingActors[ActorIndex].Get();
		if (!IsValid(OverlappingActor))
		{
			SummonOverlappingActors.RemoveAtSwap(ActorIndex);
			continue;
		}

		DamageTargets.Add(OverlappingActor);
	}

	for (const TWeakObjectPtr<AActor>& TargetPtr : DamageTargets)
	{
		AActor* OverlappingActor = TargetPtr.Get();
		if (!IsValid(OverlappingActor)
			|| !SummonOverlappingActors.ContainsByPredicate(
				[OverlappingActor](const TWeakObjectPtr<AActor>& ExistingActor)
				{
					return ExistingActor.Get() == OverlappingActor;
				}))
		{
			continue;
		}

		ApplySummonTriggerDamage(OverlappingActor, bAllowRepeatedDamage);
	}
}

void USkillSummonAction::ApplySummonTriggerDamageToExistingOverlaps()
{
	UPrimitiveComponent* TriggerComponent = SummonTriggerComponent.Get();
	if (!TriggerComponent)
	{
		return;
	}

	TriggerComponent->UpdateOverlaps();

	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	const bool bAllowRepeatedDamage = ShouldRepeatSummonTriggerDamage(SkillDataAsset);

	TArray<AActor*> OverlappingActors;
	TriggerComponent->GetOverlappingActors(OverlappingActors, ACharacterBase::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		TrackSummonTriggerOverlap(OverlappingActor);
		ApplySummonTriggerDamage(OverlappingActor, bAllowRepeatedDamage);
	}
}

void USkillSummonAction::TrackSummonTriggerOverlap(AActor* OtherActor)
{
	if (!IsValid(OtherActor) || !OtherActor->IsA<ACharacterBase>())
	{
		return;
	}

	for (const TWeakObjectPtr<AActor>& ExistingActor : SummonOverlappingActors)
	{
		if (ExistingActor.Get() == OtherActor)
		{
			return;
		}
	}

	SummonOverlappingActors.Add(OtherActor);
}

void USkillSummonAction::UntrackSummonTriggerOverlap(AActor* OtherActor)
{
	if (!OtherActor)
	{
		return;
	}

	SummonOverlappingActors.RemoveAllSwap(
		[OtherActor](const TWeakObjectPtr<AActor>& ExistingActor)
		{
			return !ExistingActor.IsValid() || ExistingActor.Get() == OtherActor;
		});
}

void USkillSummonAction::ApplySummonTriggerDamage(AActor* HitActor, const bool bAllowRepeatedDamage)
{
	AActor* SourceActor = GetAbility()->GetAvatarActorFromActorInfo();
	if (!bSummonTriggerDamageActive || !IsValid(HitActor) || HitActor == SourceActor || HitActor == SpawnedSummonActor.Get())
	{
		return;
	}

	const FObjectKey HitActorKey(HitActor);
	if (!bAllowRepeatedDamage && DamagedSummonTriggerActors.Contains(HitActorKey))
	{
		return;
	}

	const ACharacterBase* SourceCharacter = Cast<ACharacterBase>(SourceActor);
	const ACharacterBase* TargetCharacter = Cast<ACharacterBase>(HitActor);
	if (!TargetCharacter)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor);
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	if (!SourceASC || !TargetASC)
	{

		return;
	}

	if (TargetASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead))
	{
		return;
	}

	if (SourceCharacter && !SourceCharacter->CanDamageCharacterByTeam(TargetCharacter))
	{

		return;
	}

	const float DamageMagnitude = CalculateSummonTriggerDamageMagnitude();
	FGameplayEffectSpecHandle DamageSpecHandle = MakeSummonTriggerDamageSpec(DamageMagnitude);
	if (!DamageSpecHandle.IsValid() || !DamageSpecHandle.Data.IsValid())
	{

		return;
	}

	const FActiveGameplayEffectHandle AppliedHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetASC);
	if (AppliedHandle.WasSuccessfullyApplied())
	{
		GetAbility()->ApplyConfiguredStatusEffectToTarget(
			GetAbility()->GetSourceSkillDataAsset(),
			TargetASC);
	}
	if (!bAllowRepeatedDamage)
	{
		DamagedSummonTriggerActors.Add(HitActorKey);
	}

}

FGameplayEffectSpecHandle USkillSummonAction::MakeSummonTriggerDamageSpec(const float DamageMagnitude) const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	const FSkillGameplayEffectConfig TriggerDamage = SkillDataAsset ? SkillDataAsset->GetResolvedDamageConfig() : FSkillGameplayEffectConfig();
	if (!SkillDataAsset || !TriggerDamage.GameplayEffectClass || DamageMagnitude <= 0.0f)
	{

		return FGameplayEffectSpecHandle();
	}

	return GetAbility()->MakeConfiguredDamageEffectSpec(
		TriggerDamage,
		DamageMagnitude,
		SpawnedSummonActor.IsValid() ? SpawnedSummonActor.Get() : nullptr);
}

float USkillSummonAction::CalculateSummonTriggerDamageMagnitude() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{
		return 0.0f;
	}

	return GetAbility()->CalculateDamageMagnitude(SkillDataAsset->GetResolvedDamageConfig());
}

void USkillSummonAction::CleanupSummonTasks()
{
	if (SummonMontageTask)
	{
		SummonMontageTask->EndTask();
		SummonMontageTask = nullptr;
	}

	if (WaitSummonMontageTriggerTask)
	{
		WaitSummonMontageTriggerTask->EndTask();
		WaitSummonMontageTriggerTask = nullptr;
	}

	if (SummonDurationTask)
	{
		SummonDurationTask->EndTask();
		SummonDurationTask = nullptr;
	}
}

void USkillSummonAction::HandleSummonMontageTriggerEvent(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	TryCommitAndStartSummon();
}

void USkillSummonAction::HandleSummonMontageFinished()
{
	SummonMontageTask = nullptr;

	if (bSummonStarted)
	{
		const AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
		if (!AvatarActor || !AvatarActor->HasAuthority())
		{
			Finish();
		}
		return;
	}

	TryCommitAndStartSummon();
}

void USkillSummonAction::HandleSummonMontageInterrupted()
{
	SummonMontageTask = nullptr;

	if (bSummonStarted)
	{
		return;
	}

	TryCommitAndStartSummon();
	const AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
	if (bSummonStarted && (!AvatarActor || !AvatarActor->HasAuthority()))
	{
		Finish();
	}
}

void USkillSummonAction::HandleSummonDurationFinished()
{
	SummonDurationTask = nullptr;
	Finish();
}

void USkillSummonAction::HandleSummonTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	static_cast<void>(OverlappedComponent);
	static_cast<void>(OtherComp);
	static_cast<void>(OtherBodyIndex);
	static_cast<void>(bFromSweep);
	static_cast<void>(SweepResult);

	TrackSummonTriggerOverlap(OtherActor);
	ApplySummonTriggerDamage(OtherActor);
}

void USkillSummonAction::HandleSummonTriggerEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	const int32 OtherBodyIndex)
{
	static_cast<void>(OverlappedComponent);
	static_cast<void>(OtherComp);
	static_cast<void>(OtherBodyIndex);

	UntrackSummonTriggerOverlap(OtherActor);
}
