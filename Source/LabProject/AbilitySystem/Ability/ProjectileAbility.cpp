#include "AbilitySystem/Ability/ProjectileAbility.h"

#include "Abilities/GameplayAbilityTargetActor_SingleLineTrace.h"
#include "Abilities/GameplayAbilityTargetActor_Trace.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitConfirmCancel.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/Projectiles/ProjectileBase.h"
#include "AbilitySystem/Skills/SkillTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Character/PdCharacterBase.h"
#include "Character/PdPlayer.h"
#include "Common/LabGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Mode/PdHUD.h"
#include "Mode/PdPlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProjectileAbility)

DEFINE_LOG_CATEGORY_STATIC(LogPandoraProjectileAbility, Log, All);

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
	bWaitingForProjectileAimRelease = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ProjectileAimReleaseTimerHandle);
	}
	const AActor* AvatarActor = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor.Get() : nullptr;
	const APawn* AvatarPawn = Cast<APawn>(AvatarActor);
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("Projectile ability cancelled: missing SkillDataAsset. ability=%s avatar=%s"),
			*GetNameSafe(this),
			*GetNameSafe(AvatarActor));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!SkillDataAsset->ProjectileClass || SkillDataAsset->ProjectileSpeed <= 0.0)
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("Projectile ability cancelled: invalid SkillDataAsset projectile values. skill=%s projectileClass=%s speed=%.2f"),
			*GetNameSafe(SkillDataAsset),
			*GetNameSafe(SkillDataAsset->ProjectileClass.Get()),
			SkillDataAsset->ProjectileSpeed);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FGameplayTag ConfiguredShootEventTag = GetConfiguredShootProjectileEventTag();
	UE_LOG(LogPandoraProjectileAbility, Log,
		TEXT("Activate: ability=%s avatar=%s authority=%s local=%s montage=%s projectileClass=%s damageEffect=%s eventTag=%s speed=%.1f"),
		*GetNameSafe(this),
		*GetNameSafe(AvatarActor),
		AvatarActor && AvatarActor->HasAuthority() ? TEXT("true") : TEXT("false"),
		AvatarPawn && AvatarPawn->IsLocallyControlled() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(GetConfiguredShootMontage()),
		*GetNameSafe(GetConfiguredProjectileClass().Get()),
		*GetNameSafe(GetConfiguredDamageEffectClass().Get()),
		*ConfiguredShootEventTag.ToString(),
		GetConfiguredProjectileSpeed());

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
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FVector UProjectileAbility::GetSpawnLocation() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		return FVector::ZeroVector;
	}

	const APdCharacterBase* Character = Cast<APdCharacterBase>(AvatarActor);
	const USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;
	const FName ConfiguredSpawnSocketName = GetConfiguredSpawnSocketName();
	if (CharacterMesh && !ConfiguredSpawnSocketName.IsNone() && CharacterMesh->DoesSocketExist(ConfiguredSpawnSocketName))
	{
		const FVector SocketLocation = CharacterMesh->GetSocketLocation(ConfiguredSpawnSocketName);
		UE_LOG(LogPandoraProjectileAbility, Log,
			TEXT("SpawnLocation: using socket. avatar=%s socket=%s location=%s"),
			*GetNameSafe(AvatarActor),
			*ConfiguredSpawnSocketName.ToString(),
			*SocketLocation.ToCompactString());
		return SocketLocation;
	}

	if (!ConfiguredSpawnSocketName.IsNone())
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("SpawnLocation fallback: socket not found. avatar=%s socket=%s mesh=%s"),
			*GetNameSafe(AvatarActor),
			*ConfiguredSpawnSocketName.ToString(),
			*GetNameSafe(CharacterMesh));
	}

	FVector SpawnForward = AvatarActor->GetActorForwardVector();
	const FVector HorizontalForward = FVector(SpawnForward.X, SpawnForward.Y, 0.0f).GetSafeNormal();
	if (!HorizontalForward.IsNearlyZero())
	{
		SpawnForward = HorizontalForward;
	}

	const FVector ConfiguredSpawnLocationOffset = GetConfiguredSpawnLocationOffset();
	const float ForwardSpawnDistance = FMath::Max(ConfiguredSpawnLocationOffset.X, GetConfiguredMinimumForwardSpawnOffset());
	const FVector ForwardOffset = SpawnForward * ForwardSpawnDistance;
	const FVector RightOffset = AvatarActor->GetActorRightVector() * ConfiguredSpawnLocationOffset.Y;
	const FVector UpOffset = AvatarActor->GetActorUpVector() * ConfiguredSpawnLocationOffset.Z;
	const FVector OffsetLocation = AvatarActor->GetActorLocation() + ForwardOffset + RightOffset + UpOffset;
	UE_LOG(LogPandoraProjectileAbility, Log,
		TEXT("SpawnLocation: using offset. avatar=%s configuredOffset=%s forward=%s forwardDistance=%.1f location=%s"),
		*GetNameSafe(AvatarActor),
		*ConfiguredSpawnLocationOffset.ToCompactString(),
		*SpawnForward.ToCompactString(),
		ForwardSpawnDistance,
		*OffsetLocation.ToCompactString());
	return OffsetLocation;
}

