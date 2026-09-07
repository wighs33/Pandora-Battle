#include "AbilitySystem/Ability/MissileAbility.h"

#include "Component/AbilitySystem/Ability/AbilityPresentationRuntime.h"
#include "Abilities/GameplayAbilityTargetActor_GroundTrace.h"
#include "Abilities/GameplayAbilityTargetActor_Trace.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystem/TargetValidator.h"
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
#include "UObject/ObjectKey.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MissileAbility)

namespace
{
	constexpr float MissileTargetTrackingInterval = 0.05f;

	const FMissileSkillConfig* GetMissileConfigFromSkill(const USkillDefinition* SkillDataAsset)
	{
		return SkillDataAsset && SkillDataAsset->SkillDataType == ESkillDataType::Missile
			? &SkillDataAsset->Missile
			: nullptr;
	}

	bool IsFiniteMissileLocation(const FVector& Location)
	{
		return FMath::IsFinite(Location.X)
			&& FMath::IsFinite(Location.Y)
			&& FMath::IsFinite(Location.Z);
	}

	ACharacterBase* ResolveMissileTraceCharacter(AActor* HitActor)
	{
		TSet<TObjectPtr<AActor>> VisitedActors;
		AActor* CurrentActor = HitActor;
		for (int32 Depth = 0; IsValid(CurrentActor) && Depth < 8; ++Depth)
		{
			if (ACharacterBase* Character = Cast<ACharacterBase>(CurrentActor))
			{
				return Character;
			}

			if (VisitedActors.Contains(CurrentActor))
			{
				break;
			}
			VisitedActors.Add(CurrentActor);

			AActor* ParentActor = CurrentActor->GetAttachParentActor();
			CurrentActor = IsValid(ParentActor)
				? ParentActor
				: CurrentActor->GetOwner();
		}

		return nullptr;
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
	ActiveMissileTargetActors.Reset();
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

	const ESkillDataType ResolvedSkillDataType = SkillDataAsset->GetResolvedSkillDataType();
	if (ResolvedSkillDataType != ESkillDataType::Missile)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

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

void UMissileAbility::OnAbilityEnding()
{
	Super::OnAbilityEnding();
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

	ActiveMissileTargetActors.Reset();
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
	if (!ResolveMissileTargetLocation(TargetLocation))
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

	ConfirmMissileAtLocation(TargetLocation);
}

bool UMissileAbility::ResolveMissileTargetLocation(
	FVector& OutTargetLocation) const
{
	if (TryGetAutoTargetGroundLocation(OutTargetLocation))
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

void UMissileAbility::ConfirmMissileAtLocation(const FVector& TargetLocation)
{
	const FVector ClampedTargetLocation =
		ClampMissileTargetLocationToRange(TargetLocation);
	RestoreAvatarMovementForAbility();

	if (ClampedTargetLocation.IsNearlyZero())
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

	GetPresentationRuntime().SpawnConfiguredCharacterDecal(*this);
	StartDurationMovementLock();
	GetPresentationRuntime().StartConfiguredGroundFX(*this);
	StartMissilePresentation();
	StartMissileTargetTracking();
	StartMissileDurationTimer();

	StartDamageSequence();
}

void UMissileAbility::StartMissilePresentation()
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	if (!MissileConfig || !MissileConfig->MissileSystem)
	{
		return;
	}

	GetPresentationRuntime().StartConfiguredMissilePresentation(*this);
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

	StopMissileTargetTracking();
	ActiveMissileTargetActors.Reset();
	GetPresentationRuntime().StopConfiguredMissilePresentation(*this);

	TryFinishMissileAbilityAfterWork();
}

void UMissileAbility::StartMissileTargetTracking()
{
	RefreshMissileTargets();
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	const bool bNeedsLocalDebugRefresh = LabSkillDebug::IsDrawingEnabled()
		&& MissileConfig
		&& MissileConfig->bDebugTargeting;
	if (!K2_HasAuthority() && !bNeedsLocalDebugRefresh)
	{
		return;
	}

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
	RefreshMissileTargets();
}

void UMissileAbility::RefreshMissileTargets()
{
	DrawDebugTargetingRange();

	if (!K2_HasAuthority())
	{
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	if (!AvatarActor
		|| !World
		|| !MissileConfig
		|| MissileConfig->TargetingMaxRange <= 0.0)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(MissileActiveTargetSearch),
		false,
		AvatarActor);
	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(
		static_cast<float>(MissileConfig->TargetingMaxRange));

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		GetMissileTargetingOrigin(),
		FQuat::Identity,
		ObjectQueryParams,
		SphereShape,
		QueryParams);

	TSet<FObjectKey> AddedTargetKeys;
	TArray<TWeakObjectPtr<AActor>> NewTargetActors;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		if (!IsEligibleMissileTargetActor(TargetActor))
		{
			continue;
		}

		FVector TargetLocation = FVector::ZeroVector;
		if (!ResolveTargetAimLocation(TargetActor, TargetLocation)
			|| !IsMissileTargetLocationWithinRange(TargetLocation))
		{
			continue;
		}

		const FObjectKey TargetKey(TargetActor);
		if (!AddedTargetKeys.Contains(TargetKey))
		{
			AddedTargetKeys.Add(TargetKey);
			NewTargetActors.Add(TargetActor);
		}
	}

