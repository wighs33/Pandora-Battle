#include "AbilitySystem/Ability/ProjectileAbility.h"

#include "Abilities/GameplayAbilityTargetActor_SingleLineTrace.h"
#include "Abilities/GameplayAbilityTargetActor_GroundTrace.h"
#include "Abilities/GameplayAbilityTargetActor_Trace.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitConfirmCancel.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Definition/AbilitySystem/StatusEffectDefinition.h"
#include "AbilitySystem/Projectiles/ProjectileBase.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "AbilitySystem/TargetValidator.h"
#include "AbilitySystem/TargetingActors/TargetActor_GroundTrace_Decal.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Character/CharacterBase.h"
#include "Character/PdPlayer.h"
#include "Common/LabGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProjectileAbility)

DEFINE_LOG_CATEGORY_STATIC(LogProjectileAbility, Log, All);

UProjectileAbility::UProjectileAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(LabGameplayTags::GameplayAbility_ShootProjectile);
	SetAssetTags(AssetTags);

	ActivationOwnedTags.AddTag(LabGameplayTags::GameplayAbility_ShootProjectile_Active);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::Action_Attack);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::Action_Punch);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::Action_RangedAttack);
}

void UProjectileAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	static_cast<void>(TriggerEventData);

	bEndAfterProjectileFired = false;
	bPausedForPlayerAim = false;
	bPlayerProjectileConfirmed = false;
	bProjectileExecutionRequested = false;
	bProjectileSpawnSucceeded = false;
	ReadiedProjectile = nullptr;
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (SkillDataAsset->SkillDataType != ESkillDataType::Projectile)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const TSubclassOf<AProjectileBase> ConfiguredProjectileClass = GetConfiguredProjectileClass();
	const float ConfiguredProjectileSpeed = GetConfiguredProjectileSpeed();
	if (!ConfiguredProjectileClass || ConfiguredProjectileSpeed <= 0.0f)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	BeginConfirmedShot();
}

void UProjectileAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	CleanupAimingState();
	ClearSocketBarrageState(true);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UProjectileAbility::HandleMontageFinished()
{
	if (!bProjectileExecutionRequested)
	{
		if (HasPlayerController() && !bPlayerProjectileConfirmed)
		{
			if (!bWaitingForPlayerConfirm)
			{
				StartPlayerAiming();
			}
			return;
		}
		ExecuteFallbackProjectileShot();
		return;
	}

	if (IsSocketBarrageActive() || ShouldWaitForServerSocketBarrageEnd())
	{
		bSocketBarrageEndAbilityAfterFire = true;
		return;
	}
	EndProjectileAbilityAfterResolvedShot();
}

void UProjectileAbility::HandleShootProjectileEvent(FGameplayEventData Payload)
{

	if (!HasPlayerController())
	{
		if (AActor* AttackTarget = GetAttackTargetFromAvatar(); IsValid(AttackTarget))
		{
			const FVector TargetLocation = AttackTarget->GetActorLocation();
			if (ExecuteProjectileShot(TargetLocation)
				&& bEndAfterProjectileFired
				&& !IsSocketBarrageActive()
				&& !ShouldWaitForServerSocketBarrageEnd())
			{
				EndProjectileAbilityAfterResolvedShot();
			}
			return;
		}
		ExecuteFallbackProjectileShot();
		return;
	}

	if (bPlayerProjectileConfirmed)
	{
		return;
	}

	if (!bPausedForPlayerAim)
	{
		bPausedForPlayerAim = true;
		PauseProjectileMontageForAiming();
		StartPlayerAiming();
	}
	return;
}

void UProjectileAbility::HandleConfirmPressed()
{
	if (!bWaitingForPlayerConfirm)
	{
		return;
	}
	ConfirmPlayerShot();
}

void UProjectileAbility::HandleCancelPressed()
{
	if (!CanBeCanceled())
	{
		SetCanBeCanceled(true);
	}
	K2_CancelAbility();
}