void UProjectileAbility::ShootProjectile_Implementation(FVector TargetLocation)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	const TSubclassOf<AProjectileBase> ConfiguredProjectileClass = GetConfiguredProjectileClass();
	if (!AvatarActor || !AvatarActor->HasAuthority() || !World || !ConfiguredProjectileClass)
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("ShootProjectile skipped: avatar=%s authority=%s world=%s projectileClass=%s target=%s"),
			*GetNameSafe(AvatarActor),
			AvatarActor && AvatarActor->HasAuthority() ? TEXT("true") : TEXT("false"),
			World ? TEXT("valid") : TEXT("null"),
			*GetNameSafe(ConfiguredProjectileClass.Get()),
			*TargetLocation.ToCompactString());
		return;
	}

	const FVector SpawnLocation = GetSpawnLocation();
	if (TargetLocation.IsNearlyZero() || FVector::DistSquared(SpawnLocation, TargetLocation) <= UE_KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("ShootProjectile target invalid or too close. spawn=%s target=%s, using fallback."),
			*SpawnLocation.ToCompactString(),
			*TargetLocation.ToCompactString());
		TargetLocation = ResolveFallbackTargetLocation();
	}

	const FVector SpawnDirection = (TargetLocation - SpawnLocation).GetSafeNormal();
	const FRotator SpawnRotation = SpawnDirection.IsNearlyZero()
		? AvatarActor->GetActorRotation()
		: SpawnDirection.Rotation();
	const FTransform SpawnTransform(SpawnRotation, SpawnLocation);

	APawn* InstigatorPawn = Cast<APawn>(AvatarActor);
	AProjectileBase* Projectile = World->SpawnActorDeferred<AProjectileBase>(
		ConfiguredProjectileClass,
		SpawnTransform,
		AvatarActor,
		InstigatorPawn,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("ShootProjectile failed: SpawnActorDeferred returned null. class=%s spawn=%s target=%s"),
			*GetNameSafe(ConfiguredProjectileClass.Get()),
			*SpawnLocation.ToCompactString(),
			*TargetLocation.ToCompactString());
		return;
	}

	const FGameplayEffectSpecHandle DamageSpecHandle = MakeDamageEffectSpec();
	UE_LOG(LogPandoraProjectileAbility, Log,
		TEXT("ShootProjectile spawning: projectile=%s class=%s spawn=%s target=%s speed=%.1f damageSpecValid=%s"),
		*GetNameSafe(Projectile),
		*GetNameSafe(ConfiguredProjectileClass.Get()),
		*SpawnLocation.ToCompactString(),
		*TargetLocation.ToCompactString(),
		GetConfiguredProjectileSpeed(),
		DamageSpecHandle.IsValid() ? TEXT("true") : TEXT("false"));

	Projectile->InitializeProjectile(TargetLocation, GetConfiguredProjectileSpeed(), DamageSpecHandle);
	if (const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset())
	{
		Projectile->SetImpactEffectAreaSpawnConfigs(
			GetSourceProjectileImpactEffectAreasForLevel(FMath::Max(GetAbilityLevel(), 1)),
			FMath::Max(GetAbilityLevel(), 1));
	}
	UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
}

void UProjectileAbility::HandleMontageFinished()
{
	UE_LOG(LogPandoraProjectileAbility, Log,
		TEXT("Montage finished: ability=%s"),
		*GetNameSafe(this));
	K2_EndAbility();
}

