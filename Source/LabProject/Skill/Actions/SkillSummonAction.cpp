#include "Skill/Actions/SkillSummonAction.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Skill/SkillGroundProjection.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "Skill/Actors/SkillSummonActor.h"
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
#include "Skill/Actions/SkillTriggerDamage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillSummonAction)

namespace
{
	const FName SummonTriggerComponentName(TEXT("Box"));
}

void USkillSummonAction::OnStart()
{
	const auto* ActorInfo = GetAbility()->GetCurrentActorInfo();

	SummonMontageTask = nullptr;
	WaitSummonMontageTriggerTask = nullptr;
	SummonDurationTask = nullptr;
	SpawnedSummonActor.Reset();
	SummonRiseStartLocation = FVector::ZeroVector;
	SummonRiseFinalLocation = FVector::ZeroVector;
	SummonRiseFinalRotation = FRotator::ZeroRotator;
	SummonRiseStartTime = 0.0f;
	bSummonStarted = false;
	bSummonRiseFinished = false;
	if (!SummonTriggerDamage)
	{
		SummonTriggerDamage = NewObject<USkillTriggerDamage>(this);
	}
	SummonTriggerDamage->Reset();

	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	const FSkillSummonSettings* SummonConfig = GetSummonConfig();
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !SkillDataAsset || !SummonConfig)
	{
		Finish(false);
		return;
	}

	if (!SummonConfig->SummonedActorClass)
	{
		Finish(false);
		return;
	}

	SummonTriggerDamage->Configure(
		SkillDataAsset->Damage,
		FSkillTriggerHit::CreateUObject(this, &ThisClass::ApplySummonTriggerDamage));
	SummonTriggerDamage->SetDamageActive(false);

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
	}
	SummonRiseTimerHandle.Invalidate();
	SummonTriggerDamageDelayTimerHandle.Invalidate();

	if (SummonTriggerDamage)
	{
		SummonTriggerDamage->Reset();
	}

	if (SpawnedSummonActor.IsValid() && SpawnedSummonActor->HasAuthority())
	{
		SpawnedSummonActor->Destroy();
	}

	SpawnedSummonActor.Reset();
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
		GetAbility()->StartConfiguredCharacterOverlay();
		GetAbility()->StartConfiguredDefaultFX();
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

	GetAbility()->StartConfiguredCharacterOverlay();
	GetAbility()->StartConfiguredDefaultFX();
	if (!SpawnSummonedActor())
	{
		Finish(false);
		return;
	}

	GetAbility()->SpawnConfiguredCharacterDecal();
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
	if (!SummonedActor || !SummonedActor->HasAuthority() || !SummonTriggerDamage)
	{
		return;
	}

	UPrimitiveComponent* TriggerComponent =
		USkillTriggerDamage::FindTriggerComponent(SummonedActor, SummonTriggerComponentName, false);
	if (!TriggerComponent)
	{
		return;
	}

	if (TriggerComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		TriggerComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	TriggerComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SummonTriggerDamage->Bind(TriggerComponent, false);
}

void USkillSummonAction::EnableSummonTriggerDamage()
{
	AActor* SummonedActor = SpawnedSummonActor.Get();
	if (!SummonedActor || !SummonedActor->HasAuthority() || !SummonTriggerDamage)
	{
		return;
	}

	if (!SummonTriggerDamage->IsBound(SummonedActor))
	{
		BindSummonTriggerDamage(SummonedActor);
	}

	if (SummonTriggerDamage->IsBound(SummonedActor))
	{
		SummonTriggerDamage->SetDamageActive(true);
	}
}

// 트리거 추적기가 피해 차례라고 알리면 시전자와 소환물 자신을 빼고 피해를 적용한다. 피해를 시도했으면 true.
bool USkillSummonAction::ApplySummonTriggerDamage(AActor* DamageSourceActor, AActor* HitActor)
{
	AActor* SourceActor = GetAbility()->GetAvatarActorFromActorInfo();
	if (!IsValid(HitActor) || HitActor == SourceActor || HitActor == DamageSourceActor)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = nullptr;
	UAbilitySystemComponent* TargetASC = nullptr;
	if (!ResolveDamageableCharacterTarget(SourceActor, HitActor, SourceASC, TargetASC))
	{
		return false;
	}

	const FGameplayEffectSpecHandle DamageSpecHandle = MakeSummonTriggerDamageSpec(CalculateSummonTriggerDamageMagnitude());
	if (!DamageSpecHandle.IsValid())
	{
		return false;
	}

	ApplyDamageWithConfiguredStatus(*SourceASC, *TargetASC, *DamageSpecHandle.Data);
	return true;
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