void UProjectileAbility::StartPlayerAiming()
{
	bWaitingForPlayerConfirm = true;
	if (const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
		SkillDataAsset && SkillDataAsset->Movement.bLockMovementDuringDuration)
	{
		LockAvatarMovementForAbility();
	}
	SpawnReadiedProjectile();

	if (ConfirmCancelTask)
	{
		ConfirmCancelTask->EndTask();
		ConfirmCancelTask = nullptr;
	}

	if (ShouldUseGroundTargeting())
	{
		WaitForPlayerTargetData();
		return;
	}

	ConfirmCancelTask = UAbilityTask_WaitConfirmCancel::WaitConfirmCancel(this);
	if (!ConfirmCancelTask)
	{
		CancelAbilityForSkillExecutionFailure();
		return;
	}

	ConfirmCancelTask->OnConfirm.AddDynamic(this, &ThisClass::HandleConfirmPressed);
	ConfirmCancelTask->OnCancel.AddDynamic(this, &ThisClass::HandleCancelPressed);
	ConfirmCancelTask->ReadyForActivation();
}
void UProjectileAbility::BeginConfirmedShot()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo)
	{
		K2_CancelAbility();
		return;
	}

	const FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();
	const FGameplayAbilityActivationInfo ActivationInfo = GetCurrentActivationInfo();

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	LockAvatarMovementForAbility();
	SpawnConfiguredCharacterDecal();

	if (IsConfiguredImmediateFireMode())
	{
		bEndAfterProjectileFired = true;
		const FVector TargetLocation = ResolveDefaultTargetLocation();

		if (ExecuteProjectileShot(TargetLocation)
			&& !IsSocketBarrageActive()
			&& !ShouldWaitForServerSocketBarrageEnd())
		{
			EndProjectileAbilityAfterResolvedShot();
		}
		return;
	}

	StartShootProjectileEventTask();

	UAnimMontage* MontageToPlay = GetConfiguredShootMontage();
	if (!MontageToPlay)
	{
		if (HasPlayerController())
		{
			StartPlayerAiming();
		}
		else
		{
			bEndAfterProjectileFired = true;
			HandleShootProjectileEvent(FGameplayEventData());
		}
		return;
	}

	ShootMontageTask = CreateDefaultMontageAndWaitTask(MontageToPlay);
	if (!ShootMontageTask)
	{
		if (HasPlayerController())
		{
			StartPlayerAiming();
		}
		else
		{
			ExecuteFallbackProjectileShot();
		}
		return;
	}

	ShootMontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	ShootMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	ShootMontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageFinished);
	ShootMontageTask->ReadyForActivation();
}

void UProjectileAbility::ConfirmPlayerShot()
{
	bWaitingForPlayerConfirm = false;
	bPlayerProjectileConfirmed = true;
	RestoreAvatarMovementForAbility();

	if (ConfirmCancelTask)
	{
		ConfirmCancelTask->EndTask();
		ConfirmCancelTask = nullptr;
	}

	if (!bPausedForPlayerAim)
	{
		bEndAfterProjectileFired = true;
	}

	if (ShouldUseGroundTargeting())
	{
		WaitForPlayerTargetData();
	}
	else if (GetConfiguredTargetTraceProfile().Name == TEXT("NoCollision"))
	{
		if (ExecuteProjectileShot(ResolveDefaultTargetLocation())
			&& bEndAfterProjectileFired
			&& !IsSocketBarrageActive()
			&& !ShouldWaitForServerSocketBarrageEnd())
		{
			EndProjectileAbilityAfterResolvedShot();
		}
	}
	else
	{
		WaitForPlayerTargetData();
	}

	if (bPausedForPlayerAim)
	{
		ResumeProjectileMontageAfterAiming();
		bPausedForPlayerAim = false;
	}
}

bool UProjectileAbility::ExecuteProjectileShot(FVector TargetLocation)
{
	if (bProjectileExecutionRequested)
	{
		return true;
	}

	bProjectileExecutionRequested = true;
	ShootProjectile(TargetLocation);

	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor || !AvatarActor->HasAuthority() || bProjectileSpawnSucceeded)
	{
		return true;
	}

	UE_LOG(
		LogProjectileAbility,
		Error,
		TEXT("Projectile skill %s committed but failed to spawn its authoritative projectile."),
		*GetNameSafe(GetSourceSkillDataAsset()));
	CancelAbilityForSkillExecutionFailure();
	return false;
}

