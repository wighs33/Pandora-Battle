#include "AbilitySystem/Ability/MissileAbility.h"

#include "Abilities/GameplayAbilityTargetActor_GroundTrace.h"
#include "Abilities/GameplayAbilityTargetActor_Trace.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilitySystemComponent.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/CollisionProfile.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "Kismet/KismetSystemLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MissileAbility)

namespace
{
	constexpr float MissileTargetTrackingInterval = 0.05f;

	const FMissileSkillConfig* GetMissileConfigFromSkill(const USkillDefinition* SkillDataAsset)
	{
		return SkillDataAsset && SkillDataAsset->SkillDataType == EPdSkillDataType::Missile
			? &SkillDataAsset->Missile
			: nullptr;
	}

}

UMissileAbility::UMissileAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(LabGameplayTags::GameplayAbility_Missile);
	SetAssetTags(AssetTags);

	ActivationOwnedTags.AddTag(LabGameplayTags::GameplayAbility_Missile_Active);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::Action_Attack);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::Action_Punch);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::Action_RangedAttack);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::GameplayAbility_ShootProjectile);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::GameplayAbility_AOEAttack);
}

void UMissileAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	static_cast<void>(TriggerEventData);

	WaitTargetDataTask = nullptr;
	MissileMontageTask = nullptr;
	WaitMissileMontageTriggerTask = nullptr;
	ConfirmedMissileLocation = FVector::ZeroVector;
	MissileTargetingOrigin = FVector::ZeroVector;
	bHasMissileTargetingOrigin = false;
	TrackedMissileTargetActor.Reset();
	DamageTicksApplied = 0;
	PlannedDamageTickCount = 0;
	bMissileLaunched = false;
	bMissileDurationFinished = true;
	bDamageSequenceFinished = true;

	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !SkillDataAsset || !MissileConfig)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const EPdSkillDataType ResolvedSkillDataType = SkillDataAsset->GetResolvedSkillDataType();
	if (ResolvedSkillDataType != EPdSkillDataType::Missile)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MissileTargetingOrigin = ActorInfo->AvatarActor->GetActorLocation();
	bHasMissileTargetingOrigin = true;

	const FSkillGameplayEffectConfig DamageConfig = SkillDataAsset->GetResolvedDamageConfig();



	StartWaitMissileMontageTriggerTask();

	if (!GetResolvedMissileMontage())
	{

		LaunchMissileFromResolvedTarget();
		return;
	}

	if (!StartMissileMontageTask())
	{
		LaunchMissileFromResolvedTarget();
	}
}

void UMissileAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	RestoreAvatarMovementForAbility();

	if (WaitTargetDataTask)
	{
		WaitTargetDataTask->EndTask();
		WaitTargetDataTask = nullptr;
	}

	if (MissileMontageTask)
	{
		MissileMontageTask->EndTask();
		MissileMontageTask = nullptr;
	}

	if (WaitMissileMontageTriggerTask)
	{
		WaitMissileMontageTriggerTask->EndTask();
		WaitMissileMontageTriggerTask = nullptr;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MissileDurationTimerHandle);
		World->GetTimerManager().ClearTimer(MissileTargetTrackingTimerHandle);
		World->GetTimerManager().ClearTimer(DamageDelayTimerHandle);
		World->GetTimerManager().ClearTimer(DamageTickTimerHandle);
	}

	TrackedMissileTargetActor.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UMissileAbility::StartWaitMissileMontageTriggerTask()
{
	const FGameplayTag TriggerTag = GetResolvedMontageTriggerEventTag();
	if (!TriggerTag.IsValid())
	{

		return;
	}

	WaitMissileMontageTriggerTask = CreateWaitGameplayEventTask(TriggerTag);
	if (!WaitMissileMontageTriggerTask)
	{

		return;
	}

	WaitMissileMontageTriggerTask->EventReceived.AddDynamic(this, &ThisClass::HandleMissileMontageTriggerEvent);
	WaitMissileMontageTriggerTask->ReadyForActivation();
}

