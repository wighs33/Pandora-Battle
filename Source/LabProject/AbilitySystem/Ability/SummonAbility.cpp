#include "AbilitySystem/Ability/SummonAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/SkillGroundProjection.h"
#include "Definition/AbilitySystem/SkillTypes.h"
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

#include UE_INLINE_GENERATED_CPP_BY_NAME(SummonAbility)


namespace
{
	const FName SummonTriggerComponentName(TEXT("Box"));

	const FSummonSkillConfig* GetSummonConfigFromSkill(const USkillDefinition* SkillDataAsset)
	{
		return SkillDataAsset && SkillDataAsset->SkillDataType == EPdSkillDataType::Summon
			? &SkillDataAsset->Summon
			: nullptr;
	}

	bool ShouldRepeatSummonTriggerDamage(const USkillDefinition* SkillDataAsset)
	{
		return SkillDataAsset && SkillDataAsset->Damage.bRepeatTriggerDamageWhileOverlapping;
	}

	double GetSummonTriggerDamageInterval(const USkillDefinition* SkillDataAsset)
	{
		return SkillDataAsset ? SkillDataAsset->Damage.TriggerDamageInterval : 0.0;
	}

}

USummonAbility::USummonAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(LabGameplayTags::GameplayAbility_Summon);
	SetAssetTags(AssetTags);

	ActivationOwnedTags.AddTag(LabGameplayTags::GameplayAbility_Summon_Active);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::Action_Attack);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::Action_Punch);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::Action_RangedAttack);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::GameplayAbility_ShootProjectile);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::GameplayAbility_AOEAttack);
}

void USummonAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
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

	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	const FSummonSkillConfig* SummonConfig = GetSummonConfig();
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !SkillDataAsset || !SummonConfig)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!SummonConfig->SummonedActorClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}


	StartDurationMovementLock();

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

void USummonAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
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
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

const FSummonSkillConfig* USummonAbility::GetSummonConfig() const
{
	return GetSummonConfigFromSkill(GetSourceSkillDataAsset());
}

UAnimMontage* USummonAbility::GetResolvedSummonMontage() const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	if (SkillDataAsset && SkillDataAsset->Animation.PrimaryMontage)
	{
		return SkillDataAsset->Animation.PrimaryMontage.Get();
	}

	const FSummonSkillConfig* SummonConfig = GetSummonConfig();
	return SummonConfig ? SummonConfig->Animation.PrimaryMontage.Get() : nullptr;
}

FGameplayTag USummonAbility::GetResolvedMontageTriggerEventTag() const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	if (SkillDataAsset && SkillDataAsset->Animation.PrimaryEventTag.IsValid())
	{
		return SkillDataAsset->Animation.PrimaryEventTag;
	}

	const FSummonSkillConfig* SummonConfig = GetSummonConfig();
	return SummonConfig && SummonConfig->Animation.PrimaryEventTag.IsValid()
		? SummonConfig->Animation.PrimaryEventTag
		: LabGameplayTags::Event_Montage_Trigger;
}

void USummonAbility::StartWaitSummonMontageTriggerTask()
{
	const FGameplayTag TriggerTag = GetResolvedMontageTriggerEventTag();
	if (!TriggerTag.IsValid())
	{

		return;
	}

	WaitSummonMontageTriggerTask = CreateWaitGameplayEventTask(TriggerTag);
	if (!WaitSummonMontageTriggerTask)
	{

		return;
	}

	WaitSummonMontageTriggerTask->EventReceived.AddDynamic(this, &ThisClass::HandleSummonMontageTriggerEvent);
	WaitSummonMontageTriggerTask->ReadyForActivation();
}

