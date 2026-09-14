#include "AbilitySystem/Skill/Actions/SkillProjectileCastAction.h"

#include "Component/AbilitySystem/Ability/AbilityPresentationManager.h"
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
#include "Definition/AbilitySystem/SkillDefinition.h"
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

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillProjectileCastAction)

DEFINE_LOG_CATEGORY_STATIC(LogProjectileAbility, Log, All);

USkillProjectileCastAction::USkillProjectileCastAction()
{
	Settings.FireEventTag = LabGameplayTags::Event_ShootProjectile;
	Settings.ProjectileActorClass = AProjectileBase::StaticClass();
	Settings.ProjectileSpeed = 2000.0;
	Settings.ProjectileRadius = 50.0;
	Settings.SpawnLocationOffset = FVector(0.0, 0.0, 80.0);
	Settings.MinimumForwardSpawnOffset = 140.0;
	Settings.TargetTraceMaxRange = 999999.0;
	Settings.TargetDecalSize = 512.0;
	Settings.GroundTargetActorClass = ATargetActor_GroundTrace_Decal::StaticClass();
}

void USkillProjectileCastAction::OnStart()
{
	const auto Handle = GetAbility()->GetCurrentAbilitySpecHandle();
	const auto* ActorInfo = GetAbility()->GetCurrentActorInfo();
	const auto ActivationInfo = GetAbility()->GetCurrentActivationInfo();
	const auto* TriggerEventData = &GetContext().EventData;
	static_cast<void>(TriggerEventData);

	bEndAfterProjectileFired = false;
	bPausedForPlayerAim = false;
	bPlayerProjectileConfirmed = false;
	bProjectileExecutionRequested = false;
	bProjectileSpawnSucceeded = false;
	ReadiedProjectile = nullptr;
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{

		Finish(!(true));
		return;
	}

	const TSubclassOf<AProjectileBase> ConfiguredProjectileClass = GetConfiguredProjectileClass();
	const float ConfiguredProjectileSpeed = GetConfiguredProjectileSpeed();
	if (!ConfiguredProjectileClass || ConfiguredProjectileSpeed <= 0.0f)
	{

		Finish(!(true));
		return;
	}
	BeginConfirmedShot();
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
		ExecuteFallbackProjectileShot();
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

	if (!GetAbility()->HasPlayerController())
	{
		if (AActor* AttackTarget = GetAbility()->GetAttackTargetFromAvatar(); IsValid(AttackTarget))
		{
			const FVector TargetLocation = AttackTarget->GetActorLocation();
			if (ExecuteProjectileShot(TargetLocation)
				&& bEndAfterProjectileFired
				&& !IsSocketBarrageActive()
				&& !ShouldWaitForServerSocketBarrageEnd())
			{
				FinishCast();
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

	if (ShouldUseGroundTargeting())
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
void USkillProjectileCastAction::BeginConfirmedShot()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetAbility()->GetCurrentActorInfo();
	if (!ActorInfo)
	{
		Finish(false);
		return;
	}

	const FGameplayAbilitySpecHandle Handle = GetAbility()->GetCurrentAbilitySpecHandle();
	const FGameplayAbilityActivationInfo ActivationInfo = GetAbility()->GetCurrentActivationInfo();

	if (!GetAbility()->CommitSkill())
	{
		Finish(!(true));
		return;
	}

	GetAbility()->LockAvatarMovementForAbility();
	GetAbility()->GetPresentationManager().SpawnConfiguredCharacterDecal(*GetAbility());

	if (IsConfiguredImmediateFireMode())
	{
		bEndAfterProjectileFired = true;
		const FVector TargetLocation = ResolveDefaultTargetLocation();

		if (ExecuteProjectileShot(TargetLocation)
			&& !IsSocketBarrageActive()
			&& !ShouldWaitForServerSocketBarrageEnd())
		{
			FinishCast();
		}
		return;
	}

	StartShootProjectileEventTask();

	UAnimMontage* MontageToPlay = GetConfiguredShootMontage();
	if (!MontageToPlay)
	{
		if (GetAbility()->HasPlayerController())
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

	ShootMontageTask = GetAbility()->CreateDefaultMontageAndWaitTask(MontageToPlay);
	if (!ShootMontageTask)
	{
		if (GetAbility()->HasPlayerController())
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
			FinishCast();
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

void USkillProjectileCastAction::ExecuteFallbackProjectileShot()
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

	if (!ExecuteProjectileShot(ResolveDefaultTargetLocation()))
	{
		return;
	}

	if (!IsSocketBarrageActive() && !ShouldWaitForServerSocketBarrageEnd())
	{
		FinishCast();
	}
}

void USkillProjectileCastAction::HandleTargetDataValid(const FGameplayAbilityTargetDataHandle& Data)
{
	const bool bUsingGroundTargeting = ShouldUseGroundTargeting();
	if (bUsingGroundTargeting && bWaitingForPlayerConfirm)
	{
		// Receiving valid UserConfirmed target data is the explicit fire input.
		// Mark it before validation so a broken trace can use the post-confirm
		// fallback without ever turning skill activation itself into a shot.
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
	if (!GetAbility()->GetCurrentActorInfo() || !ClientHitResult)
	{
		ExecuteFallbackProjectileShot();
		return;
	}

	const FVector TargetDataEndPoint = UAbilitySystemBlueprintLibrary::GetTargetDataEndPoint(Data, 0);
	FVector TargetLocation = FVector::ZeroVector;
	if (GetAbility()->GetCurrentActorInfo()->IsNetAuthority())
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

	if (!GetAbility()->GetCurrentActorInfo()->IsNetAuthority() && TargetLocation.IsNearlyZero())
	{
		TryResolveProjectileAimTargetLocation(TargetLocation);
	}
	else if (!GetAbility()->GetCurrentActorInfo()->IsNetAuthority()
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
		FinishCast();
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
		ExecuteFallbackProjectileShot();
		return;
	}

	if (!bProjectileExecutionRequested)
	{
		ExecuteFallbackProjectileShot();
	}
	else if (bEndAfterProjectileFired && !ShouldWaitForServerSocketBarrageEnd())
	{
		FinishCast();
	}
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
	const bool bUsingGroundTargeting = ShouldUseGroundTargeting();
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
			ExecuteFallbackProjectileShot();
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
	const FCollisionProfileName ConfiguredTargetTraceProfile = GetConfiguredTargetTraceProfile();
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

FGameplayEffectSpecHandle USkillProjectileCastAction::MakeDamageEffectSpec(const float ChargeDamageAlpha) const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{

		return FGameplayEffectSpecHandle();
	}

	const FSkillGameplayEffectConfig DamageConfig = SkillDataAsset->GetResolvedDamageConfig();
	if (!DamageConfig.GameplayEffectClass)
	{

		return FGameplayEffectSpecHandle();
	}

	const float ScaledDamage = GetAbility()->CalculateBaseSkillDamageMagnitude(DamageConfig);
	const float FullDamage = GetAbility()->ApplyIntelligenceToSkillDamage(ScaledDamage);
	const float ClampedChargeDamageAlpha = FMath::Clamp(ChargeDamageAlpha, 0.0f, 1.0f);
	const float CalculatedDamage = FullDamage * ClampedChargeDamageAlpha;
	return GetAbility()->MakeConfiguredDamageEffectSpec(DamageConfig, CalculatedDamage);
}

FGameplayEffectSpecHandle USkillProjectileCastAction::MakeStatusEffectSpec() const
{
	return GetAbility()->MakeConfiguredStatusEffectSpec(
		GetAbility()->GetSourceSkillDataAsset(),
		GetConfiguredStatusEffectClass(),
		GetConfiguredStatusEffectLevel());
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
	const FGameplayAbilityActorInfo* ActorInfo = GetAbility()->GetCurrentActorInfo();
	const AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
	if (AvatarActor && !AvatarActor->HasAuthority())
	{
		Finish();
		return;
	}
	Finish();
}
