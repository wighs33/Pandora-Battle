#include "AbilitySystem/Skill/Actions/SkillMissileAction.h"

#include "AbilitySystem/Ability/SkillAbility.h"
#include "Component/AbilitySystem/Ability/AbilityPresentationManager.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "UObject/ObjectKey.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillMissileAction)

namespace
{
	constexpr float MissileTargetTrackingInterval = 0.05f;
}


void USkillMissileAction::OnStart()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetAbility()->GetCurrentActorInfo();

	MissileMontageTask = nullptr;
	WaitMissileMontageTriggerTask = nullptr;
	ActiveMissileTargetActors.Reset();
	DamageTicksApplied = 0;
	PlannedDamageTickCount = 0;
	bMissileLaunched = false;
	bMissileDurationFinished = true;
	bDamageSequenceFinished = true;

	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !SkillDataAsset)
	{
		Finish(false);
		return;
	}

	StartWaitMissileMontageTriggerTask();

	if (!GetResolvedMissileMontage())
	{
		TryLaunchMissile();
		return;
	}

	if (!StartMissileMontageTask())
	{
		TryLaunchMissile();
	}
}

void USkillMissileAction::OnStop()
{
	GetAbility()->RestoreAvatarMovementForAbility();

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

void USkillMissileAction::StartWaitMissileMontageTriggerTask()
{
	const FGameplayTag TriggerTag = GetResolvedMontageTriggerEventTag();
	if (!TriggerTag.IsValid())
	{
		return;
	}

	WaitMissileMontageTriggerTask = GetAbility()->CreateWaitGameplayEventTask(TriggerTag);
	if (!WaitMissileMontageTriggerTask)
	{
		return;
	}

	WaitMissileMontageTriggerTask->EventReceived.AddDynamic(this, &ThisClass::HandleMissileMontageTriggerEvent);
	WaitMissileMontageTriggerTask->ReadyForActivation();
}

bool USkillMissileAction::StartMissileMontageTask()
{
	UAnimMontage* ResolvedMontage = GetResolvedMissileMontage();
	if (!ResolvedMontage)
	{
		return false;
	}

	MissileMontageTask = GetAbility()->CreateDefaultMontageAndWaitTask(ResolvedMontage);
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

UAnimMontage* USkillMissileAction::GetResolvedMissileMontage() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->Animation.PrimaryMontage.Get() : nullptr;
}

FGameplayTag USkillMissileAction::GetResolvedMontageTriggerEventTag() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	return SkillDataAsset && SkillDataAsset->Animation.PrimaryEventTag.IsValid()
		       ? SkillDataAsset->Animation.PrimaryEventTag
		       : LabGameplayTags::Event_Montage_Trigger;
}

void USkillMissileAction::TryLaunchMissile()
{
	if (bMissileLaunched || !(IsRunning() && GetAbility()->CanRunActions()))
	{
		return;
	}

	bMissileLaunched = true;
	LaunchMissile();
}

void USkillMissileAction::LaunchMissile()
{
	if (!GetAbility()->CommitSkill())
	{
		FinishMissile(true);
		return;
	}

	GetAbility()->GetPresentationManager().SpawnConfiguredCharacterDecal(*GetAbility());
	GetAbility()->StartDurationMovementLock();
	GetAbility()->GetPresentationManager().StartConfiguredGroundFX(*GetAbility());
	StartMissilePresentation();
	StartMissileTargetTracking();
	StartMissileDurationTimer();

	StartDamageSequence();
}

void USkillMissileAction::StartMissilePresentation()
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (!SkillDataAsset || !SkillDataAsset->Niagara.SocketNiagaraSystem)
	{
		return;
	}

	GetAbility()->GetPresentationManager().StartConfiguredMissilePresentation(*GetAbility());
	GetAbility()->GetPresentationManager().SetMissileTargeting(Settings.AimPositionParameterName,
	                                                           Settings.TargetSocketName);
}