void UProjectileAbility::ExecuteFallbackProjectileShot()
{
	if (!CanExecuteSkillPayload())
	{
		return;
	}
	if (HasPlayerController() && !bPlayerProjectileConfirmed)
	{
		return;
	}

	bWaitingForPlayerConfirm = false;
	bPlayerProjectileConfirmed = true;
	bEndAfterProjectileFired = true;
	RestoreAvatarMovementForAbility();

	if (!ExecuteProjectileShot(ResolveDefaultTargetLocation()))
	{
		return;
	}

	if (!IsSocketBarrageActive() && !ShouldWaitForServerSocketBarrageEnd())
	{
		EndProjectileAbilityAfterResolvedShot();
	}
}

void UProjectileAbility::HandleTargetDataValid(const FGameplayAbilityTargetDataHandle& Data)
{
	const bool bUsingGroundTargeting = ShouldUseGroundTargeting();
	if (bUsingGroundTargeting && bWaitingForPlayerConfirm)
	{
		// Receiving valid UserConfirmed target data is the explicit fire input.
		// Mark it before validation so a broken trace can use the post-confirm
		// fallback without ever turning skill activation itself into a shot.
		bWaitingForPlayerConfirm = false;
		bPlayerProjectileConfirmed = true;
		RestoreAvatarMovementForAbility();
		if (!bPausedForPlayerAim)
		{
			bEndAfterProjectileFired = true;
		}
	}

	const FGameplayAbilityTargetData* TargetData = Data.Get(0);
	const FHitResult* ClientHitResult = TargetData ? TargetData->GetHitResult() : nullptr;
	if (!CurrentActorInfo || !ClientHitResult)
	{
		ExecuteFallbackProjectileShot();
		return;
	}

	const FVector TargetDataEndPoint = UAbilitySystemBlueprintLibrary::GetTargetDataEndPoint(Data, 0);
	FVector TargetLocation = FVector::ZeroVector;
	if (CurrentActorInfo->IsNetAuthority())
	{
		if (!TryValidateServerProjectileTargetLocation(
			*ClientHitResult,
			TargetDataEndPoint,
			bUsingGroundTargeting,
			TargetLocation))
		{
			ExecuteFallbackProjectileShot();
			return;
		}
	}
	else if (!PdTargetValidator::TryResolveTargetDataLocation(
		*ClientHitResult,
		TargetDataEndPoint,
		TargetLocation))
	{
		ExecuteFallbackProjectileShot();
		return;
	}
	else if (bUsingGroundTargeting)
	{
		TargetLocation.Z += GetConfiguredProjectileRadius();
	}

	if (!CurrentActorInfo->IsNetAuthority() && TargetLocation.IsNearlyZero())
	{
		TryResolveProjectileAimTargetLocation(TargetLocation);
	}
	else if (!CurrentActorInfo->IsNetAuthority()
		&& !bUsingGroundTargeting
		&& ShouldRetargetUsingAim(TargetLocation))
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

	if (bShotExecuted
		&& bEndAfterProjectileFired
		&& !IsSocketBarrageActive()
		&& !ShouldWaitForServerSocketBarrageEnd())
	{
		EndProjectileAbilityAfterResolvedShot();
	}
}

bool UProjectileAbility::TryValidateServerProjectileTargetLocation(
	const FHitResult& ClientHitResult,
	const FVector& TargetDataEndPoint,
	const bool bUsingGroundTargeting,
	FVector& OutValidatedLocation) const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
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
		ValidationParams.LineOfSightProfileName = GetConfiguredGroundTargetingTraceProfile().Name;

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
	ValidationParams.LineOfSightProfileName = GetConfiguredTargetTraceProfile().Name;

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

void UProjectileAbility::HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data)
{
	static_cast<void>(Data);

	if (bCleaningUpTargetDataTask)
	{
		return;
	}

	if (HasPlayerController())
	{
		if (!bPlayerProjectileConfirmed)
		{
			CancelAbilityForSkillExecutionFailure();
			return;
		}
		if (ShouldWaitForServerSocketBarrageEnd())
		{
			return;
		}
		ExecuteFallbackProjectileShot();
		return;
	}

	if (!bProjectileExecutionRequested)
	{
		ExecuteFallbackProjectileShot();
	}
	else if (bEndAfterProjectileFired && !ShouldWaitForServerSocketBarrageEnd())
	{
		EndProjectileAbilityAfterResolvedShot();
	}
}