	NewTargetActors.Sort([](
		const TWeakObjectPtr<AActor>& Left,
		const TWeakObjectPtr<AActor>& Right)
	{
		const AActor* LeftActor = Left.Get();
		const AActor* RightActor = Right.Get();
		return (LeftActor ? LeftActor->GetUniqueID() : 0)
			< (RightActor ? RightActor->GetUniqueID() : 0);
	});

	bool bTargetsChanged = ActiveMissileTargetActors.Num() != NewTargetActors.Num();
	if (!bTargetsChanged)
	{
		for (int32 Index = 0; Index < NewTargetActors.Num(); ++Index)
		{
			if (ActiveMissileTargetActors[Index] != NewTargetActors[Index])
			{
				bTargetsChanged = true;
				break;
			}
		}
	}

	if (!bTargetsChanged)
	{
		return;
	}

	ActiveMissileTargetActors = MoveTemp(NewTargetActors);
	TArray<AActor*> PresentationTargets;
	PresentationTargets.Reserve(ActiveMissileTargetActors.Num());
	for (const TWeakObjectPtr<AActor>& TargetActor : ActiveMissileTargetActors)
	{
		if (TargetActor.IsValid())
		{
			PresentationTargets.Add(TargetActor.Get());
		}
	}

	GetPresentationRuntime().UpdateConfiguredMissilePresentationTargets(PresentationTargets);
}