void USkillMissileAction::StartMissileDurationTimer()
{
	// Duration 스킬은 Ability가 관리하는 전체 지속시간을 사용한다.
	if (GetAbility()->HasDurationDeadline())
	{
		bMissileDurationFinished = false;
		return;
	}

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

void USkillMissileAction::HandleMissileDurationFinished()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MissileDurationTimerHandle);
	}

	bMissileDurationFinished = true;

	StopMissileTargetTracking();
	ActiveMissileTargetActors.Reset();
	GetAbility()->GetPresentationManager().StopConfiguredMissilePresentation(*GetAbility());

	TryFinishAfterWork();
}

void USkillMissileAction::StartMissileTargetTracking()
{
	RefreshMissileTargets();
	const bool bNeedsLocalDebugRefresh = LabSkillDebug::IsDrawingEnabled() && Settings.bDebugTargeting;
	if (!GetAbility()->K2_HasAuthority() && !bNeedsLocalDebugRefresh)
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

void USkillMissileAction::StopMissileTargetTracking()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MissileTargetTrackingTimerHandle);
	}
}

void USkillMissileAction::HandleMissileTargetTrackingTick()
{
	RefreshMissileTargets();
}

void USkillMissileAction::RefreshMissileTargets()
{
	DrawDebugTargetingRange();

	if (!GetAbility()->K2_HasAuthority())
	{
		return;
	}

	AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!AvatarActor || !World || Settings.TargetingMaxRange <= 0.0)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(MissileActiveTargetSearch),
		false,
		AvatarActor);
	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(static_cast<float>(Settings.TargetingMaxRange));

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

	GetAbility()->GetPresentationManager().UpdateConfiguredMissilePresentationTargets(PresentationTargets);
}

void USkillMissileAction::StartDamageSequence()
{
	PlannedDamageTickCount = CalculateDamageTickCount();
	DamageTicksApplied = 0;
	bDamageSequenceFinished = false;

	const float DamageStartDelay = static_cast<float>(FMath::Max(Settings.DamageStartDelay, 0.0));

	UWorld* World = GetWorld();
	if (!World)
	{
		FinishMissile(true);
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

void USkillMissileAction::HandleDamageDelayFinished()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageDelayTimerHandle);
	}

	HandleDamageTick();
}

void USkillMissileAction::HandleDamageTick()
{
	if (PlannedDamageTickCount <= 0)
	{
		MarkDamageSequenceFinished();
		return;
	}

	++DamageTicksApplied;
	const float TickDamageMagnitude = CalculateDamageMagnitudePerTick();
	if (GetAbility()->K2_HasAuthority())
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
		FinishMissile(true);
	}
}