bool UMissileAbility::StartMissileMontageTask()
{
	UAnimMontage* ResolvedMontage = GetResolvedMissileMontage();
	if (!ResolvedMontage)
	{
		return false;
	}

	MissileMontageTask = CreateDefaultMontageAndWaitTask(ResolvedMontage);
	if (!MissileMontageTask)
	{

		return false;
	}

	MissileMontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMissileMontageFinished);
	MissileMontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMissileMontageFinished);
	MissileMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMissileMontageInterrupted);
	MissileMontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMissileMontageInterrupted);
	MissileMontageTask->ReadyForActivation();



	return true;
}

UAnimMontage* UMissileAbility::GetResolvedMissileMontage() const
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	return MissileConfig ? MissileConfig->Animation.PrimaryMontage.Get() : nullptr;
}

FGameplayTag UMissileAbility::GetResolvedMontageTriggerEventTag() const
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	return MissileConfig && MissileConfig->Animation.PrimaryEventTag.IsValid()
		? MissileConfig->Animation.PrimaryEventTag
		: LabGameplayTags::Event_Montage_Trigger;
}

void UMissileAbility::LaunchMissileFromResolvedTarget()
{
	if (bMissileLaunched || !CanExecuteSkillPayload())
	{

		return;
	}

	FVector TargetLocation = FVector::ZeroVector;
	AActor* TargetActor = nullptr;
	if (!ResolveMissileTargetLocation(TargetLocation, TargetActor))
	{
		AActor* AvatarActor = GetAvatarActorFromActorInfo();
		if (!AvatarActor)
		{
			FinishMissileAbility(true);
			return;
		}
		TargetLocation = GetMissileTargetingOrigin()
			+ AvatarActor->GetActorForwardVector() * 800.0f;
	}

	bMissileLaunched = true;

	ConfirmMissileAtLocation(TargetLocation, TargetActor);
}

bool UMissileAbility::ResolveMissileTargetLocation(FVector& OutTargetLocation, AActor*& OutTargetActor) const
{
	OutTargetActor = nullptr;
	if (TryGetAutoTargetGroundLocation(OutTargetLocation, OutTargetActor))
	{
		return true;
	}

	return ResolveForwardGroundTargetLocation(OutTargetLocation);
}

bool UMissileAbility::ResolveForwardGroundTargetLocation(FVector& OutGroundLocation) const
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!MissileConfig || !AvatarActor || !World)
	{
		return false;
	}

	const double RawTargetDistance = MissileConfig->TargetingMaxRange > 0.0
		? MissileConfig->TargetingMaxRange
		: MissileConfig->AutoTargetSearchRadius;
	const float TargetDistance = static_cast<float>(FMath::Clamp(RawTargetDistance > 0.0 ? RawTargetDistance : 1200.0, 300.0, 3000.0));
	const FVector CandidateLocation = AvatarActor->GetActorLocation() + AvatarActor->GetActorForwardVector() * TargetDistance;
	const FVector TraceStart = CandidateLocation + FVector(0.0, 0.0, 1000.0);
	const FVector TraceEnd = CandidateLocation - FVector(0.0, 0.0, 10000.0);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MissileForwardGroundTrace), false, AvatarActor);
	FHitResult GroundHit;
	const bool bHitGround = World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams) && GroundHit.bBlockingHit;
	OutGroundLocation = bHitGround ? GroundHit.Location : CandidateLocation;



	return true;
}