void UProjectileAbility::StartShootProjectileEventTask()
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

	ShootProjectileEventTask = CreateWaitGameplayEventTask(ConfiguredShootEventTag);
	if (!ShootProjectileEventTask)
	{
		return;
	}

	ShootProjectileEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleShootProjectileEvent);
	ShootProjectileEventTask->ReadyForActivation();
}

void UProjectileAbility::WaitForPlayerTargetData()
{
	const bool bUsingGroundTargeting = ShouldUseGroundTargeting();
	const TSubclassOf<AGameplayAbilityTargetActor> TargetActorClass = bUsingGroundTargeting
		? GetConfiguredGroundTargetActorClass()
		: TSubclassOf<AGameplayAbilityTargetActor>(AGameplayAbilityTargetActor_SingleLineTrace::StaticClass());
	if (!TargetActorClass)
	{
		if (HasPlayerController() && !bPlayerProjectileConfirmed)
		{
			CancelAbilityForSkillExecutionFailure();
		}
		else
		{
			ExecuteFallbackProjectileShot();
		}
		return;
	}

	const FCollisionProfileName ConfiguredTargetTraceProfile = bUsingGroundTargeting
		? GetConfiguredGroundTargetingTraceProfile()
		: GetConfiguredTargetTraceProfile();

	if (TargetDataTask)
	{
		bCleaningUpTargetDataTask = true;
		TargetDataTask->EndTask();
		bCleaningUpTargetDataTask = false;
		TargetDataTask = nullptr;
	}

	UAbilityTask_WaitTargetData* const PendingTargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(
		this,
		NAME_None,
		bUsingGroundTargeting ? EGameplayTargetingConfirmation::UserConfirmed : EGameplayTargetingConfirmation::Instant,
		TargetActorClass);
	TargetDataTask = PendingTargetDataTask;
	if (!PendingTargetDataTask)
	{
		if (HasPlayerController() && !bPlayerProjectileConfirmed)
		{
			CancelAbilityForSkillExecutionFailure();
		}
		else
		{
			ExecuteFallbackProjectileShot();
		}
		return;
	}

	PendingTargetDataTask->ValidData.AddDynamic(this, &ThisClass::HandleTargetDataValid);
	PendingTargetDataTask->Cancelled.AddDynamic(this, &ThisClass::HandleTargetDataCancelled);

	if (AGameplayAbilityTargetActor* SpawnedActor =
		BeginSpawningTargetDataActor(PendingTargetDataTask, TargetActorClass))
	{
		if (AGameplayAbilityTargetActor_Trace* TraceActor = Cast<AGameplayAbilityTargetActor_Trace>(SpawnedActor))
		{
			TraceActor->MaxRange = bUsingGroundTargeting ? GetConfiguredGroundTargetingMaxRange() : GetConfiguredTargetTraceMaxRange();
			TraceActor->TraceProfile = ConfiguredTargetTraceProfile;
			TraceActor->bTraceAffectsAimPitch = bUsingGroundTargeting
				? GetConfiguredGroundTargetingTraceAffectsAimPitch()
				: GetConfiguredTraceAffectsAimPitch();
		}

		if (AGameplayAbilityTargetActor_GroundTrace* GroundTraceActor = Cast<AGameplayAbilityTargetActor_GroundTrace>(SpawnedActor))
		{
			GroundTraceActor->CollisionRadius = GetConfiguredGroundTargetingCollisionRadius();
			GroundTraceActor->CollisionHeight = GetConfiguredGroundTargetingCollisionHeight();
		}

		if (ATargetActor_GroundTrace_Decal* DecalTargetActor = Cast<ATargetActor_GroundTrace_Decal>(SpawnedActor))
		{
			DecalTargetActor->ConfigureGroundProjection(
				GetConfiguredGroundTargetingTraceStartHeight(),
				GetConfiguredGroundTargetingTraceDepth());
			DecalTargetActor->Decal = GetConfiguredGroundTargetingDecal();
			DecalTargetActor->DecalSize = GetConfiguredGroundTargetingDecalSize();
			DecalTargetActor->DecalColor = GetConfiguredGroundTargetingDecalColor();

			float DecalStartSize = 0.0f;
			float DecalTargetSize = 0.0f;
			float DecalGrowthDuration = 0.0f;
			if (TryBuildGroundTargetingDecalGrowth(DecalStartSize, DecalTargetSize, DecalGrowthDuration))
			{
				DecalTargetActor->ConfigureDecalGrowth(DecalStartSize, DecalTargetSize, DecalGrowthDuration);
			}
		}

		SpawnedActor->StartLocation = MakeTargetLocationInfoFromOwnerActor();
		SpawnedActor->bDebug = bUsingGroundTargeting ? GetConfiguredDrawGroundTargetingDebug() : GetConfiguredDrawTargetTraceDebug();
		FinishSpawningTargetDataActor(PendingTargetDataTask, SpawnedActor);
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

bool UProjectileAbility::ShouldRetargetUsingAim(const FVector& TargetLocation) const
{
	const FVector SpawnLocation = GetSpawnLocation();
	const float MinimumDistance = FMath::Max(GetConfiguredMinimumTargetDistanceFromSpawn(), 0.0f);
	const bool bTooClose = MinimumDistance > 0.0f
		&& FVector::DistSquared(SpawnLocation, TargetLocation) < FMath::Square(MinimumDistance);
	const bool bStronglyDownward = TargetLocation.Z < SpawnLocation.Z - 50.0f
		&& FVector::DistSquared2D(SpawnLocation, TargetLocation) < FMath::Square(MinimumDistance);

	return bTooClose || bStronglyDownward;
}

bool UProjectileAbility::TryResolveProjectileAimTargetLocation(FVector& OutTargetLocation) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
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
	const FCollisionProfileName ConfiguredTargetTraceProfile = GetConfiguredTargetTraceProfile();
	if (ConfiguredTargetTraceProfile.Name == TEXT("NoCollision"))
	{
		OutTargetLocation = ViewTraceEnd;

		return true;
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(const_cast<AActor*>(AvatarActor));
	ActorsToIgnore.Add(GetAvatarActorFromActorInfo());

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

FVector UProjectileAbility::ResolveDefaultTargetLocation() const
{
	FVector TargetLocation = FVector::ZeroVector;
	if (TryResolveProjectileAimTargetLocation(TargetLocation))
	{
		return TargetLocation;
	}

	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		return FVector::ForwardVector * GetConfiguredTargetTraceMaxRange();
	}

	return GetSpawnLocation() + (AvatarActor->GetActorForwardVector() * FMath::Max(GetConfiguredTargetTraceMaxRange(), 1000.0f));
}

FGameplayEffectSpecHandle UProjectileAbility::MakeDamageEffectSpec(const float ChargeDamageAlpha) const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{

		return FGameplayEffectSpecHandle();
	}

	const FSkillGameplayEffectConfig DamageConfig = SkillDataAsset->GetResolvedDamageConfig();
	if (!DamageConfig.GameplayEffectClass)
	{

		return FGameplayEffectSpecHandle();
	}

	const float ScaledDamage = CalculateBaseSkillDamageMagnitude(DamageConfig);
	const float FullDamage = ApplyIntelligenceToSkillDamage(ScaledDamage);
	const float ClampedChargeDamageAlpha = FMath::Clamp(ChargeDamageAlpha, 0.0f, 1.0f);
	const float CalculatedDamage = FullDamage * ClampedChargeDamageAlpha;
	return MakeConfiguredDamageEffectSpec(DamageConfig, CalculatedDamage);
}

FGameplayEffectSpecHandle UProjectileAbility::MakeStatusEffectSpec() const
{
	return MakeConfiguredStatusEffectSpec(
		GetSourceSkillDataAsset(),
		GetConfiguredStatusEffectClass(),
		GetConfiguredStatusEffectLevel());
}

bool UProjectileAbility::HasPlayerController() const
{
	const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	const AController* Controller = AvatarPawn ? AvatarPawn->GetController() : nullptr;
	return Controller && Controller->IsPlayerController();
}

void UProjectileAbility::PauseProjectileMontageForAiming()
{
	UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->CurrentMontageSetPlayRate(0.0f);

}

void UProjectileAbility::ResumeProjectileMontageAfterAiming()
{
	UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->CurrentMontageSetPlayRate(1.0f);

}

void UProjectileAbility::CleanupAimingState()
{
	RestoreAvatarMovementForAbility();
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

void UProjectileAbility::EndProjectileAbilityAfterResolvedShot()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (AvatarActor && !AvatarActor->HasAuthority())
	{
		K2_EndAbilityLocally();
		return;
	}
	K2_EndAbility();
}