bool USummonAbility::StartSummonMontageTask()
{
	UAnimMontage* ResolvedMontage = GetResolvedSummonMontage();
	if (!ResolvedMontage)
	{
		return false;
	}

	SummonMontageTask = CreateDefaultMontageAndWaitTask(ResolvedMontage);
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

void USummonAbility::TryCommitAndStartSummon()
{
	if (bSummonStarted || !CanExecuteSkillPayload())
	{
		return;
	}
	bSummonStarted = true;

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor || !AvatarActor->HasAuthority())
	{

		StartConfiguredCharacterOverlay();
		StartConfiguredDefaultFX();
		if (!GetResolvedSummonMontage())
		{
			K2_EndAbilityLocally();
		}
		return;
	}

	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		K2_CancelAbility();
		return;
	}

	StartConfiguredCharacterOverlay();
	StartConfiguredDefaultFX();
	if (!SpawnSummonedActor())
	{
		CancelAbilityForSkillExecutionFailure();
		return;
	}

	SpawnConfiguredCharacterDecal();
	StartDurationMovementLock();
	StartSummonRise();
}

bool USummonAbility::SpawnSummonedActor()
{
	const FSummonSkillConfig* SummonConfig = GetSummonConfig();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!SummonConfig || !AvatarActor || !World || !SummonConfig->SummonedActorClass)
	{
		return false;
	}

	const FTransform FinalTransform = ResolveFinalSummonTransform();
	SummonRiseFinalLocation = FinalTransform.GetLocation();
	SummonRiseFinalRotation = FinalTransform.Rotator();
	SummonRiseStartLocation = SummonRiseFinalLocation
		- FVector::UpVector * static_cast<float>(FMath::Max(SummonConfig->RiseDistanceBelowGround, 0.0));

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

void USummonAbility::ConfigureSummonedActorReplication(AActor* SummonedActor, const FSummonSkillConfig& SummonConfig) const
{
	if (!SummonedActor || !SummonConfig.bForceReplicateSpawnedActor)
	{
		return;
	}

	SummonedActor->SetReplicates(true);
	SummonedActor->SetReplicateMovement(true);
}

void USummonAbility::ForceSummonedActorNetUpdate(AActor* SummonedActor, const FSummonSkillConfig& SummonConfig) const
{
	if (!SummonedActor || !SummonConfig.bForceReplicateSpawnedActor || !SummonedActor->HasAuthority())
	{
		return;
	}

	SummonedActor->ForceNetUpdate();
}