void UMissileAbility::StartTargeting()
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	if (!MissileConfig || !MissileConfig->TargetActorClass)
	{

		FinishMissileAbility(true);
		return;
	}

	if (const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
		SkillDataAsset && SkillDataAsset->Movement.bLockMovementDuringDuration)
	{
		LockAvatarMovementForAbility();
	}

	WaitTargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(
		this,
		NAME_None,
		EGameplayTargetingConfirmation::UserConfirmed,
		MissileConfig->TargetActorClass);
	if (!WaitTargetDataTask)
	{
		FinishMissileAbility(true);
		return;
	}

	WaitTargetDataTask->ValidData.AddDynamic(this, &ThisClass::HandleTargetDataValid);
	WaitTargetDataTask->Cancelled.AddDynamic(this, &ThisClass::HandleTargetDataCancelled);

	if (AGameplayAbilityTargetActor* SpawnedTargetActor =
		BeginSpawningTargetDataActor(WaitTargetDataTask, MissileConfig->TargetActorClass))
	{
		ConfigureSpawnedTargetActor(SpawnedTargetActor);
		FinishSpawningTargetDataActor(WaitTargetDataTask, SpawnedTargetActor);
	}

	WaitTargetDataTask->ReadyForActivation();
}

void UMissileAbility::ConfirmMissileAtLocation(const FVector& TargetLocation, AActor* TargetActor)
{
	ConfirmedMissileLocation = ClampMissileTargetLocationToRange(TargetLocation);
	TrackedMissileTargetActor = IsValid(TargetActor) && IsMissileTargetLocationWithinRange(TargetLocation)
		? TargetActor
		: nullptr;
	RestoreAvatarMovementForAbility();

	if (ConfirmedMissileLocation.IsNearlyZero())
	{

		FinishMissileAbility(true);
		return;
	}



	LaunchMissile();
}

void UMissileAbility::LaunchMissile()
{
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{

		FinishMissileAbility(true);
		return;
	}

	SpawnConfiguredCharacterDecal();
	StartDurationMovementLock();
	SpawnMissileNiagara();
	StartMissileTargetTracking();
	StartMissileDurationTimer();

	StartDamageSequence();
}

void UMissileAbility::SpawnMissileNiagara()
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	if (!MissileConfig || !MissileConfig->MissileSystem)
	{
		return;
	}

	StartConfiguredMissilePresentation(ConfirmedMissileLocation);
}

void UMissileAbility::ApplyAimPositionToMissileNiagara()
{
	UpdateConfiguredMissilePresentationTarget(ConfirmedMissileLocation);
}

void UMissileAbility::StartMissileDurationTimer()
{
	const float MissileDuration = CalculateMissileDuration();
	bMissileDurationFinished = MissileDuration <= 0.0f;

	if (bMissileDurationFinished)
	{

		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		bMissileDurationFinished = true;
		return;
	}

	World->GetTimerManager().SetTimer(
		MissileDurationTimerHandle,
		this,
		&ThisClass::HandleMissileDurationFinished,
		MissileDuration,
		false);


}

void UMissileAbility::HandleMissileDurationFinished()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MissileDurationTimerHandle);
	}

	bMissileDurationFinished = true;

	StopConfiguredMissilePresentation();

	TryFinishMissileAbilityAfterWork();
}

void UMissileAbility::StartMissileTargetTracking()
{
	if (!TrackedMissileTargetActor.IsValid())
	{
		ApplyAimPositionToMissileNiagara();

		return;
	}

	if (!RefreshTrackedMissileTargetLocation())
	{

		ApplyAimPositionToMissileNiagara();
		return;
	}

	ApplyAimPositionToMissileNiagara();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		MissileTargetTrackingTimerHandle,
		this,
		&ThisClass::HandleMissileTargetTrackingTick,
		MissileTargetTrackingInterval,
		true);


}

void UMissileAbility::StopMissileTargetTracking()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MissileTargetTrackingTimerHandle);
	}
}

void UMissileAbility::HandleMissileTargetTrackingTick()
{
	const FVector PreviousLocation = ConfirmedMissileLocation;
	if (!RefreshTrackedMissileTargetLocation())
	{

		StopMissileTargetTracking();
		return;
	}

	ApplyAimPositionToMissileNiagara();

}