void UProjectileAbility::HandleShootProjectileEvent(FGameplayEventData Payload)
{
	UE_LOG(LogPandoraProjectileAbility, Log,
		TEXT("Shoot event received: ability=%s payloadTag=%s avatar=%s authority=%s hasPlayerController=%s"),
		*GetNameSafe(this),
		*Payload.EventTag.ToString(),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		HasAuthority(&CurrentActivationInfo) ? TEXT("true") : TEXT("false"),
		HasPlayerController() ? TEXT("true") : TEXT("false"));

	if (!HasPlayerController())
	{
		if (AActor* AttackTarget = GetAttackTargetFromAvatar(); IsValid(AttackTarget))
		{
			const FVector TargetLocation = AttackTarget->GetActorLocation();
			UE_LOG(LogPandoraProjectileAbility, Log,
				TEXT("Shoot event using attack target: ability=%s target=%s location=%s"),
				*GetNameSafe(this),
				*GetNameSafe(AttackTarget),
				*TargetLocation.ToCompactString());
			ShootProjectile(TargetLocation);
			if (bEndAfterProjectileFired)
			{
				K2_EndAbility();
			}
			return;
		}

		UE_LOG(LogPandoraProjectileAbility, Log,
			TEXT("Shoot event cancelled: AI avatar has no living attack target. ability=%s avatar=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetAvatarActorFromActorInfo()));
		K2_EndAbility();
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

	UE_LOG(LogPandoraProjectileAbility, Log,
		TEXT("Confirm pressed: ability=%s avatar=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetAvatarActorFromActorInfo()));
	ConfirmPlayerShot();
}

void UProjectileAbility::HandleCancelPressed()
{
	UE_LOG(LogPandoraProjectileAbility, Log,
		TEXT("Cancel pressed: ability=%s avatar=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetAvatarActorFromActorInfo()));
	K2_CancelAbility();
}

void UProjectileAbility::StartPlayerAiming()
{
	bWaitingForPlayerConfirm = true;
	ApplyProjectileAimCamera(true);
	ShowProjectileCrosshair(true);

	if (ConfirmCancelTask)
	{
		ConfirmCancelTask->EndTask();
		ConfirmCancelTask = nullptr;
	}

	ConfirmCancelTask = UAbilityTask_WaitConfirmCancel::WaitConfirmCancel(this);
	if (!ConfirmCancelTask)
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("StartPlayerAiming failed: confirm task is null. ability=%s"),
			*GetNameSafe(this));
		K2_CancelAbility();
		return;
	}

	ConfirmCancelTask->OnConfirm.AddDynamic(this, &ThisClass::HandleConfirmPressed);
	ConfirmCancelTask->OnCancel.AddDynamic(this, &ThisClass::HandleCancelPressed);
	ConfirmCancelTask->ReadyForActivation();

	UE_LOG(LogPandoraProjectileAbility, Log,
		TEXT("Player aiming started: ability=%s avatar=%s camera=%s crosshair=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetConfiguredProjectileCameraSettings().TargetBoomSocketOffset.ToString(),
		*GetConfiguredProjectileCrosshairWidgetTag().ToString());
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
	const AActor* AvatarActor = ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor.Get() : nullptr;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("BeginConfirmedShot failed: CommitAbility returned false. ability=%s avatar=%s"),
			*GetNameSafe(this),
			*GetNameSafe(AvatarActor));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	StartShootProjectileEventTask();

	UAnimMontage* MontageToPlay = GetConfiguredShootMontage();
	if (!MontageToPlay)
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("BeginConfirmedShot without montage: %s. ability=%s avatar=%s"),
			HasPlayerController() ? TEXT("waiting for player confirm") : TEXT("firing immediately"),
			*GetNameSafe(this),
			*GetNameSafe(AvatarActor));
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

	ShootMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		MontageToPlay,
		1.0f,
		NAME_None,
		true,
		1.0f,
		0.0f,
		true);
	if (!ShootMontageTask)
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("BeginConfirmedShot failed: could not create montage task. ability=%s montage=%s"),
			*GetNameSafe(this),
			*GetNameSafe(MontageToPlay));
		EndAbilityFromActivation(Handle, ActorInfo, ActivationInfo);
		return;
	}

	ShootMontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	ShootMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	ShootMontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageFinished);
	ShootMontageTask->ReadyForActivation();

	UE_LOG(LogPandoraProjectileAbility, Log,
		TEXT("Montage started: ability=%s montage=%s waitingEvent=%s"),
		*GetNameSafe(this),
		*GetNameSafe(MontageToPlay),
		*GetConfiguredShootProjectileEventTag().ToString());

	if (HasPlayerController())
	{
		bPausedForPlayerAim = true;
		PauseProjectileMontageForAiming();
		StartPlayerAiming();
	}
}