void USkillMissileAction::ApplyMissileDamageTick(const float TickDamageMagnitude)
{
	AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
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

void USkillMissileAction::ApplyEffectToHitActor(AActor* HitActor, const float TickDamageMagnitude)
{
	if (!IsValid(HitActor))
	{
		return;
	}

	const ACharacterBase* SourceCharacter = Cast<ACharacterBase>(GetAbility()->GetAvatarActorFromActorInfo());
	const ACharacterBase* TargetCharacter = Cast<ACharacterBase>(HitActor);
	if (SourceCharacter && TargetCharacter && !SourceCharacter->CanDamageCharacterByTeam(TargetCharacter))
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(
		GetAbility()->GetAvatarActorFromActorInfo());
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

	const FActiveGameplayEffectHandle AppliedHandle = SourceASC->ApplyGameplayEffectSpecToTarget(
		*DamageSpecHandle.Data.Get(), TargetASC);
	if (AppliedHandle.WasSuccessfullyApplied())
	{
		GetAbility()->ApplyConfiguredStatusEffectToTarget(
			GetAbility()->GetSourceSkillDataAsset(),
			TargetASC);
	}
}

bool USkillMissileAction::ResolveTargetAimLocation(AActor* TargetActor, FVector& OutAimLocation) const
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	const FName TargetSocketName = Settings.TargetSocketName;
	if (!TargetSocketName.IsNone())
	{
		if (const ACharacterBase* TargetCharacter = Cast<ACharacterBase>(TargetActor);
			TargetCharacter && TargetCharacter->GetMesh() && TargetCharacter->GetMesh()->DoesSocketExist(
				TargetSocketName))
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

bool USkillMissileAction::IsEligibleMissileTargetActor(const AActor* TargetActor) const
{
	const ACharacterBase* SourceCharacter =
		Cast<ACharacterBase>(GetAbility()->GetAvatarActorFromActorInfo());
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

bool USkillMissileAction::IsMissileTargetLocationWithinRange(const FVector& TargetLocation) const
{
	if (Settings.TargetingMaxRange <= 0.0 || TargetLocation.ContainsNaN())
	{
		return false;
	}

	const double MaxRange = Settings.TargetingMaxRange;
	return FVector::DistSquared(GetMissileTargetingOrigin(), TargetLocation) <= FMath::Square(MaxRange);
}

FVector USkillMissileAction::GetMissileTargetingOrigin() const
{
	const AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
	return AvatarActor ? AvatarActor->GetActorLocation() : FVector::ZeroVector;
}

FGameplayEffectSpecHandle USkillMissileAction::MakeDamageEffectSpec(const float DamageMagnitude) const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	const FSkillGameplayEffectConfig DamageConfig = SkillDataAsset
		                                                ? SkillDataAsset->GetResolvedDamageConfig()
		                                                : FSkillGameplayEffectConfig();
	if (!DamageConfig.GameplayEffectClass)
	{
		return FGameplayEffectSpecHandle();
	}

	return GetAbility()->MakeConfiguredDamageEffectSpec(DamageConfig, DamageMagnitude);
}


float USkillMissileAction::CalculateMissileDuration() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	return SkillDataAsset ? static_cast<float>(FMath::Max(SkillDataAsset->Time.Duration, 0.0)) : 0.0f;
}

float USkillMissileAction::CalculateDamageRadius() const
{
	return static_cast<float>(FMath::Max(Settings.DamageRadius, 0.0));
}

float USkillMissileAction::CalculateDamageMagnitudePerTick() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	return SkillDataAsset
		       ? GetAbility()->CalculateDamageMagnitude(SkillDataAsset->GetResolvedDamageConfig())
		       : 0.0f;
}

int32 USkillMissileAction::CalculateDamageTickCount() const
{
	const float Duration = CalculateDamageApplicationDuration();
	const float Interval = CalculateDamageInterval();
	return FMath::Max(FMath::CeilToInt(Duration / Interval), 1);
}

float USkillMissileAction::CalculateDamageApplicationDuration() const
{
	return static_cast<float>(FMath::Max(Settings.DamageApplicationDuration, 0.0));
}

float USkillMissileAction::CalculateDamageInterval() const
{
	return static_cast<float>(FMath::Max(Settings.DamageInterval, 0.05));
}

void USkillMissileAction::DrawDebugTargetingRange() const
{
	if (!LabSkillDebug::IsDrawingEnabled() || !Settings.bDebugTargeting || Settings.TargetingMaxRange <= 0.0)
	{
		return;
	}

	const AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	DrawDebugSphere(
		World,
		GetMissileTargetingOrigin(),
		static_cast<float>(Settings.TargetingMaxRange),
		64,
		FColor::Cyan,
		false,
		MissileTargetTrackingInterval * 1.5f,
		0,
		2.0f);
}

void USkillMissileAction::MarkDamageSequenceFinished()
{
	bDamageSequenceFinished = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageDelayTimerHandle);
		World->GetTimerManager().ClearTimer(DamageTickTimerHandle);
	}

	TryFinishAfterWork();
}

void USkillMissileAction::TryFinishAfterWork()
{
	if (!bDamageSequenceFinished || !bMissileDurationFinished)
	{
		return;
	}

	FinishMissile(false);
}

void USkillMissileAction::FinishMissile(const bool bWasCancelled)
{
	if (bWasCancelled)
	{
		Finish(false);
		return;
	}

	Finish();
}

void USkillMissileAction::HandleMissileMontageTriggerEvent(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	TryLaunchMissile();
}

void USkillMissileAction::HandleMissileMontageFinished()
{
	MissileMontageTask = nullptr;

	if (!bMissileLaunched)
	{
		TryLaunchMissile();
	}
}

void USkillMissileAction::HandleMissileMontageInterrupted()
{
	MissileMontageTask = nullptr;

	if (bMissileLaunched)
	{
		return;
	}

	TryLaunchMissile();
}