bool UMissileAbility::RefreshTrackedMissileTargetLocation()
{
	AActor* TargetActor = TrackedMissileTargetActor.Get();
	if (!IsValid(TargetActor) || TargetActor == GetAvatarActorFromActorInfo())
	{
		TrackedMissileTargetActor.Reset();
		return false;
	}

	if (const UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
		TargetASC && TargetASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead))
	{
		TrackedMissileTargetActor.Reset();
		return false;
	}

	FVector UpdatedLocation = FVector::ZeroVector;
	if (!ResolveTargetAimLocation(TargetActor, UpdatedLocation))
	{
		TrackedMissileTargetActor.Reset();
		return false;
	}

	if (!IsMissileTargetLocationWithinRange(UpdatedLocation))
	{
		// Keep ConfirmedMissileLocation at the last valid in-range point so the
		// Niagara missile cannot continue following a target beyond cast range.
		TrackedMissileTargetActor.Reset();
		return false;
	}

	ConfirmedMissileLocation = UpdatedLocation;
	return true;
}

void UMissileAbility::StartDamageSequence()
{
	PlannedDamageTickCount = CalculateDamageTickCount();
	DamageTicksApplied = 0;
	bDamageSequenceFinished = false;

	const float DamageStartDelay = GetMissileConfig()
		? static_cast<float>(FMath::Max(GetMissileConfig()->DamageStartDelay, 0.0))
		: 0.0f;

	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const APawn* AvatarPawn = Cast<APawn>(AvatarActor);
	const float TickDamageMagnitude = CalculateDamageMagnitudePerTick();
	const float PlannedTotalDamageMagnitude = TickDamageMagnitude * static_cast<float>(FMath::Max(PlannedDamageTickCount, 0));
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	const FSkillGameplayEffectConfig DamageConfig = SkillDataAsset
		? SkillDataAsset->GetResolvedDamageConfig()
		: FSkillGameplayEffectConfig();


	UWorld* World = GetWorld();
	if (!World)
	{

		FinishMissileAbility(true);
		return;
	}

	if (DamageStartDelay <= 0.0f)
	{
		HandleDamageDelayFinished();
		return;
	}

	World->GetTimerManager().SetTimer(
		DamageDelayTimerHandle,
		this,
		&ThisClass::HandleDamageDelayFinished,
		DamageStartDelay,
		false);


}

void UMissileAbility::HandleDamageDelayFinished()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageDelayTimerHandle);
	}

	const AActor* AvatarActor = GetAvatarActorFromActorInfo();


	HandleDamageTick();
}

void UMissileAbility::HandleDamageTick()
{
	if (PlannedDamageTickCount <= 0)
	{

		MarkDamageSequenceFinished();
		return;
	}

	++DamageTicksApplied;
	const float TickDamageMagnitude = CalculateDamageMagnitudePerTick();
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();


	if (K2_HasAuthority())
	{
		RefreshTrackedMissileTargetLocation();
		ApplyMissileDamageTick(TickDamageMagnitude);
	}

	if (DamageTicksApplied >= PlannedDamageTickCount)
	{
		MarkDamageSequenceFinished();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DamageTickTimerHandle,
			this,
			&ThisClass::HandleDamageTick,
			CalculateDamageInterval(),
			false);
	}
	else
	{
		FinishMissileAbility(true);
	}
}