void UProjectileAbility::ConfirmPlayerShot()
{
	const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	UE_LOG(LogPandoraProjectileAbility, Warning,
		TEXT("[ProjectileCameraDebug] ConfirmPlayerShot ability=%s avatar=%s authority=%s local=%s pausedForAim=%s endAfterFire=%s profile=%s releaseDelay=%.2f"),
		*GetNameSafe(this),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		HasAuthority(&CurrentActivationInfo) ? TEXT("true") : TEXT("false"),
		AvatarPawn && AvatarPawn->IsLocallyControlled() ? TEXT("true") : TEXT("false"),
		bPausedForPlayerAim ? TEXT("true") : TEXT("false"),
		bEndAfterProjectileFired ? TEXT("true") : TEXT("false"),
		*GetConfiguredTargetTraceProfile().Name.ToString(),
		GetConfiguredProjectileAimReleaseDelay());

	bWaitingForPlayerConfirm = false;
	bPlayerProjectileConfirmed = true;

	if (ConfirmCancelTask)
	{
		ConfirmCancelTask->EndTask();
		ConfirmCancelTask = nullptr;
	}

	if (!bPausedForPlayerAim)
	{
		bEndAfterProjectileFired = true;
	}

	if (IsLocallyControlledPlayer())
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("[ProjectileCameraDebug] Confirm starts local camera release timer ability=%s avatar=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetAvatarActorFromActorInfo()));
		StartProjectileAimReleaseDelay();
	}
	else
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("[ProjectileCameraDebug] Confirm did not start release timer because this instance is not local ability=%s avatar=%s hasPlayerController=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetAvatarActorFromActorInfo()),
			HasPlayerController() ? TEXT("true") : TEXT("false"));
	}

	if (GetConfiguredTargetTraceProfile().Name == TEXT("NoCollision"))
	{
		ShootProjectile(ResolveFallbackTargetLocation());
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

void UProjectileAbility::HandleTargetDataValid(const FGameplayAbilityTargetDataHandle& Data)
{
	FVector TargetLocation = UAbilitySystemBlueprintLibrary::GetTargetDataEndPoint(Data, 0);
	UE_LOG(LogPandoraProjectileAbility, Log,
		TEXT("TargetData valid: ability=%s numData=%d endpoint=%s"),
		*GetNameSafe(this),
		Data.Num(),
		*TargetLocation.ToCompactString());
	if (TargetLocation.IsNearlyZero())
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("TargetData endpoint is zero, trying player aim fallback. ability=%s"),
			*GetNameSafe(this));
		TryResolveProjectileAimTargetLocation(TargetLocation);
	}
	else if (ShouldUseAimFallbackForTarget(TargetLocation))
	{
		FVector AimTargetLocation = FVector::ZeroVector;
		if (TryResolveProjectileAimTargetLocation(AimTargetLocation))
		{
			UE_LOG(LogPandoraProjectileAbility, Warning,
				TEXT("TargetData endpoint rejected for projectile flight. ability=%s endpoint=%s replacement=%s"),
				*GetNameSafe(this),
				*TargetLocation.ToCompactString(),
				*AimTargetLocation.ToCompactString());
			TargetLocation = AimTargetLocation;
		}
	}

	ShootProjectile(TargetLocation);
	if (bEndAfterProjectileFired)
	{
		K2_EndAbility();
	}
}

void UProjectileAbility::HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data)
{
	UE_LOG(LogPandoraProjectileAbility, Warning,
		TEXT("TargetData cancelled: ability=%s numData=%d"),
		*GetNameSafe(this),
		Data.Num());

	if (HasPlayerController() && bPlayerProjectileConfirmed)
	{
		ReleaseProjectileAimState();
		K2_CancelAbility();
		return;
	}

	if (bEndAfterProjectileFired)
	{
		K2_EndAbility();
	}
}

void UProjectileAbility::StartShootProjectileEventTask()
{
	const FGameplayTag ConfiguredShootEventTag = GetConfiguredShootProjectileEventTag();
	if (!ConfiguredShootEventTag.IsValid())
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("Shoot event task skipped: ShootProjectileEventTag is invalid. ability=%s"),
			*GetNameSafe(this));
		return;
	}

	if (ShootProjectileEventTask)
	{
		ShootProjectileEventTask->EndTask();
		ShootProjectileEventTask = nullptr;
	}

	ShootProjectileEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		ConfiguredShootEventTag,
		nullptr,
		false,
		true);
	if (!ShootProjectileEventTask)
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("Shoot event task creation failed: ability=%s tag=%s"),
			*GetNameSafe(this),
			*ConfiguredShootEventTag.ToString());
		return;
	}

	ShootProjectileEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleShootProjectileEvent);
	ShootProjectileEventTask->ReadyForActivation();
	UE_LOG(LogPandoraProjectileAbility, Log,
		TEXT("Shoot event task ready: ability=%s tag=%s"),
		*GetNameSafe(this),
		*ConfiguredShootEventTag.ToString());
}

void UProjectileAbility::WaitForPlayerTargetData()
{
	const FCollisionProfileName ConfiguredTargetTraceProfile = GetConfiguredTargetTraceProfile();
	UE_LOG(LogPandoraProjectileAbility, Log,
		TEXT("WaitForPlayerTargetData: ability=%s range=%.1f profile=%s debug=%s"),
		*GetNameSafe(this),
		GetConfiguredTargetTraceMaxRange(),
		*ConfiguredTargetTraceProfile.Name.ToString(),
		GetConfiguredDrawTargetTraceDebug() ? TEXT("true") : TEXT("false"));

	if (TargetDataTask)
	{
		TargetDataTask->EndTask();
		TargetDataTask = nullptr;
	}

	TargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(
		this,
		NAME_None,
		EGameplayTargetingConfirmation::Instant,
		AGameplayAbilityTargetActor_SingleLineTrace::StaticClass());
	if (!TargetDataTask)
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("WaitForPlayerTargetData failed: target data task is null. ability=%s"),
			*GetNameSafe(this));
		ShootProjectile(ResolveFallbackTargetLocation());
		return;
	}

	TargetDataTask->ValidData.AddDynamic(this, &ThisClass::HandleTargetDataValid);
	TargetDataTask->Cancelled.AddDynamic(this, &ThisClass::HandleTargetDataCancelled);

	AGameplayAbilityTargetActor* SpawnedActor = nullptr;
	if (TargetDataTask->BeginSpawningActor(this, AGameplayAbilityTargetActor_SingleLineTrace::StaticClass(), SpawnedActor))
	{
		if (AGameplayAbilityTargetActor_Trace* TraceActor = Cast<AGameplayAbilityTargetActor_Trace>(SpawnedActor))
		{
			TraceActor->MaxRange = GetConfiguredTargetTraceMaxRange();
			TraceActor->TraceProfile = ConfiguredTargetTraceProfile;
			TraceActor->bTraceAffectsAimPitch = GetConfiguredTraceAffectsAimPitch();
		}

		SpawnedActor->StartLocation = MakeTargetLocationInfoFromOwnerActor();
		SpawnedActor->bDebug = GetConfiguredDrawTargetTraceDebug();
		TargetDataTask->FinishSpawningActor(this, SpawnedActor);
		UE_LOG(LogPandoraProjectileAbility, Log,
			TEXT("Target actor spawned: ability=%s actor=%s"),
			*GetNameSafe(this),
			*GetNameSafe(SpawnedActor));
	}
	else
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("Target actor BeginSpawningActor returned false. ability=%s"),
			*GetNameSafe(this));
	}

	TargetDataTask->ReadyForActivation();
}