void UMissileAbility::StartDamageSequence()
{
	PlannedDamageTickCount = CalculateDamageTickCount();
	DamageTicksApplied = 0;
	bDamageSequenceFinished = false;

	const float DamageStartDelay = GetMissileConfig()
		? static_cast<float>(FMath::Max(GetMissileConfig()->DamageStartDelay, 0.0))
		: 0.0f;

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
	if (K2_HasAuthority())
	{
		RefreshMissileTargets();
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

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(MissileDamage),
		false,
		AvatarActor);
	const float DamageRadius = CalculateDamageRadius();
	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(
		DamageRadius);
	TSet<FObjectKey> DamageTargetKeys;
	TArray<TWeakObjectPtr<AActor>> DamageTargets;

	for (const TWeakObjectPtr<AActor>& TargetPtr : ActiveMissileTargetActors)
	{
		AActor* TargetActor = TargetPtr.Get();
		FVector TargetLocation = FVector::ZeroVector;
		if (!IsEligibleMissileTargetActor(TargetActor)
			|| !ResolveTargetAimLocation(TargetActor, TargetLocation)
			|| !IsMissileTargetLocationWithinRange(TargetLocation))
		{
			continue;
		}

		const FObjectKey TargetKey(TargetActor);
		if (!DamageTargetKeys.Contains(TargetKey))
		{
			DamageTargetKeys.Add(TargetKey);
			DamageTargets.Add(TargetActor);
		}

		if (DamageRadius <= 0.0f)
		{
			continue;
		}

		TArray<FOverlapResult> OverlapResults;
		World->OverlapMultiByObjectType(
			OverlapResults,
			TargetLocation,
			FQuat::Identity,
			ObjectQueryParams,
			SphereShape,
			QueryParams);
		for (const FOverlapResult& OverlapResult : OverlapResults)
		{
			AActor* HitActor = OverlapResult.GetActor();
			if (!IsEligibleMissileTargetActor(HitActor))
			{
				continue;
			}

			const FObjectKey HitActorKey(HitActor);
			if (!DamageTargetKeys.Contains(HitActorKey))
			{
				DamageTargetKeys.Add(HitActorKey);
				DamageTargets.Add(HitActor);
			}
		}
	}

	for (const TWeakObjectPtr<AActor>& DamageTarget : DamageTargets)
	{
		if (DamageTarget.IsValid())
		{
			ApplyEffectToHitActor(DamageTarget.Get(), TickDamageMagnitude);
		}
	}

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
	if (AppliedHandle.WasSuccessfullyApplied())
	{
		ApplyConfiguredStatusEffectToTarget(
			GetSourceSkillDataAsset(),
			TargetASC);
	}
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

bool UMissileAbility::TryGetAttackTargetGroundLocation(
	FVector& OutGroundLocation) const
{
	AActor* AttackTarget = GetAttackTargetFromAvatar();
	if (!IsEligibleMissileTargetActor(AttackTarget))
	{
		return false;
	}

	if (const AActor* AvatarActor = GetAvatarActorFromActorInfo();
		AvatarActor && AvatarActor->HasAuthority())
	{
		if (!TryValidateServerMissileActorTarget(
			AttackTarget,
			OutGroundLocation))
		{
			return false;
		}

		return true;
	}

	if (!ResolveTargetAimLocation(AttackTarget, OutGroundLocation))
	{
		return false;
	}

	if (!IsMissileTargetLocationWithinRange(OutGroundLocation))
	{
		return false;
	}

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

		if (!IsEligibleMissileTargetActor(CandidateActor))
		{
			continue;
		}

		FVector CandidateAimLocation = FVector::ZeroVector;
		if (!ResolveTargetAimLocation(CandidateActor, CandidateAimLocation)
			|| !IsMissileTargetLocationWithinRange(CandidateAimLocation))
		{
			continue;
		}

		if (AvatarActor->HasAuthority()
			&& !TryValidateServerMissileActorTarget(
				CandidateActor,
				CandidateAimLocation))
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

bool UMissileAbility::TryGetAutoTargetGroundLocation(
	FVector& OutGroundLocation) const
{
	if (TryGetAttackTargetGroundLocation(OutGroundLocation))
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

	return ResolveTargetAimLocation(ClosestEnemy, OutGroundLocation);
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

bool UMissileAbility::IsEligibleMissileTargetActor(const AActor* TargetActor) const
{
	const ACharacterBase* SourceCharacter =
		Cast<ACharacterBase>(GetAvatarActorFromActorInfo());
	const ACharacterBase* TargetCharacter =
		Cast<ACharacterBase>(TargetActor);
	if (!IsValid(SourceCharacter)
		|| !IsValid(TargetCharacter)
		|| !SourceCharacter->CanDamageCharacterByTeam(TargetCharacter))
	{
		return false;
	}

	const UAbilitySystemComponent* TargetASC =
		TargetCharacter->GetAbilitySystemComponent();
	return TargetASC
		&& !TargetASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead)
		&& TargetASC->GetNumericAttribute(
			UBasicAttributeSet::GetHealthAttribute()) > 0.0f;
}

bool UMissileAbility::TryValidateServerMissileActorTarget(
	AActor* TargetActor,
	FVector& OutTargetLocation) const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	if (!AvatarActor
		|| !AvatarActor->HasAuthority()
		|| !World
		|| !MissileConfig
		|| !IsEligibleMissileTargetActor(TargetActor))
	{
		return false;
	}

	FVector AuthorityTargetLocation = FVector::ZeroVector;
	if (!ResolveTargetAimLocation(TargetActor, AuthorityTargetLocation)
		|| !IsFiniteMissileLocation(AuthorityTargetLocation))
	{
		return false;
	}

	PdTargetValidator::FPointTargetValidationParams ValidationParams;
	ValidationParams.MaxRange = MissileConfig->TargetingMaxRange;
	ValidationParams.LineOfSightProfileName =
		MissileConfig->TargetingTraceProfileName;

	PdTargetValidator::FValidatedPointTarget ValidatedTarget;
	if (!PdTargetValidator::ValidatePointTarget(
		World,
		AvatarActor,
		GetMissileTargetingOrigin(),
		GetMissileTraceStartLocation(),
		AuthorityTargetLocation,
		ValidationParams,
		ValidatedTarget))
	{
		return false;
	}

	ACharacterBase* TraceCharacter =
		ResolveMissileTraceCharacter(ValidatedTarget.BlockingActor.Get());
	if (TraceCharacter != TargetActor)
	{
		return false;
	}

	OutTargetLocation = AuthorityTargetLocation;
	return true;
}

bool UMissileAbility::TryValidateServerMissileTargetData(
	const FGameplayAbilityTargetDataHandle& Data,
	FVector& OutTargetLocation,
	AActor*& OutTargetActor) const
{
	OutTargetLocation = FVector::ZeroVector;
	OutTargetActor = nullptr;

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	const FGameplayAbilityTargetData* TargetData = Data.Get(0);
	const FHitResult* ClientHitResult =
		TargetData ? TargetData->GetHitResult() : nullptr;
	if (!AvatarActor
		|| !AvatarActor->HasAuthority()
		|| !World
		|| !MissileConfig
		|| !TargetData)
	{
		return false;
	}

	if (AActor* ClientActorHint = ResolveTargetDataActor(Data))
	{
		if (!TryValidateServerMissileActorTarget(
			ClientActorHint,
			OutTargetLocation))
		{
			return false;
		}

		OutTargetActor = ClientActorHint;
		return true;
	}

	FVector RequestedLocation = FVector::ZeroVector;
	const FVector TargetDataEndPoint =
		UAbilitySystemBlueprintLibrary::GetTargetDataEndPoint(Data, 0);
	if (ClientHitResult)
	{
		if (!PdTargetValidator::TryResolveTargetDataLocation(
			*ClientHitResult,
			TargetDataEndPoint,
			RequestedLocation))
		{
			return false;
		}
	}
	else if (!IsFiniteMissileLocation(TargetDataEndPoint))
	{
		return false;
	}
	else
	{
		RequestedLocation = TargetDataEndPoint;
	}

	PdTargetValidator::FPointTargetValidationParams ValidationParams;
	ValidationParams.MaxRange = MissileConfig->TargetingMaxRange;
	ValidationParams.LineOfSightProfileName =
		MissileConfig->TargetingTraceProfileName;

	PdTargetValidator::FValidatedPointTarget ValidatedTarget;
	if (!PdTargetValidator::ValidatePointTarget(
		World,
		AvatarActor,
		GetMissileTargetingOrigin(),
		GetMissileTraceStartLocation(),
		RequestedLocation,
		ValidationParams,
		ValidatedTarget))
	{
		return false;
	}

	AActor* BlockingActor = ValidatedTarget.BlockingActor.Get();
	if (ACharacterBase* TraceCharacter =
		ResolveMissileTraceCharacter(BlockingActor))
	{
		if (!IsEligibleMissileTargetActor(TraceCharacter))
		{
			return false;
		}

		FVector AuthorityTargetLocation = FVector::ZeroVector;
		if (!ResolveTargetAimLocation(
			TraceCharacter,
			AuthorityTargetLocation)
			|| !IsMissileTargetLocationWithinRange(
				AuthorityTargetLocation))
		{
			return false;
		}

		OutTargetLocation = AuthorityTargetLocation;
		OutTargetActor = TraceCharacter;
		return true;
	}

	if (IsValid(BlockingActor) && BlockingActor->IsA<APawn>())
	{
		return false;
	}

	OutTargetLocation = ValidatedTarget.Location;
	return IsFiniteMissileLocation(OutTargetLocation);
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
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	return AvatarActor ? AvatarActor->GetActorLocation() : FVector::ZeroVector;
}

FVector UMissileAbility::GetMissileTraceStartLocation() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	if (!AvatarActor)
	{
		return FVector::ZeroVector;
	}

	const FName SourceSocketName = MissileConfig
		? MissileConfig->NiagaraSpawnSocketName
		: NAME_None;
	if (!SourceSocketName.IsNone())
	{
		if (const ACharacterBase* SourceCharacter =
			Cast<ACharacterBase>(AvatarActor);
			SourceCharacter
			&& SourceCharacter->GetMesh()
			&& SourceCharacter->GetMesh()->DoesSocketExist(
				SourceSocketName))
		{
			return SourceCharacter->GetMesh()->GetSocketLocation(
				SourceSocketName);
		}

		if (const USkeletalMeshComponent* SourceMesh =
			Cast<USkeletalMeshComponent>(
				AvatarActor->GetComponentByClass(
					USkeletalMeshComponent::StaticClass()));
			SourceMesh
			&& SourceMesh->DoesSocketExist(SourceSocketName))
		{
			return SourceMesh->GetSocketLocation(SourceSocketName);
		}
	}

	return AvatarActor->GetActorLocation();
}