void UMissileAbility::ApplyMissileDamageTick(const float TickDamageMagnitude)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!World)
	{

		return;
	}

	const float DamageRadius = CalculateDamageRadius();
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MissileDamage), false, AvatarActor);
	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(DamageRadius);

	MissileDamageHitActorKeys.Reset();
	MissileDamageOverlapResults.Reset();
	TArray<TWeakObjectPtr<AActor>> DamageTargets;
	if (AActor* TrackedTargetActor = TrackedMissileTargetActor.Get();
		IsValid(TrackedTargetActor) && TrackedTargetActor != AvatarActor)
	{

		MissileDamageHitActorKeys.Add(FObjectKey(TrackedTargetActor));
		DamageTargets.Add(TrackedTargetActor);
	}

	World->OverlapMultiByObjectType(
		MissileDamageOverlapResults,
		ConfirmedMissileLocation,
		FQuat::Identity,
		ObjectQueryParams,
		SphereShape,
		QueryParams);

	for (const FOverlapResult& OverlapResult : MissileDamageOverlapResults)
	{
		AActor* HitActor = OverlapResult.GetActor();
		if (!IsValid(HitActor) || HitActor == AvatarActor)
		{
			continue;
		}

		const FObjectKey HitActorKey(HitActor);
		if (MissileDamageHitActorKeys.Contains(HitActorKey))
		{
			continue;
		}

		MissileDamageHitActorKeys.Add(HitActorKey);
		DamageTargets.Add(HitActor);
	}

	for (const TWeakObjectPtr<AActor>& TargetPtr : DamageTargets)
	{
		AActor* HitActor = TargetPtr.Get();
		if (!IsValid(HitActor))
		{
			continue;
		}

		ApplyEffectToHitActor(HitActor, TickDamageMagnitude);
	}

	DrawDebugDamageRadius(TEXT("DamageTick"));


}

void UMissileAbility::ApplyEffectToHitActor(AActor* HitActor, const float TickDamageMagnitude)
{
	if (!IsValid(HitActor))
	{
		return;
	}

	const ACharacterBase* SourceCharacter = Cast<ACharacterBase>(GetAvatarActorFromActorInfo());
	const ACharacterBase* TargetCharacter = Cast<ACharacterBase>(HitActor);
	if (SourceCharacter && TargetCharacter && !SourceCharacter->CanDamageCharacterByTeam(TargetCharacter))
	{

		return;
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	if (!SourceASC || !TargetASC)
	{

		return;
	}

	FGameplayEffectSpecHandle DamageSpecHandle = MakeDamageEffectSpec(TickDamageMagnitude);
	if (!DamageSpecHandle.IsValid() || !DamageSpecHandle.Data.IsValid())
	{

		return;
	}

	const FActiveGameplayEffectHandle AppliedHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetASC);

}

void UMissileAbility::ConfigureSpawnedTargetActor(AGameplayAbilityTargetActor* SpawnedActor)
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	if (!SpawnedActor || !MissileConfig)
	{
		return;
	}

	SpawnedActor->StartLocation = MakeTargetStartLocation();
	SpawnedActor->ReticleClass = nullptr;
	SpawnedActor->bDebug = LabSkillDebug::IsDrawingEnabled() && MissileConfig->bDebugTargeting;

	if (AGameplayAbilityTargetActor_Trace* TraceActor = Cast<AGameplayAbilityTargetActor_Trace>(SpawnedActor))
	{
		TraceActor->MaxRange = static_cast<float>(MissileConfig->TargetingMaxRange);
		TraceActor->TraceProfile = FCollisionProfileName(MissileConfig->TargetingTraceProfileName);
		TraceActor->bTraceAffectsAimPitch = MissileConfig->bTargetingTraceAffectsAimPitch;
	}

	if (AGameplayAbilityTargetActor_GroundTrace* GroundTraceActor = Cast<AGameplayAbilityTargetActor_GroundTrace>(SpawnedActor))
	{
		GroundTraceActor->CollisionRadius = static_cast<float>(MissileConfig->TargetingCollisionRadius);
		GroundTraceActor->CollisionHeight = static_cast<float>(MissileConfig->TargetingCollisionHeight);
	}

}

FGameplayAbilityTargetingLocationInfo UMissileAbility::MakeTargetStartLocation()
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	if (MissileConfig && !MissileConfig->NiagaraSpawnSocketName.IsNone())
	{
		return MakeTargetLocationInfoFromOwnerSkeletalMeshComponent(MissileConfig->NiagaraSpawnSocketName);
	}

	return MakeTargetLocationInfoFromOwnerActor();
}