bool UProjectileAbility::ShouldUseAimFallbackForTarget(const FVector& TargetLocation) const
{
	const FVector SpawnLocation = GetSpawnLocation();
	const float MinimumDistance = FMath::Max(GetConfiguredMinimumTargetDistanceFromSpawn(), 0.0f);
	const bool bTooClose = MinimumDistance > 0.0f
		&& FVector::DistSquared(SpawnLocation, TargetLocation) < FMath::Square(MinimumDistance);
	const bool bStronglyDownward = TargetLocation.Z < SpawnLocation.Z - 50.0f
		&& FVector::DistSquared2D(SpawnLocation, TargetLocation) < FMath::Square(MinimumDistance);

	if (bTooClose || bStronglyDownward)
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("TargetData endpoint looks unsuitable. ability=%s spawn=%s target=%s tooClose=%s downward=%s minDistance=%.1f"),
			*GetNameSafe(this),
			*SpawnLocation.ToCompactString(),
			*TargetLocation.ToCompactString(),
			bTooClose ? TEXT("true") : TEXT("false"),
			bStronglyDownward ? TEXT("true") : TEXT("false"),
			MinimumDistance);
		return true;
	}

	return false;
}

bool UProjectileAbility::TryResolveProjectileAimTargetLocation(FVector& OutTargetLocation) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const float ConfiguredTargetTraceMaxRange = GetConfiguredTargetTraceMaxRange();
	if (!AvatarActor || ConfiguredTargetTraceMaxRange <= 0.0f)
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("Aim target fallback failed: avatar=%s range=%.1f"),
			*GetNameSafe(AvatarActor),
			ConfiguredTargetTraceMaxRange);
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
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("Aim target fallback failed: view direction is zero. avatar=%s"),
			*GetNameSafe(AvatarActor));
		return false;
	}

	const FVector ViewTraceEnd = ViewTraceStart + (AimDirection * ConfiguredTargetTraceMaxRange);
	const FCollisionProfileName ConfiguredTargetTraceProfile = GetConfiguredTargetTraceProfile();
	if (ConfiguredTargetTraceProfile.Name == TEXT("NoCollision"))
	{
		OutTargetLocation = ViewTraceEnd;
		UE_LOG(LogPandoraProjectileAbility, Log,
			TEXT("Aim target fallback resolved from view without collision trace: start=%s direction=%s end=%s profile=%s"),
			*ViewTraceStart.ToCompactString(),
			*AimDirection.ToCompactString(),
			*OutTargetLocation.ToCompactString(),
			*ConfiguredTargetTraceProfile.Name.ToString());
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
	UE_LOG(LogPandoraProjectileAbility, Log,
		TEXT("Aim target fallback resolved: hit=%s start=%s end=%s target=%s hitActor=%s"),
		bHit ? TEXT("true") : TEXT("false"),
		*ViewTraceStart.ToCompactString(),
		*ViewTraceEnd.ToCompactString(),
		*OutTargetLocation.ToCompactString(),
		*GetNameSafe(ViewHitResult.GetActor()));
	return true;
}

FVector UProjectileAbility::ResolveFallbackTargetLocation() const
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