AActor* UMissileAbility::ResolveTargetDataActor(const FGameplayAbilityTargetDataHandle& Data) const
{
	const FGameplayAbilityTargetData* TargetData = Data.Get(0);
	if (!TargetData)
	{
		return nullptr;
	}

	if (const FHitResult* HitResult = TargetData->GetHitResult())
	{
		AActor* HitActor = HitResult->GetActor();
		if (IsValid(HitActor) && HitActor->IsA<APawn>())
		{
			return HitActor;
		}
	}

	for (const TWeakObjectPtr<AActor>& TargetActor : TargetData->GetActors())
	{
		AActor* Actor = TargetActor.Get();
		if (IsValid(Actor) && Actor->IsA<APawn>())
		{
			return Actor;
		}
	}

	return nullptr;
}

FVector UMissileAbility::ResolveTargetDataLocation(const FGameplayAbilityTargetDataHandle& Data) const
{
	AActor* TargetActor = ResolveTargetDataActor(Data);
	if (IsValid(TargetActor))
	{
		FVector TargetAimLocation = FVector::ZeroVector;
		if (ResolveTargetAimLocation(TargetActor, TargetAimLocation))
		{
			return TargetAimLocation;
		}
	}

	const FGameplayAbilityTargetData* TargetData = Data.Get(0);
	const FHitResult* HitResult =
		TargetData ? TargetData->GetHitResult() : nullptr;
	if (HitResult)
	{
		FVector ResolvedLocation = FVector::ZeroVector;
		if (PdTargetValidator::TryResolveTargetDataLocation(
			*HitResult,
			UAbilitySystemBlueprintLibrary::GetTargetDataEndPoint(
				Data,
				0),
			ResolvedLocation))
		{
			return ResolvedLocation;
		}
	}

	const FVector TargetDataEndPoint =
		UAbilitySystemBlueprintLibrary::GetTargetDataEndPoint(Data, 0);
	return IsFiniteMissileLocation(TargetDataEndPoint)
		? TargetDataEndPoint
		: FVector::ZeroVector;
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
	return MissileConfig
		? static_cast<float>(FMath::Max(MissileConfig->DamageRadius, 0.0))
		: 0.0f;
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

void UMissileAbility::DrawDebugTargetingRange() const
{
	const FMissileSkillConfig* MissileConfig = GetMissileConfig();
	if (!LabSkillDebug::IsDrawingEnabled()
		|| !MissileConfig
		|| !MissileConfig->bDebugTargeting
		|| MissileConfig->TargetingMaxRange <= 0.0)
	{
		return;
	}

	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	DrawDebugSphere(
		World,
		GetMissileTargetingOrigin(),
		static_cast<float>(MissileConfig->TargetingMaxRange),
		64,
		FColor::Cyan,
		false,
		MissileTargetTrackingInterval * 1.5f,
		0,
		2.0f);
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

	if (bMissileLaunched)
	{
		return;
	}

	if (!CanExecuteSkillPayload())
	{
		FinishMissileAbility(true);
		return;
	}

	FVector TargetLocation = FVector::ZeroVector;
	AActor* TargetActor = nullptr;
	bool bResolvedTargetData = false;
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
	{
		bResolvedTargetData = TryValidateServerMissileTargetData(
			Data,
			TargetLocation,
			TargetActor);
	}
	else
	{
		TargetActor = ResolveTargetDataActor(Data);
		TargetLocation = ResolveTargetDataLocation(Data);
		bResolvedTargetData = IsFiniteMissileLocation(TargetLocation)
			&& !TargetLocation.IsNearlyZero()
			&& (!TargetActor
				|| IsEligibleMissileTargetActor(TargetActor));
	}

	if (!bResolvedTargetData)
	{
		LaunchMissileFromResolvedTarget();
		return;
	}

	bMissileLaunched = true;
	ConfirmMissileAtLocation(TargetLocation);
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