bool UMissileAbility::TryGetAttackTargetGroundLocation(FVector& OutGroundLocation, AActor*& OutTargetActor) const
{
	OutTargetActor = nullptr;
	AActor* AttackTarget = GetAttackTargetFromAvatar();
	if (!IsValid(AttackTarget))
	{
		return false;
	}

	if (!ResolveTargetAimLocation(AttackTarget, OutGroundLocation))
	{
		return false;
	}

	if (!IsMissileTargetLocationWithinRange(OutGroundLocation))
	{
		return false;
	}

	OutTargetActor = AttackTarget;
	return true;
}

AActor* UMissileAbility::FindAutoTargetActor() const
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	const ACharacterBase* SourceCharacter = Cast<ACharacterBase>(AvatarActor);
	if (!MissileConfig || !AvatarActor || !World || MissileConfig->AutoTargetSearchRadius <= 0.0)
	{
		return nullptr;
	}

	const FVector SearchOrigin = AvatarActor->GetActorLocation()
		+ (AvatarActor->GetActorForwardVector() * static_cast<float>(MissileConfig->AutoTargetForwardOffset));
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MissileAutoTargetSearch), false, AvatarActor);
	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(static_cast<float>(MissileConfig->AutoTargetSearchRadius));

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		SearchOrigin,
		FQuat::Identity,
		ObjectQueryParams,
		SphereShape,
		QueryParams);

	AActor* BestTarget = nullptr;
	double BestDistanceSq = TNumericLimits<double>::Max();
	int32 CandidateCount = 0;
	int32 RejectedTeamCount = 0;
	int32 RejectedDeadCount = 0;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* CandidateActor = OverlapResult.GetActor();
		ACharacterBase* CandidateCharacter = Cast<ACharacterBase>(CandidateActor);
		if (!IsValid(CandidateActor) || CandidateActor == AvatarActor || !CandidateCharacter)
		{
			continue;
		}

		++CandidateCount;
		if (const UAbilitySystemComponent* CandidateASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(CandidateActor);
			CandidateASC && CandidateASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead))
		{
			++RejectedDeadCount;
			continue;
		}

		if (SourceCharacter && !SourceCharacter->CanDamageCharacterByTeam(CandidateCharacter))
		{
			++RejectedTeamCount;
			continue;
		}

		FVector CandidateAimLocation = FVector::ZeroVector;
		if (!ResolveTargetAimLocation(CandidateActor, CandidateAimLocation)
			|| !IsMissileTargetLocationWithinRange(CandidateAimLocation))
		{
			continue;
		}

		const double DistanceSq = FVector::DistSquared(GetMissileTargetingOrigin(), CandidateAimLocation);
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestTarget = CandidateActor;
		}
	}



	return BestTarget;
}

bool UMissileAbility::TryGetAutoTargetGroundLocation(FVector& OutGroundLocation, AActor*& OutTargetActor) const
{
	OutTargetActor = nullptr;
	if (TryGetAttackTargetGroundLocation(OutGroundLocation, OutTargetActor))
	{

		return true;
	}

	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	if (!MissileConfig || MissileConfig->AutoTargetSearchRadius <= 0.0)
	{
		return false;
	}

	AActor* ClosestEnemy = FindAutoTargetActor();
	if (!IsValid(ClosestEnemy))
	{
		return false;
	}

	const bool bProjected = ResolveTargetAimLocation(ClosestEnemy, OutGroundLocation);
	OutTargetActor = bProjected ? ClosestEnemy : nullptr;


	return bProjected;
}