FGameplayEffectSpecHandle UProjectileAbility::MakeDamageEffectSpec() const
{
	UPdAbilitySystemComponent* SourceASC = GetPdAbilitySystemComponentFromActorInfo();
	const TSubclassOf<UGameplayEffect> ConfiguredDamageEffectClass = GetConfiguredDamageEffectClass();
	if (!SourceASC || !ConfiguredDamageEffectClass)
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("MakeDamageEffectSpec failed: sourceASC=%s damageEffect=%s ability=%s"),
			*GetNameSafe(SourceASC),
			*GetNameSafe(ConfiguredDamageEffectClass.Get()),
			*GetNameSafe(this));
		return FGameplayEffectSpecHandle();
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());
	if (const FGameplayAbilitySpec* AbilitySpec = GetCurrentAbilitySpec())
	{
		if (UObject* SourceObject = AbilitySpec->SourceObject.Get())
		{
			EffectContext.AddSourceObject(SourceObject);
		}
	}

	FGameplayEffectSpecHandle DamageEffectSpecHandle =
		SourceASC->MakeOutgoingSpec(ConfiguredDamageEffectClass, FMath::Max(GetAbilityLevel(), 1), EffectContext);
	if (!DamageEffectSpecHandle.IsValid() || !DamageEffectSpecHandle.Data.IsValid())
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("MakeDamageEffectSpec failed: MakeOutgoingSpec invalid. effect=%s level=%d ability=%s"),
			*GetNameSafe(ConfiguredDamageEffectClass.Get()),
			FMath::Max(GetAbilityLevel(), 1),
			*GetNameSafe(this));
		return FGameplayEffectSpecHandle();
	}

	FGameplayTag ResolvedDamageDataTag = GetConfiguredDamageDataTag();
	if (!ResolvedDamageDataTag.IsValid())
	{
		SourceASC->ResolveDamageMagnitudeSetByCallerTag(ResolvedDamageDataTag);
	}

	if (ResolvedDamageDataTag.IsValid())
	{
		const double AbilityLevel = static_cast<double>(FMath::Max(GetAbilityLevel(), 1));
		const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
		if (!SkillDataAsset)
		{
			UE_LOG(LogPandoraProjectileAbility, Warning,
				TEXT("Damage spec failed: missing SkillDataAsset. ability=%s"),
				*GetNameSafe(this));
			return FGameplayEffectSpecHandle();
		}

		const double ConfiguredDamageMagnitude = SkillDataAsset->ProjectileDamageMagnitude;
		const double ConfiguredDamagePercentIncreasePerLevel = SkillDataAsset->ProjectileDamagePercentIncreasePerLevel;
		const float CalculatedDamage = static_cast<float>(
			ConfiguredDamageMagnitude
			+ ((ConfiguredDamageMagnitude * ConfiguredDamagePercentIncreasePerLevel) * (AbilityLevel - 1.0)));
		DamageEffectSpecHandle.Data->SetSetByCallerMagnitude(ResolvedDamageDataTag, CalculatedDamage);
		UE_LOG(LogPandoraProjectileAbility, Log,
			TEXT("Damage spec ready: effect=%s tag=%s magnitude=%.2f level=%.0f sourceObject=%s"),
			*GetNameSafe(ConfiguredDamageEffectClass.Get()),
			*ResolvedDamageDataTag.ToString(),
			CalculatedDamage,
			AbilityLevel,
			DamageEffectSpecHandle.Data->GetContext().GetSourceObject()
				? *GetNameSafe(DamageEffectSpecHandle.Data->GetContext().GetSourceObject())
				: TEXT("None"));
	}
	else
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("Damage spec missing SetByCaller tag: effect=%s ability=%s"),
			*GetNameSafe(ConfiguredDamageEffectClass.Get()),
			*GetNameSafe(this));
	}

	return DamageEffectSpecHandle;
}

UAnimMontage* UProjectileAbility::GetConfiguredShootMontage() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->ProjectileShootMontage.Get() : nullptr;
}

TSubclassOf<AProjectileBase> UProjectileAbility::GetConfiguredProjectileClass() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->ProjectileClass : nullptr;
}

TSubclassOf<UGameplayEffect> UProjectileAbility::GetConfiguredDamageEffectClass() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->ProjectileDamageEffectClass : nullptr;
}

float UProjectileAbility::GetConfiguredProjectileSpeed() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? static_cast<float>(SkillDataAsset->ProjectileSpeed) : 0.0f;
}

FGameplayTag UProjectileAbility::GetConfiguredDamageDataTag() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->ProjectileDamageDataTag : FGameplayTag();
}

FGameplayTag UProjectileAbility::GetConfiguredShootProjectileEventTag() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->ShootProjectileEventTag : FGameplayTag();
}

float UProjectileAbility::GetConfiguredTargetTraceMaxRange() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? static_cast<float>(SkillDataAsset->TargetTraceMaxRange) : 0.0f;
}

FCollisionProfileName UProjectileAbility::GetConfiguredTargetTraceProfile() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->TargetTraceProfile : FCollisionProfileName(TEXT("NoCollision"));
}

float UProjectileAbility::GetConfiguredMinimumTargetDistanceFromSpawn() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? static_cast<float>(SkillDataAsset->MinimumTargetDistanceFromSpawn) : 0.0f;
}

bool UProjectileAbility::GetConfiguredTraceAffectsAimPitch() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset && SkillDataAsset->bTraceAffectsAimPitch;
}

bool UProjectileAbility::GetConfiguredDrawTargetTraceDebug() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset && SkillDataAsset->bDrawTargetTraceDebug;
}

FName UProjectileAbility::GetConfiguredSpawnSocketName() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->SpawnSocketName : NAME_None;
}