FTransform USummonAbility::ResolveFinalSummonTransform() const
{
	const FSummonSkillConfig* SummonConfig = GetSummonConfig();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!SummonConfig || !AvatarActor)
	{
		return FTransform::Identity;
	}

	FVector BaseLocation = AvatarActor->GetActorLocation();
	FRotator BaseRotation = AvatarActor->GetActorRotation();
	if (USkeletalMeshComponent* MeshComponent = GetPdCharacterFromActorInfo() ? GetPdCharacterFromActorInfo()->GetMesh() : nullptr;
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

FVector USummonAbility::ProjectSummonLocationToGround(const FVector& CandidateLocation) const
{
	const FSummonSkillConfig* SummonConfig = GetSummonConfig();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
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

void USummonAbility::DeactivateSummonNiagara(AActor* SummonedActor) const
{
	const FSummonSkillConfig* SummonConfig = GetSummonConfig();
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

void USummonAbility::ActivateSummonNiagara(AActor* SummonedActor) const
{
	const FSummonSkillConfig* SummonConfig = GetSummonConfig();
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

void USummonAbility::FindConfiguredNiagaraComponents(AActor* SummonedActor, TArray<UNiagaraComponent*>& OutComponents) const
{
	const FSummonSkillConfig* SummonConfig = GetSummonConfig();
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

void USummonAbility::StartSummonRise()
{
	AActor* SummonedActor = SpawnedSummonActor.Get();
	const FSummonSkillConfig* SummonConfig = GetSummonConfig();
	if (!SummonedActor || !SummonConfig)
	{
		CancelAbilityForSkillExecutionFailure();
		return;
	}

	const float RiseDuration = static_cast<float>(FMath::Max(SummonConfig->RiseDuration, 0.0));
	if (RiseDuration <= KINDA_SMALL_NUMBER || SummonRiseStartLocation.Equals(SummonRiseFinalLocation))
	{
		FinishSummonRiseAndActivateLaser();
		return;
	}

	UWorld* World = SummonedActor->GetWorld();
	if (!World)
	{
		CancelAbilityForSkillExecutionFailure();
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

void USummonAbility::HandleSummonRiseTick()
{
	AActor* SummonedActor = SpawnedSummonActor.Get();
	const FSummonSkillConfig* SummonConfig = GetSummonConfig();
	UWorld* World = SummonedActor ? SummonedActor->GetWorld() : nullptr;
	if (!SummonedActor || !SummonConfig || !World)
	{
		CancelAbilityForSkillExecutionFailure();
		return;
	}

	const float RiseDuration = static_cast<float>(FMath::Max(SummonConfig->RiseDuration, static_cast<double>(KINDA_SMALL_NUMBER)));
	const float ElapsedTime = World->GetTimeSeconds() - SummonRiseStartTime;
	const float Alpha = FMath::Clamp(ElapsedTime / RiseDuration, 0.0f, 1.0f);
	const FVector NewLocation = FMath::Lerp(SummonRiseStartLocation, SummonRiseFinalLocation, Alpha);
	SummonedActor->SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.0f - KINDA_SMALL_NUMBER)
	{
		FinishSummonRiseAndActivateLaser();
	}
}

void USummonAbility::FinishSummonRiseAndActivateLaser()
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
	const FSummonSkillConfig* SummonConfig = GetSummonConfig();
	if (!SummonedActor || !SummonConfig)
	{
		CancelAbilityForSkillExecutionFailure();
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

	if (const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
		SkillDataAsset
		&& SkillDataAsset->GetResolvedDamageConfig().GameplayEffectClass
		&& CalculateSummonTriggerDamageMagnitude() > 0.0f)
	{
		const float TriggerDelay = static_cast<float>(FMath::Max(SkillDataAsset->SummonSettings.TriggerDamageDelay, 0.0));
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

void USummonAbility::StartSummonLifetimeTimerOrEnd()
{
	if (SummonDurationTask)
	{
		return;
	}

	const float ActiveDuration = ResolveSummonLifetimeTimerDuration();
	if (ActiveDuration <= KINDA_SMALL_NUMBER)
	{
		K2_EndAbility();
		return;
	}

	SummonDurationTask = UAbilityTask_WaitDelay::WaitDelay(this, ActiveDuration);
	if (!SummonDurationTask)
	{

		K2_EndAbility();
		return;
	}

	SummonDurationTask->OnFinish.AddDynamic(this, &ThisClass::HandleSummonDurationFinished);
	SummonDurationTask->ReadyForActivation();
}

float USummonAbility::ResolveSummonActiveDuration() const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	if (SkillDataAsset && SkillDataAsset->SkillType == EPdSkillType::Duration && SkillDataAsset->Time.Duration > 0.0)
	{
		return static_cast<float>(SkillDataAsset->Time.Duration);
	}

	const FSummonSkillConfig* SummonConfig = GetSummonConfig();
	return SummonConfig ? static_cast<float>(FMath::Max(SummonConfig->SummonedActorLifeSpan, 0.0)) : 0.0f;
}

float USummonAbility::ResolveSummonLifetimeTimerDuration() const
{
	const float ActiveDuration = ResolveSummonActiveDuration();
	const FSummonSkillConfig* SummonConfig = GetSummonConfig();
	if (!SummonConfig || !SummonConfig->bForceReplicateSpawnedActor || !SpawnedSummonActor.IsValid())
	{
		return ActiveDuration;
	}

	return FMath::Max(
		ActiveDuration,
		static_cast<float>(FMath::Max(SummonConfig->MinimumReplicatedActorLifetime, 0.0)));
}

void USummonAbility::BindSummonTriggerDamage(AActor* SummonedActor)
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

void USummonAbility::UnbindSummonTriggerDamage()
{
	if (UPrimitiveComponent* TriggerComponent = SummonTriggerComponent.Get())
	{
		TriggerComponent->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandleSummonTriggerBeginOverlap);
		TriggerComponent->OnComponentEndOverlap.RemoveDynamic(this, &ThisClass::HandleSummonTriggerEndOverlap);
	}
	SummonOverlappingActors.Reset();
}

UPrimitiveComponent* USummonAbility::FindSummonTriggerComponent(AActor* SummonedActor) const
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

void USummonAbility::EnableSummonTriggerDamage()
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

void USummonAbility::DisableSummonTriggerDamage()
{
	bSummonTriggerDamageActive = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SummonTriggerDamageTickTimerHandle);
	}
	SummonTriggerDamageTickTimerHandle.Invalidate();
}

void USummonAbility::StartSummonTriggerDamageTickIfNeeded()
{
	if (SummonTriggerDamageTickTimerHandle.IsValid())
	{
		return;
	}

	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
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

void USummonAbility::HandleSummonTriggerDamageTick()
{
	if (!bSummonTriggerDamageActive)
	{
		return;
	}

	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
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

void USummonAbility::ApplySummonTriggerDamageToExistingOverlaps()
{
	UPrimitiveComponent* TriggerComponent = SummonTriggerComponent.Get();
	if (!TriggerComponent)
	{
		return;
	}

	TriggerComponent->UpdateOverlaps();

	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	const bool bAllowRepeatedDamage = ShouldRepeatSummonTriggerDamage(SkillDataAsset);

	TArray<AActor*> OverlappingActors;
	TriggerComponent->GetOverlappingActors(OverlappingActors, ACharacterBase::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		TrackSummonTriggerOverlap(OverlappingActor);
		ApplySummonTriggerDamage(OverlappingActor, bAllowRepeatedDamage);
	}
}

void USummonAbility::TrackSummonTriggerOverlap(AActor* OtherActor)
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

void USummonAbility::UntrackSummonTriggerOverlap(AActor* OtherActor)
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

void USummonAbility::ApplySummonTriggerDamage(AActor* HitActor, const bool bAllowRepeatedDamage)
{
	AActor* SourceActor = GetAvatarActorFromActorInfo();
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
	if (!bAllowRepeatedDamage)
	{
		DamagedSummonTriggerActors.Add(HitActorKey);
	}


}

FGameplayEffectSpecHandle USummonAbility::MakeSummonTriggerDamageSpec(const float DamageMagnitude) const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	const FSkillGameplayEffectConfig TriggerDamage = SkillDataAsset ? SkillDataAsset->GetResolvedDamageConfig() : FSkillGameplayEffectConfig();
	if (!SkillDataAsset || !TriggerDamage.GameplayEffectClass || DamageMagnitude <= 0.0f)
	{

		return FGameplayEffectSpecHandle();
	}

	return MakeConfiguredDamageEffectSpec(
		TriggerDamage,
		DamageMagnitude,
		SpawnedSummonActor.IsValid() ? SpawnedSummonActor.Get() : nullptr);
}

float USummonAbility::CalculateSummonTriggerDamageMagnitude() const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{
		return 0.0f;
	}

	return CalculateSkillDamageMagnitude(SkillDataAsset->GetResolvedDamageConfig());
}

void USummonAbility::CleanupSummonTasks()
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

void USummonAbility::HandleSummonMontageTriggerEvent(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	TryCommitAndStartSummon();
}

void USummonAbility::HandleSummonMontageFinished()
{
	SummonMontageTask = nullptr;

	if (bSummonStarted)
	{
		const AActor* AvatarActor = GetAvatarActorFromActorInfo();
		if (!AvatarActor || !AvatarActor->HasAuthority())
		{
			K2_EndAbilityLocally();
		}
		return;
	}

	TryCommitAndStartSummon();
}

void USummonAbility::HandleSummonMontageInterrupted()
{
	SummonMontageTask = nullptr;

	if (bSummonStarted)
	{
		return;
	}

	TryCommitAndStartSummon();
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (bSummonStarted && (!AvatarActor || !AvatarActor->HasAuthority()))
	{
		K2_EndAbilityLocally();
	}
}

void USummonAbility::HandleSummonDurationFinished()
{
	SummonDurationTask = nullptr;
	FinishAbilityFromDuration();
}

void USummonAbility::HandleSummonTriggerBeginOverlap(
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

void USummonAbility::HandleSummonTriggerEndOverlap(
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