bool UMissileAbility::ResolveTargetAimLocation(AActor* TargetActor, FVector& OutAimLocation) const
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	const FName TargetSocketName = MissileConfig ? MissileConfig->TargetSocketName : NAME_None;
	if (!TargetSocketName.IsNone())
	{
		if (const ACharacterBase* TargetCharacter = Cast<ACharacterBase>(TargetActor);
			TargetCharacter && TargetCharacter->GetMesh() && TargetCharacter->GetMesh()->DoesSocketExist(TargetSocketName))
		{
			OutAimLocation = TargetCharacter->GetMesh()->GetSocketLocation(TargetSocketName);
			return true;
		}

		if (const USkeletalMeshComponent* TargetMesh = Cast<USkeletalMeshComponent>(TargetActor->GetComponentByClass(USkeletalMeshComponent::StaticClass()));
			TargetMesh && TargetMesh->DoesSocketExist(TargetSocketName))
		{
			OutAimLocation = TargetMesh->GetSocketLocation(TargetSocketName);
			return true;
		}


	}

	OutAimLocation = TargetActor->GetActorLocation();
	return true;
}

bool UMissileAbility::IsMissileTargetLocationWithinRange(const FVector& TargetLocation) const
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	if (!MissileConfig || MissileConfig->TargetingMaxRange <= 0.0 || TargetLocation.ContainsNaN())
	{
		return false;
	}

	const double MaxRange = MissileConfig->TargetingMaxRange;
	return FVector::DistSquared(GetMissileTargetingOrigin(), TargetLocation) <= FMath::Square(MaxRange);
}

FVector UMissileAbility::ClampMissileTargetLocationToRange(const FVector& TargetLocation) const
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	if (!MissileConfig || MissileConfig->TargetingMaxRange <= 0.0 || TargetLocation.ContainsNaN())
	{
		return TargetLocation;
	}

	const FVector TargetingOrigin = GetMissileTargetingOrigin();
	const FVector TargetOffset = TargetLocation - TargetingOrigin;
	const double MaxRange = MissileConfig->TargetingMaxRange;
	if (TargetOffset.SizeSquared() <= FMath::Square(MaxRange))
	{
		return TargetLocation;
	}

	return TargetingOrigin + TargetOffset.GetSafeNormal() * MaxRange;
}

FVector UMissileAbility::GetMissileTargetingOrigin() const
{
	if (bHasMissileTargetingOrigin)
	{
		return MissileTargetingOrigin;
	}

	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	return AvatarActor ? AvatarActor->GetActorLocation() : FVector::ZeroVector;
}

AActor* UMissileAbility::ResolveTargetDataActor(const FGameplayAbilityTargetDataHandle& Data) const
{
	FHitResult HitResult = UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(Data, 0);
	AActor* HitActor = HitResult.GetActor();
	return IsValid(HitActor) && HitActor->IsA<APawn>() ? HitActor : nullptr;
}

FVector UMissileAbility::ResolveTargetDataLocation(const FGameplayAbilityTargetDataHandle& Data) const
{
	FHitResult HitResult = UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(Data, 0);
	FVector ResolvedLocation = HitResult.Location.IsNearlyZero()
		? UAbilitySystemBlueprintLibrary::GetTargetDataEndPoint(Data, 0)
		: HitResult.Location;

	AActor* HitActor = HitResult.GetActor();
	if (!IsValid(HitActor) || !HitActor->IsA<APawn>())
	{
		return ResolvedLocation;
	}

	FVector TargetAimLocation = FVector::ZeroVector;
	if (ResolveTargetAimLocation(HitActor, TargetAimLocation))
	{
		return TargetAimLocation;
	}

	return ResolvedLocation;
}

FGameplayEffectSpecHandle UMissileAbility::MakeDamageEffectSpec(const float DamageMagnitude) const
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	const FSkillGameplayEffectConfig DamageConfig = SkillDataAsset ? SkillDataAsset->GetResolvedDamageConfig() : FSkillGameplayEffectConfig();
	if (!MissileConfig || !DamageConfig.GameplayEffectClass)
	{

		return FGameplayEffectSpecHandle();
	}

	return MakeConfiguredDamageEffectSpec(DamageConfig, DamageMagnitude);
}

const FMissileSkillConfig* UMissileAbility::GetMissileConfig() const
{
	return GetMissileConfigFromSkill(GetSourceSkillDataAsset());
}