FVector UProjectileAbility::GetConfiguredSpawnLocationOffset() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->SpawnLocationOffset : FVector::ZeroVector;
}

float UProjectileAbility::GetConfiguredMinimumForwardSpawnOffset() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? static_cast<float>(SkillDataAsset->MinimumForwardSpawnOffset) : 0.0f;
}

FWeaponAimCameraSettings UProjectileAbility::GetConfiguredProjectileCameraSettings() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	if (SkillDataAsset && SkillDataAsset->bUseProjectileCameraSettings)
	{
		return SkillDataAsset->ProjectileCameraSettings;
	}

	return FWeaponAimCameraSettings();
}

FGameplayTag UProjectileAbility::GetConfiguredProjectileCrosshairWidgetTag() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->ProjectileCrosshairWidgetTag : FGameplayTag();
}

float UProjectileAbility::GetConfiguredProjectileAimReleaseDelay() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? static_cast<float>(FMath::Max(SkillDataAsset->ProjectileAimReleaseDelay, 0.0)) : 0.0f;
}

bool UProjectileAbility::HasPlayerController() const
{
	const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	const AController* Controller = AvatarPawn ? AvatarPawn->GetController() : nullptr;
	return Controller && Controller->IsPlayerController();
}

bool UProjectileAbility::IsLocallyControlledPlayer() const
{
	const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	return AvatarPawn && AvatarPawn->IsLocallyControlled() && HasPlayerController();
}

void UProjectileAbility::ApplyProjectileAimCamera(const bool bEnabled) const
{
	APdPlayer* Player = Cast<APdPlayer>(GetAvatarActorFromActorInfo());
	if (!Player)
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("[ProjectileCameraDebug] ApplyProjectileAimCamera skipped: no APdPlayer ability=%s avatar=%s enabled=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetAvatarActorFromActorInfo()),
			bEnabled ? TEXT("true") : TEXT("false"));
		return;
	}

	const FWeaponAimCameraSettings CameraSettings = bEnabled ? GetConfiguredProjectileCameraSettings() : FWeaponAimCameraSettings();
	UE_LOG(LogPandoraProjectileAbility, Warning,
		TEXT("[ProjectileCameraDebug] ApplyProjectileAimCamera ability=%s player=%s enabled=%s local=%s fov=%.1f offset=%s rotation=%s interp=%.1f"),
		*GetNameSafe(this),
		*GetNameSafe(Player),
		bEnabled ? TEXT("true") : TEXT("false"),
		Player->IsLocallyControlled() ? TEXT("true") : TEXT("false"),
		CameraSettings.TargetFOV,
		*CameraSettings.TargetBoomSocketOffset.ToCompactString(),
		*CameraSettings.TargetCameraRotation.ToCompactString(),
		CameraSettings.InterpSpeed);
	Player->SetWeaponAimActive(bEnabled, CameraSettings);
	Player->SetAbilityCameraOverrideActive(bEnabled, CameraSettings);
}

void UProjectileAbility::ShowProjectileCrosshair(const bool bEnabled) const
{
	const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	const APdPlayerController* Controller = AvatarPawn ? Cast<APdPlayerController>(AvatarPawn->GetController()) : nullptr;
	APdHUD* HUD = Controller ? Cast<APdHUD>(Controller->GetHUD()) : nullptr;
	if (!HUD)
	{
		return;
	}

	if (bEnabled)
	{
		HUD->ShowAimCrosshair(GetConfiguredProjectileCrosshairWidgetTag());
		return;
	}

	HUD->HideAimCrosshair();
}