float UMissileAbility::CalculateMissileDuration() const
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	return MissileConfig ? static_cast<float>(FMath::Max(MissileConfig->MissileDuration, 0.0)) : 0.0f;
}

float UMissileAbility::CalculateDamageRadius() const
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	if (!MissileConfig)
	{
		return 0.0f;
	}

	const double BaseRadius = FMath::Max(MissileConfig->DamageRadius, 0.0);
	return FMath::Max(static_cast<float>(BaseRadius), 0.0f);
}

float UMissileAbility::CalculateDamageMagnitudePerTick() const
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	if (!MissileConfig)
	{
		return 0.0f;
	}

	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset
		? CalculateSkillDamageMagnitude(SkillDataAsset->GetResolvedDamageConfig())
		: 0.0f;
}

int32 UMissileAbility::CalculateDamageTickCount() const
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	if (!MissileConfig)
	{
		return 0;
	}

	const float Duration = CalculateDamageApplicationDuration();
	const float Interval = CalculateDamageInterval();
	return FMath::Max(FMath::CeilToInt(Duration / Interval), 1);
}

float UMissileAbility::CalculateDamageApplicationDuration() const
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	return MissileConfig ? static_cast<float>(FMath::Max(MissileConfig->DamageApplicationDuration, 0.0)) : 0.0f;
}

float UMissileAbility::CalculateDamageInterval() const
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	return MissileConfig ? static_cast<float>(FMath::Max(MissileConfig->DamageInterval, 0.05)) : 0.05f;
}

bool UMissileAbility::ShouldDrawDebugDamageRadius() const
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	return LabSkillDebug::IsDrawingEnabled()
		&& MissileConfig
		&& MissileConfig->bDrawDebugDamageRadius;
}

void UMissileAbility::DrawDebugDamageRadius(const TCHAR* Context) const
{
	if (!ShouldDrawDebugDamageRadius())
	{
		return;
	}

	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	if (!World || !MissileConfig)
	{
		return;
	}

	DrawDebugSphere(
		World,
		ConfirmedMissileLocation,
		CalculateDamageRadius(),
		32,
		FColor::Orange,
		false,
		static_cast<float>(FMath::Max(MissileConfig->DebugDamageRadiusDrawTime, 0.0)));


}

void UMissileAbility::MarkDamageSequenceFinished()
{
	bDamageSequenceFinished = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageDelayTimerHandle);
		World->GetTimerManager().ClearTimer(DamageTickTimerHandle);
	}



	TryFinishMissileAbilityAfterWork();
}

void UMissileAbility::TryFinishMissileAbilityAfterWork()
{
	if (!bDamageSequenceFinished || !bMissileDurationFinished)
	{
		return;
	}

	FinishMissileAbility(false);
}

void UMissileAbility::FinishMissileAbility(const bool bWasCancelled)
{
	if (bWasCancelled)
	{
		CancelAbilityForSkillExecutionFailure();
		return;
	}

	K2_EndAbility();
}

void UMissileAbility::HandleTargetDataValid(const FGameplayAbilityTargetDataHandle& Data)
{
	if (WaitTargetDataTask)
	{
		WaitTargetDataTask->EndTask();
		WaitTargetDataTask = nullptr;
	}

	ConfirmMissileAtLocation(ResolveTargetDataLocation(Data), ResolveTargetDataActor(Data));
}

void UMissileAbility::HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data)
{
	static_cast<void>(Data);
	LaunchMissileFromResolvedTarget();
}

void UMissileAbility::HandleMissileMontageTriggerEvent(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	LaunchMissileFromResolvedTarget();
}

void UMissileAbility::HandleMissileMontageFinished()
{
	MissileMontageTask = nullptr;

	if (!bMissileLaunched)
	{
		LaunchMissileFromResolvedTarget();
	}
}

void UMissileAbility::HandleMissileMontageInterrupted()
{
	MissileMontageTask = nullptr;

	if (bMissileLaunched)
	{
		return;
	}

	LaunchMissileFromResolvedTarget();
}