void UProjectileAbility::ReleaseProjectileAimAnimationState() const
{
	APdPlayer* Player = Cast<APdPlayer>(GetAvatarActorFromActorInfo());
	if (!Player)
	{
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("[ProjectileCameraDebug] ReleaseProjectileAimAnimationState skipped: no APdPlayer ability=%s avatar=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetAvatarActorFromActorInfo()));
		return;
	}

	UE_LOG(LogPandoraProjectileAbility, Warning,
		TEXT("[ProjectileCameraDebug] ReleaseProjectileAimAnimationState ability=%s player=%s local=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Player),
		Player->IsLocallyControlled() ? TEXT("true") : TEXT("false"));
	Player->SetWeaponAimActive(false, FWeaponAimCameraSettings());
}

void UProjectileAbility::PauseProjectileMontageForAiming()
{
	UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->CurrentMontageSetPlayRate(0.0f);
	UE_LOG(LogPandoraProjectileAbility, Log,
		TEXT("Projectile montage paused for aiming: ability=%s montage=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetCurrentMontage()));
}

void UProjectileAbility::ResumeProjectileMontageAfterAiming()
{
	UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->CurrentMontageSetPlayRate(1.0f);
	UE_LOG(LogPandoraProjectileAbility, Log,
		TEXT("Projectile montage resumed after aiming: ability=%s montage=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetCurrentMontage()));
}

void UProjectileAbility::StartProjectileAimReleaseDelay()
{
	ReleaseProjectileAimAnimationState();

	if (!IsLocallyControlledPlayer())
	{
		const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("[ProjectileCameraDebug] StartProjectileAimReleaseDelay skipped: not local ability=%s avatar=%s pawnLocal=%s hasPlayerController=%s endAfterFire=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetAvatarActorFromActorInfo()),
			AvatarPawn && AvatarPawn->IsLocallyControlled() ? TEXT("true") : TEXT("false"),
			HasPlayerController() ? TEXT("true") : TEXT("false"),
			bEndAfterProjectileFired ? TEXT("true") : TEXT("false"));
		if (bEndAfterProjectileFired)
		{
			K2_EndAbility();
		}
		return;
	}

	bWaitingForProjectileAimRelease = true;
	const float ReleaseDelay = GetConfiguredProjectileAimReleaseDelay();
	APdPlayer* Player = Cast<APdPlayer>(GetAvatarActorFromActorInfo());
	UE_LOG(LogPandoraProjectileAbility, Warning,
		TEXT("[ProjectileCameraDebug] StartProjectileAimReleaseDelay ability=%s avatar=%s player=%s delay=%.2f world=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(Player),
		ReleaseDelay,
		GetWorld() ? TEXT("valid") : TEXT("null"));

	if (Player)
	{
		Player->SetAbilityCameraOverrideActiveForDuration(true, GetConfiguredProjectileCameraSettings(), ReleaseDelay);
		ShowProjectileCrosshair(false);
		UE_LOG(LogPandoraProjectileAbility, Warning,
			TEXT("[ProjectileCameraDebug] Release timer delegated to player ability=%s player=%s delay=%.2f"),
			*GetNameSafe(this),
			*GetNameSafe(Player),
			ReleaseDelay);
		return;
	}

	UE_LOG(LogPandoraProjectileAbility, Warning,
		TEXT("[ProjectileCameraDebug] Release delay fallback: no player, releasing immediately ability=%s delay=%.2f"),
		*GetNameSafe(this),
		ReleaseDelay);
	ReleaseProjectileAimState();
}

void UProjectileAbility::ReleaseProjectileAimState()
{
	const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	UE_LOG(LogPandoraProjectileAbility, Warning,
		TEXT("[ProjectileCameraDebug] ReleaseProjectileAimState ENTER ability=%s avatar=%s authority=%s local=%s waitingRelease=%s world=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		HasAuthority(&CurrentActivationInfo) ? TEXT("true") : TEXT("false"),
		AvatarPawn && AvatarPawn->IsLocallyControlled() ? TEXT("true") : TEXT("false"),
		bWaitingForProjectileAimRelease ? TEXT("true") : TEXT("false"),
		GetWorld() ? TEXT("valid") : TEXT("null"));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ProjectileAimReleaseTimerHandle);
	}

	bWaitingForProjectileAimRelease = false;
	ApplyProjectileAimCamera(false);
	ShowProjectileCrosshair(false);

	UE_LOG(LogPandoraProjectileAbility, Warning,
		TEXT("[ProjectileCameraDebug] ReleaseProjectileAimState EXIT ability=%s"),
		*GetNameSafe(this));
}

void UProjectileAbility::CleanupAimingState()
{
	bWaitingForPlayerConfirm = false;
	bPlayerProjectileConfirmed = false;
	const bool bKeepAimUntilReleaseTimer = bWaitingForProjectileAimRelease;
	UE_LOG(LogPandoraProjectileAbility, Warning,
		TEXT("[ProjectileCameraDebug] CleanupAimingState ability=%s avatar=%s keepTimer=%s timerActive=%s timerRemaining=%.2f pausedForAim=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		bKeepAimUntilReleaseTimer ? TEXT("true") : TEXT("false"),
		GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(ProjectileAimReleaseTimerHandle) ? TEXT("true") : TEXT("false"),
		GetWorld() ? GetWorld()->GetTimerManager().GetTimerRemaining(ProjectileAimReleaseTimerHandle) : -1.0f,
		bPausedForPlayerAim ? TEXT("true") : TEXT("false"));
	ReleaseProjectileAimAnimationState();

	if (bPausedForPlayerAim)
	{
		ResumeProjectileMontageAfterAiming();
		bPausedForPlayerAim = false;
	}

	if (!bKeepAimUntilReleaseTimer)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ProjectileAimReleaseTimerHandle);
		}
		ApplyProjectileAimCamera(false);
		ShowProjectileCrosshair(false);
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
		TargetDataTask->EndTask();
		TargetDataTask = nullptr;
	}

	UE_LOG(LogPandoraProjectileAbility, Log,
		TEXT("Cleanup aiming state: ability=%s keepAimUntilReleaseTimer=%s"),
		*GetNameSafe(this),
		bKeepAimUntilReleaseTimer ? TEXT("true") : TEXT("false"));
}

void UProjectileAbility::EndAbilityFromActivation(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
