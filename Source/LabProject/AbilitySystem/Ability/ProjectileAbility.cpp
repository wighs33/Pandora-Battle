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

namespace
{

	const FName ProjectileBarrageTargetSocketName(TEXT("Socket_Target"));
	constexpr float SocketBarrageMinimumInitialReplicationDelay = 0.1f;

	const FSkillProjectileSettings* GetProjectileSettings(const USkillDefinition* SkillDataAsset)
	{
		return SkillDataAsset
			&& SkillDataAsset->SkillDataType == EPdSkillDataType::Projectile
			? &SkillDataAsset->ProjectileSettings
			: nullptr;
	}

	FName GetFirstConfiguredProjectileSocketName(const FSkillProjectileSettings& ProjectileSettings)
	{
		for (const FName& SocketName : ProjectileSettings.ProjectileSocketNames)
		{
			if (!SocketName.IsNone())
			{
				return SocketName;
			}
		}

		return NAME_None;
	}

}

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
	if (SkillDataAsset->SkillDataType != EPdSkillDataType::Projectile)
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

FVector UProjectileAbility::GetSpawnLocation() const
{
	return GetSpawnLocationForSocket(GetConfiguredSpawnSocketName());
}

FVector UProjectileAbility::GetSpawnLocationForSocket(const FName SocketName) const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		return FVector::ZeroVector;
	}

	const ACharacterBase* Character = Cast<ACharacterBase>(AvatarActor);
	const USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;
	if (CharacterMesh && !SocketName.IsNone() && CharacterMesh->DoesSocketExist(SocketName))
	{
		const FTransform SocketTransform = CharacterMesh->GetSocketTransform(SocketName, RTS_World);
		const FVector ConfiguredSpawnLocationOffset = GetConfiguredSpawnLocationOffset();
		const FVector OffsetLocation =
			SocketTransform.GetLocation() + SocketTransform.TransformVectorNoScale(ConfiguredSpawnLocationOffset);

		return OffsetLocation;
	}


	FVector SpawnForward = AvatarActor->GetActorForwardVector();
	const FVector HorizontalForward = FVector(SpawnForward.X, SpawnForward.Y, 0.0f).GetSafeNormal();
	if (!HorizontalForward.IsNearlyZero())
	{
		SpawnForward = HorizontalForward;
	}

	const FVector ConfiguredSpawnLocationOffset = GetConfiguredSpawnLocationOffset();
	const float ForwardSpawnDistance =
		FMath::Max(GetConfiguredMinimumForwardSpawnOffset() + ConfiguredSpawnLocationOffset.X, 0.0f);
	const FVector ForwardOffset = SpawnForward * ForwardSpawnDistance;
	const FVector RightOffset = AvatarActor->GetActorRightVector() * ConfiguredSpawnLocationOffset.Y;
	const FVector UpOffset = AvatarActor->GetActorUpVector() * ConfiguredSpawnLocationOffset.Z;
	const FVector OffsetLocation = AvatarActor->GetActorLocation() + ForwardOffset + RightOffset + UpOffset;

	return OffsetLocation;
}

void UProjectileAbility::ShootProjectile_Implementation(FVector TargetLocation)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	const TSubclassOf<AProjectileBase> ConfiguredProjectileClass = GetConfiguredProjectileClass();
	if (!AvatarActor || !AvatarActor->HasAuthority() || !World || !ConfiguredProjectileClass)
	{
		return;
	}

	const FVector SpawnLocation = GetSpawnLocation();
	if (TargetLocation.IsNearlyZero() || FVector::DistSquared(SpawnLocation, TargetLocation) <= UE_KINDA_SMALL_NUMBER)
	{
		TargetLocation = ResolveDefaultTargetLocation();
	}

	if (TryStartSocketBarrage(TargetLocation))
	{
		bProjectileSpawnSucceeded = true;
		return;
	}

	const FVector SpawnDirection = (TargetLocation - SpawnLocation).GetSafeNormal();
	const FRotator SpawnRotation = SpawnDirection.IsNearlyZero()
		? AvatarActor->GetActorRotation()
		: SpawnDirection.Rotation();
	const FTransform SpawnTransform(SpawnRotation, SpawnLocation);

	AProjectileBase* Projectile = ReadiedProjectile.Get();
	const bool bUsingReadiedProjectile = IsValid(Projectile);
	if (!Projectile)
	{
		APawn* InstigatorPawn = Cast<APawn>(AvatarActor);
		Projectile = World->SpawnActorDeferred<AProjectileBase>(
			ConfiguredProjectileClass,
			SpawnTransform,
			AvatarActor,
			InstigatorPawn,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Projectile)
		{
			return;
		}
		ApplyConfiguredProjectileVisuals(Projectile);
		ApplyConfiguredProjectileImpactPersistence(Projectile);
	}

	const float ChargeDamageAlpha = bUsingReadiedProjectile && IsConfiguredReadiedProjectileChargeGrowthEnabled()
		? Projectile->GetReadiedScaleGrowthAlpha()
		: 1.0f;
	const FGameplayEffectSpecHandle DamageSpecHandle = MakeDamageEffectSpec(ChargeDamageAlpha);
	Projectile->SetImpactAreaDamageRadius(
		CalculateConfiguredImpactAreaDamageRadius(ChargeDamageAlpha));
	ApplyConfiguredStatusEffect(Projectile);


	if (const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset())
	{
		Projectile->SetImpactEffectAreaSpawnConfigs(
			GetSourceProjectileImpactEffectAreas(),
			FMath::Max(GetAbilityLevel(), 1));
	}

	if (bUsingReadiedProjectile)
	{
		Projectile->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Projectile->SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
		ApplyConfiguredProjectileTrajectory(Projectile);
		Projectile->LaunchProjectile(TargetLocation, GetConfiguredProjectileSpeed(), DamageSpecHandle);
		ReadiedProjectile = nullptr;
	}
	else
	{
		ApplyConfiguredProjectileTrajectory(Projectile);
		Projectile->InitializeProjectile(TargetLocation, GetConfiguredProjectileSpeed(), DamageSpecHandle);
	}

	if (!bUsingReadiedProjectile)
	{
		UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
		Projectile->ForceNetUpdate();
	}

	bProjectileSpawnSucceeded = true;
}

AProjectileBase* UProjectileAbility::SpawnReadiedProjectile()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	const TSubclassOf<AProjectileBase> ConfiguredProjectileClass = GetConfiguredProjectileClass();
	const APawn* AvatarPawn = Cast<APawn>(AvatarActor);
	const bool bAvatarHasAuthority = AvatarActor && AvatarActor->HasAuthority();
	const bool bLocallyControlledAvatar = AvatarPawn && AvatarPawn->IsLocallyControlled();
	if (!AvatarActor || (!bAvatarHasAuthority && !bLocallyControlledAvatar) || !World || !ConfiguredProjectileClass)
	{
		return nullptr;
	}

	if (IsValid(ReadiedProjectile))
	{
		return ReadiedProjectile.Get();
	}

	FVector SpawnLocation = GetSpawnLocation();
	FRotator SpawnRotation = AvatarActor->GetActorRotation();
	USkeletalMeshComponent* AttachMesh = nullptr;
	FName AttachSocketName = NAME_None;
	if (const ACharacterBase* Character = Cast<ACharacterBase>(AvatarActor))
	{
		USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
		const FName ConfiguredSpawnSocketName = GetConfiguredSpawnSocketName();
		if (CharacterMesh && !ConfiguredSpawnSocketName.IsNone() && CharacterMesh->DoesSocketExist(ConfiguredSpawnSocketName))
		{
			const FTransform SocketTransform = CharacterMesh->GetSocketTransform(ConfiguredSpawnSocketName, RTS_World);
			SpawnRotation = SocketTransform.GetRotation().Rotator();
			AttachMesh = CharacterMesh;
			AttachSocketName = ConfiguredSpawnSocketName;
		}
	}
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
		return nullptr;
	}

	const bool bCosmeticReadiedProjectile = !bAvatarHasAuthority;
	if (bCosmeticReadiedProjectile)
	{
		Projectile->SetReplicates(false);
		Projectile->SetReplicateMovement(false);
	}

	ApplyConfiguredProjectileVisuals(Projectile);
	ApplyConfiguredProjectileImpactPersistence(Projectile);

	if (bCosmeticReadiedProjectile)
	{
		Projectile->PrepareCosmeticReadiedProjectile();
	}
	else
	{
		const FGameplayEffectSpecHandle DamageSpecHandle = MakeDamageEffectSpec();
		ApplyConfiguredStatusEffect(Projectile);
		Projectile->PrepareProjectile(DamageSpecHandle);
		if (const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset())
		{
			Projectile->SetImpactEffectAreaSpawnConfigs(
				GetSourceProjectileImpactEffectAreas(),
				FMath::Max(GetAbilityLevel(), 1));
		}
	}
	UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);

	if (AttachMesh && !AttachSocketName.IsNone())
	{
		Projectile->AttachToComponent(AttachMesh, FAttachmentTransformRules::KeepWorldTransform, AttachSocketName);

	}

	ApplyReadiedProjectileScaleGrowth(Projectile);

	ReadiedProjectile = Projectile;
	if (!bCosmeticReadiedProjectile)
	{
		Projectile->ForceNetUpdate();
	}
	return Projectile;
}

void UProjectileAbility::DestroyReadiedProjectile()
{
	AProjectileBase* Projectile = ReadiedProjectile.Get();
	ReadiedProjectile = nullptr;
	if (!IsValid(Projectile))
	{
		return;
	}

	if (Projectile->HasAuthority() || !Projectile->GetIsReplicated())
	{
		Projectile->Destroy();
	}
}

bool UProjectileAbility::TryStartSocketBarrage(const FVector TargetLocation)
{
	TArray<FName> SocketNames = GetConfiguredProjectileSocketNames();
	if (SocketNames.Num() <= 1)
	{
		return false;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor || !AvatarActor->HasAuthority())
	{
		return false;
	}

	DestroyReadiedProjectile();
	ClearSocketBarrageState(true);

	SocketBarrageSocketNames = MoveTemp(SocketNames);
	SocketBarrageTargetLocation = TargetLocation;
	bSocketBarrageUseCharacterTargetSocket = true;
	NextSocketBarrageProjectileIndex = 0;
	bSocketBarrageEndAbilityAfterFire = bEndAfterProjectileFired;

	SocketBarrageProjectiles.Reserve(SocketBarrageSocketNames.Num());
	for (const FName SocketName : SocketBarrageSocketNames)
	{
		SocketBarrageProjectiles.Add(SpawnPreparedSocketBarrageProjectile(SocketName));
	}

	int32 PreparedProjectileCount = 0;
	for (const TObjectPtr<AProjectileBase>& Projectile : SocketBarrageProjectiles)
	{
		if (IsValid(Projectile.Get()))
		{
			++PreparedProjectileCount;
		}
	}
	if (PreparedProjectileCount <= 0)
	{
		ClearSocketBarrageState(true);
		return false;
	}

	AvatarActor->ForceNetUpdate();

	const float ConfiguredFireInterval = GetConfiguredProjectileSocketFireInterval();
	const float InitialFireDelay = FMath::Max(
		ConfiguredFireInterval,
		SocketBarrageMinimumInitialReplicationDelay);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SocketBarrageTimerHandle,
			this,
			&ThisClass::FireNextSocketBarrageProjectile,
			InitialFireDelay,
			false);
	}
	else
	{
		FireNextSocketBarrageProjectile();
	}
	return true;
}

AProjectileBase* UProjectileAbility::SpawnPreparedSocketBarrageProjectile(const FName SocketName)
{
	if (!SocketBarrageSocketNames.Contains(SocketName) && !SocketName.IsNone())
	{
		return nullptr;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	const TSubclassOf<AProjectileBase> ConfiguredProjectileClass = GetConfiguredProjectileClass();
	if (!AvatarActor || !AvatarActor->HasAuthority() || !World || !ConfiguredProjectileClass)
	{
		return nullptr;
	}

	const FVector SpawnLocation = GetSpawnLocationForSocket(SocketName);
	FRotator SpawnRotation = AvatarActor->GetActorRotation();
	USkeletalMeshComponent* AttachMesh = nullptr;
	FName AttachSocketName = NAME_None;
	if (const ACharacterBase* Character = Cast<ACharacterBase>(AvatarActor))
	{
		USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
		if (CharacterMesh && !SocketName.IsNone() && CharacterMesh->DoesSocketExist(SocketName))
		{
			const FTransform SocketTransform = CharacterMesh->GetSocketTransform(SocketName, RTS_World);
			SpawnRotation = SocketTransform.GetRotation().Rotator();
			AttachMesh = CharacterMesh;
			AttachSocketName = SocketName;
		}
	}

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

		return nullptr;
	}

	Projectile->SetReplicates(true);
	Projectile->SetReplicateMovement(true);
	ApplyConfiguredProjectileVisuals(Projectile);
	ApplyConfiguredProjectileTrajectory(Projectile);
	ApplyConfiguredProjectileImpactPersistence(Projectile);
	ApplyConfiguredStatusEffect(Projectile);
	Projectile->SetImpactEffectAreaSpawnConfigs(
		GetSourceProjectileImpactEffectAreas(),
		FMath::Max(GetAbilityLevel(), 1));
	Projectile->PrepareProjectile(MakeDamageEffectSpec());
	UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);

	if (AttachMesh && !AttachSocketName.IsNone())
	{
		Projectile->AttachToComponent(AttachMesh, FAttachmentTransformRules::KeepWorldTransform, AttachSocketName);
	}

	ApplyReadiedProjectileScaleGrowth(Projectile);
	Projectile->FlushNetDormancy();
	Projectile->ForceNetUpdate();

	return Projectile;
}

void UProjectileAbility::LaunchSocketBarrageProjectile(AProjectileBase* Projectile)
{
	if (!IsValid(Projectile))
	{
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor || !AvatarActor->HasAuthority())
	{
		return;
	}

	Projectile->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	const FVector SpawnLocation = Projectile->GetActorLocation();
	const FVector LaunchTargetLocation = ResolveSocketBarrageLaunchTargetLocation(SpawnLocation);
	const FVector SpawnDirection = (LaunchTargetLocation - SpawnLocation).GetSafeNormal();
	const FRotator SpawnRotation = SpawnDirection.IsNearlyZero()
		? AvatarActor->GetActorRotation()
		: SpawnDirection.Rotation();
	Projectile->SetActorRotation(SpawnRotation, ETeleportType::TeleportPhysics);

	const float ChargeDamageAlpha = IsConfiguredReadiedProjectileChargeGrowthEnabled()
		? Projectile->GetReadiedScaleGrowthAlpha()
		: 1.0f;
	Projectile->SetImpactAreaDamageRadius(
		CalculateConfiguredImpactAreaDamageRadius(ChargeDamageAlpha));
	ApplyConfiguredProjectileTrajectory(Projectile);
	Projectile->LaunchProjectile(
		LaunchTargetLocation,
		GetConfiguredProjectileSpeed(),
		MakeDamageEffectSpec(ChargeDamageAlpha));
	Projectile->ForceNetUpdate();
}

void UProjectileAbility::SpawnSocketBarrageProjectileCosmetic(
	TSubclassOf<AProjectileBase> ProjectileClass,
	const FVector& SpawnLocation,
	const FRotator& SpawnRotation,
	const FVector& TargetLocation,
	const float ProjectileSpeed,
	const int32 SocketIndex) const
{
	ACharacterBase* Character = Cast<ACharacterBase>(GetAvatarActorFromActorInfo());
	if (!Character || !Character->HasAuthority() || !ProjectileClass)
	{
		return;
	}

	UNiagaraSystem* MuzzleFX = nullptr;
	UNiagaraSystem* ProjectileFX = nullptr;
	UNiagaraSystem* HitFX = nullptr;
	bool bSpawnHitNiagaraOnGround = false;
	FGameplayTag SpawnGameplayCueTag;
	FGameplayTag ImpactGameplayCueTag;
	GetConfiguredProjectileVisuals(
		MuzzleFX,
		ProjectileFX,
		HitFX,
		bSpawnHitNiagaraOnGround,
		SpawnGameplayCueTag,
		ImpactGameplayCueTag);

	FVector SpawnScale = FVector::OneVector;
	FName NiagaraVector2DParameterName = NAME_None;
	FVector2D NiagaraSize = FVector2D::UnitVector;
	ResolveSocketBarrageProjectileLaunchScale(SocketIndex, SpawnScale, NiagaraVector2DParameterName, NiagaraSize);

	Character->MulticastSpawnProjectileCosmetic(
		ProjectileClass,
		FVector_NetQuantize(SpawnLocation),
		SpawnRotation,
		FVector_NetQuantize(TargetLocation),
		ProjectileSpeed,
		ShouldUseConfiguredProjectileArcTrajectory(),
		GetConfiguredProjectileArcHeight(),
		GetConfiguredProjectileArcGravityScale(),
		MuzzleFX,
		ProjectileFX,
		HitFX,
		bSpawnHitNiagaraOnGround,
		SpawnGameplayCueTag,
		ImpactGameplayCueTag,
		SpawnScale,
		NiagaraVector2DParameterName,
		NiagaraSize,
		ResolveSocketBarrageCosmeticLifeSpan(SpawnLocation, TargetLocation, ProjectileSpeed));
}

void UProjectileAbility::ApplySocketBarrageProjectileLaunchScale(
	AProjectileBase* Projectile,
	const int32 SocketIndex) const
{
	if (!IsValid(Projectile))
	{
		return;
	}

	FVector Scale = FVector::OneVector;
	FName NiagaraVector2DParameterName = NAME_None;
	FVector2D NiagaraSize = FVector2D::UnitVector;
	ResolveSocketBarrageProjectileLaunchScale(SocketIndex, Scale, NiagaraVector2DParameterName, NiagaraSize);

	if (Scale.Equals(FVector::OneVector)
		&& NiagaraVector2DParameterName.IsNone()
		&& NiagaraSize.Equals(FVector2D::UnitVector))
	{
		return;
	}

	Projectile->StartReadiedScaleGrowth(
		Scale,
		Scale,
		0.0f,
		NiagaraVector2DParameterName,
		NiagaraSize,
		NiagaraSize);
}

void UProjectileAbility::ResolveSocketBarrageProjectileLaunchScale(
	const int32 SocketIndex,
	FVector& OutScale,
	FName& OutNiagaraVector2DParameterName,
	FVector2D& OutNiagaraSize) const
{
	OutScale = FVector::OneVector;
	OutNiagaraVector2DParameterName = NAME_None;
	OutNiagaraSize = FVector2D::UnitVector;

	if (!IsConfiguredReadiedProjectileChargeGrowthEnabled())
	{
		return;
	}

	const float ScaleDuration = GetConfiguredReadiedProjectileScaleDuration();
	const float ElapsedBeforeFire = FMath::Max(GetConfiguredProjectileSocketFireInterval(), 0.0f)
		* static_cast<float>(FMath::Max(SocketIndex, 0));
	const float GrowthAlpha = ScaleDuration > UE_SMALL_NUMBER
		? FMath::Clamp(ElapsedBeforeFire / ScaleDuration, 0.0f, 1.0f)
		: 1.0f;

	OutScale = FMath::Lerp(
		GetConfiguredReadiedProjectileStartScale(),
		GetConfiguredReadiedProjectileTargetScale(),
		GrowthAlpha);
	OutNiagaraSize = FMath::Lerp(
		GetConfiguredReadiedProjectileNiagaraStartSize(),
		GetConfiguredReadiedProjectileNiagaraTargetSize(),
		GrowthAlpha);
	OutNiagaraVector2DParameterName = GetConfiguredReadiedProjectileNiagaraVector2DParameterName();
}

float UProjectileAbility::ResolveSocketBarrageCosmeticLifeSpan(
	const FVector& SpawnLocation,
	const FVector& TargetLocation,
	const float ProjectileSpeed) const
{
	if (ProjectileSpeed <= UE_SMALL_NUMBER)
	{
		return 1.0f;
	}

	const float TravelTime = FVector::Dist(SpawnLocation, TargetLocation) / ProjectileSpeed;
	return FMath::Clamp(TravelTime + 0.35f, 0.35f, 3.0f);
}

FVector UProjectileAbility::ResolveSocketBarrageLaunchTargetLocation(const FVector& ProjectileLocation) const
{
	if (bSocketBarrageUseCharacterTargetSocket)
	{
		return ResolveCharacterTargetSocketProjectileTargetLocation(ProjectileLocation);
	}

	FVector LaunchTargetLocation = SocketBarrageTargetLocation;
	if (LaunchTargetLocation.IsNearlyZero()
		|| FVector::DistSquared(ProjectileLocation, LaunchTargetLocation) <= UE_KINDA_SMALL_NUMBER)
	{
		LaunchTargetLocation = ResolveDefaultTargetLocation();
	}

	return LaunchTargetLocation;
}

FVector UProjectileAbility::ResolveCharacterTargetSocketProjectileTargetLocation(const FVector& FromLocation) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const float LaunchRange = FMath::Max(GetConfiguredTargetTraceMaxRange(), 1000.0f);
	if (!AvatarActor)
	{
		return FromLocation + FVector::ForwardVector * LaunchRange;
	}

	if (const ACharacterBase* Character = Cast<ACharacterBase>(AvatarActor))
	{
		if (const USkeletalMeshComponent* CharacterMesh = Character->GetMesh())
		{
			if (CharacterMesh->DoesSocketExist(ProjectileBarrageTargetSocketName))
			{
				const FVector SocketTargetLocation = CharacterMesh->GetSocketLocation(ProjectileBarrageTargetSocketName);
				if (FVector::DistSquared(FromLocation, SocketTargetLocation) > UE_KINDA_SMALL_NUMBER)
				{
					return SocketTargetLocation;
				}
			}
		}
	}

	FVector ForwardDirection = AvatarActor->GetActorForwardVector().GetSafeNormal();
	if (ForwardDirection.IsNearlyZero())
	{
		ForwardDirection = FVector::ForwardVector;
	}
	return FromLocation + ForwardDirection * LaunchRange;
}

void UProjectileAbility::FireNextSocketBarrageProjectile()
{
	if (!SocketBarrageSocketNames.IsValidIndex(NextSocketBarrageProjectileIndex))
	{
		const bool bShouldEndAbility = bSocketBarrageEndAbilityAfterFire;
		ClearSocketBarrageState(false);
		if (bShouldEndAbility)
		{
			EndProjectileAbilityAfterResolvedShot();
		}
		return;
	}

	AProjectileBase* Projectile = SocketBarrageProjectiles.IsValidIndex(NextSocketBarrageProjectileIndex)
		? SocketBarrageProjectiles[NextSocketBarrageProjectileIndex].Get()
		: nullptr;
	LaunchSocketBarrageProjectile(Projectile);
	if (SocketBarrageProjectiles.IsValidIndex(NextSocketBarrageProjectileIndex))
	{
		SocketBarrageProjectiles[NextSocketBarrageProjectileIndex] = nullptr;
	}

	++NextSocketBarrageProjectileIndex;
	if (!SocketBarrageSocketNames.IsValidIndex(NextSocketBarrageProjectileIndex))
	{
		const bool bShouldEndAbility = bSocketBarrageEndAbilityAfterFire;
		ClearSocketBarrageState(false);
		if (bShouldEndAbility)
		{
			EndProjectileAbilityAfterResolvedShot();
		}
		return;
	}

	const float FireInterval = GetConfiguredProjectileSocketFireInterval();
	if (FireInterval <= KINDA_SMALL_NUMBER)
	{
		FireNextSocketBarrageProjectile();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SocketBarrageTimerHandle,
			this,
			&ThisClass::FireNextSocketBarrageProjectile,
			FireInterval,
			false);
	}
}

void UProjectileAbility::ClearSocketBarrageState(const bool bDestroyPendingProjectiles)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SocketBarrageTimerHandle);
	}
	SocketBarrageTimerHandle.Invalidate();

	if (bDestroyPendingProjectiles)
	{
		for (AProjectileBase* Projectile : SocketBarrageProjectiles)
		{
			if (IsValid(Projectile) && Projectile->HasAuthority())
			{
				Projectile->Destroy();
			}
		}
	}

	SocketBarrageSocketNames.Reset();
	SocketBarrageProjectiles.Reset();
	SocketBarrageTargetLocation = FVector::ZeroVector;
	bSocketBarrageUseCharacterTargetSocket = false;
	NextSocketBarrageProjectileIndex = 0;
	bSocketBarrageEndAbilityAfterFire = false;
}

bool UProjectileAbility::IsSocketBarrageActive() const
{
	return !SocketBarrageSocketNames.IsEmpty();
}

bool UProjectileAbility::ShouldWaitForServerSocketBarrageEnd() const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	return AvatarActor
		&& !AvatarActor->HasAuthority()
		&& GetConfiguredProjectileSocketNames().Num() > 1;
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

	if (bTooClose || bStronglyDownward)
	{

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

UAnimMontage* UProjectileAbility::GetConfiguredShootMontage() const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	return GetProjectileSettings(SkillDataAsset) && SkillDataAsset->Animation.PrimaryMontage
		? SkillDataAsset->Animation.PrimaryMontage.Get()
		: nullptr;
}

TSubclassOf<AProjectileBase> UProjectileAbility::GetConfiguredProjectileClass() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings ? ProjectileSettings->ProjectileActorClass : nullptr;
}

TSubclassOf<UGameplayEffect> UProjectileAbility::GetConfiguredDamageEffectClass() const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->GetResolvedDamageConfig().GameplayEffectClass : nullptr;
}

const UStatusEffectDefinition* UProjectileAbility::GetConfiguredStatusEffectDataAsset() const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->StatusEffectDataAsset.Get() : nullptr;
}

TSubclassOf<UGameplayEffect> UProjectileAbility::GetConfiguredStatusEffectClass() const
{
	if (const UStatusEffectDefinition* StatusEffectDataAsset = GetConfiguredStatusEffectDataAsset())
	{
		if (StatusEffectDataAsset->StatusEffectClass)
		{
			return StatusEffectDataAsset->StatusEffectClass;
		}
	}

	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings ? ProjectileSettings->StatusEffectClass : nullptr;
}

float UProjectileAbility::GetConfiguredStatusEffectLevel() const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	if (SkillDataAsset && SkillDataAsset->StatusEffectDataAsset)
	{
		return FMath::Max(SkillDataAsset->StatusEffectLevel, 1.0f);
	}

	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings ? FMath::Max(ProjectileSettings->StatusEffectLevel, 1.0f) : 1.0f;
}

float UProjectileAbility::GetConfiguredStatusEffectDuration() const
{
	const UStatusEffectDefinition* StatusEffectDataAsset = GetConfiguredStatusEffectDataAsset();
	return StatusEffectDataAsset ? FMath::Max(StatusEffectDataAsset->StatusDuration, 0.0f) : 0.0f;
}

float UProjectileAbility::GetConfiguredProjectileSpeed() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->ProjectileSpeed, 0.0))
		: 0.0f;
}

float UProjectileAbility::GetConfiguredProjectileRadius() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->ProjectileRadius, 0.0))
		: 0.0f;
}

bool UProjectileAbility::ShouldUseConfiguredProjectileArcTrajectory() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings && ProjectileSettings->bUseArcTrajectory;
}

float UProjectileAbility::GetConfiguredProjectileArcHeight() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->ProjectileArcHeight, 0.0))
		: 0.0f;
}

float UProjectileAbility::GetConfiguredProjectileArcGravityScale() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->ProjectileArcGravityScale, 0.0))
		: 1.0f;
}

FGameplayTag UProjectileAbility::GetConfiguredDamageDataTag() const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{
		return FGameplayTag();
	}

	return SkillDataAsset->GetResolvedDamageConfig().MagnitudeDataTag;
}

FGameplayTag UProjectileAbility::GetConfiguredShootProjectileEventTag() const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(SkillDataAsset);
	if (!ProjectileSettings)
	{
		return FGameplayTag();
	}

	return ProjectileSettings->FireEventTag.IsValid()
		? ProjectileSettings->FireEventTag
		: SkillDataAsset->Animation.PrimaryEventTag;
}

bool UProjectileAbility::IsConfiguredImmediateFireMode() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings && ProjectileSettings->FireMode == EPdSkillProjectileFireMode::Immediate;
}

TArray<FName> UProjectileAbility::GetConfiguredProjectileSocketNames() const
{
	TArray<FName> SocketNames;
	if (const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset()))
	{
		for (const FName& SocketName : ProjectileSettings->ProjectileSocketNames)
		{
			if (!SocketName.IsNone())
			{
				SocketNames.Add(SocketName);
			}
		}
	}

	return SocketNames;
}

float UProjectileAbility::GetConfiguredProjectileSocketFireInterval() const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	if (const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(SkillDataAsset))
	{
		return static_cast<float>(FMath::Max(ProjectileSettings->ProjectileSocketFireInterval, 0.0));
	}

	return 0.0f;
}

void UProjectileAbility::GetConfiguredProjectileVisuals(
	UNiagaraSystem*& OutMuzzleFX,
	UNiagaraSystem*& OutProjectileFX,
	UNiagaraSystem*& OutHitFX,
	bool& bOutSpawnHitNiagaraOnGround,
	FGameplayTag& OutSpawnGameplayCueTag,
	FGameplayTag& OutImpactGameplayCueTag) const
{
	OutMuzzleFX = nullptr;
	OutProjectileFX = nullptr;
	OutHitFX = nullptr;
	bOutSpawnHitNiagaraOnGround = false;
	OutSpawnGameplayCueTag = FGameplayTag();
	OutImpactGameplayCueTag = FGameplayTag();

	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	if (const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(SkillDataAsset))
	{
		OutMuzzleFX = ProjectileSettings->MuzzleNiagaraSystem.Get();
		OutProjectileFX = ProjectileSettings->ProjectileNiagaraSystem.Get();
		OutHitFX = ProjectileSettings->HitNiagaraSystem.Get();
		bOutSpawnHitNiagaraOnGround = ProjectileSettings->bSpawnHitNiagaraOnGround;
	}
}

void UProjectileAbility::ApplyConfiguredProjectileVisuals(AProjectileBase* Projectile) const
{
	if (!IsValid(Projectile))
	{
		return;
	}

	UNiagaraSystem* MuzzleFX = nullptr;
	UNiagaraSystem* ProjectileFX = nullptr;
	UNiagaraSystem* HitFX = nullptr;
	bool bSpawnHitNiagaraOnGround = false;
	FGameplayTag SpawnGameplayCueTag;
	FGameplayTag ImpactGameplayCueTag;
	GetConfiguredProjectileVisuals(
		MuzzleFX,
		ProjectileFX,
		HitFX,
		bSpawnHitNiagaraOnGround,
		SpawnGameplayCueTag,
		ImpactGameplayCueTag);

	Projectile->ConfigureProjectileVisuals(
		MuzzleFX,
		ProjectileFX,
		HitFX,
		bSpawnHitNiagaraOnGround,
		SpawnGameplayCueTag,
		ImpactGameplayCueTag);
}

void UProjectileAbility::ApplyConfiguredProjectileTrajectory(AProjectileBase* Projectile) const
{
	if (!IsValid(Projectile))
	{
		return;
	}

	Projectile->ConfigureArcTrajectory(
		ShouldUseConfiguredProjectileArcTrajectory(),
		GetConfiguredProjectileArcHeight(),
		GetConfiguredProjectileArcGravityScale());
}

void UProjectileAbility::ApplyConfiguredProjectileImpactPersistence(AProjectileBase* Projectile) const
{
	if (!IsValid(Projectile))
	{
		return;
	}

	const FSkillProjectileSettings* ProjectileSettings =
		GetProjectileSettings(GetSourceSkillDataAsset());
	Projectile->ConfigureImpactPersistence(
		ProjectileSettings && ProjectileSettings->bStickOnImpact,
		ProjectileSettings
			? static_cast<float>(FMath::Max(ProjectileSettings->PostImpactLifeSpan, 0.0))
			: 0.0f);
}

float UProjectileAbility::GetConfiguredTargetTraceMaxRange() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->TargetTraceMaxRange, 0.0))
		: 0.0f;
}

FCollisionProfileName UProjectileAbility::GetConfiguredTargetTraceProfile() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings
		? ProjectileSettings->TargetTraceProfile
		: FCollisionProfileName(TEXT("NoCollision"));
}

float UProjectileAbility::GetConfiguredMinimumTargetDistanceFromSpawn() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->MinimumTargetDistanceFromSpawn, 0.0))
		: 0.0f;
}

bool UProjectileAbility::GetConfiguredTraceAffectsAimPitch() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings && ProjectileSettings->bTraceAffectsAimPitch;
}

bool UProjectileAbility::GetConfiguredDrawTargetTraceDebug() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return LabSkillDebug::IsDrawingEnabled()
		&& ProjectileSettings
		&& ProjectileSettings->bDrawTargetTraceDebug;
}

FName UProjectileAbility::GetConfiguredSpawnSocketName() const
{
	if (const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset()))
	{
		return GetFirstConfiguredProjectileSocketName(*ProjectileSettings);
	}

	return NAME_None;
}

FVector UProjectileAbility::GetConfiguredSpawnLocationOffset() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings ? ProjectileSettings->SpawnLocationOffset : FVector::ZeroVector;
}

float UProjectileAbility::GetConfiguredMinimumForwardSpawnOffset() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->MinimumForwardSpawnOffset, 0.0))
		: 0.0f;
}

bool UProjectileAbility::IsConfiguredReadiedProjectileChargeGrowthEnabled() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings
		&& (ProjectileSettings->bGrowProjectileSize
			|| ProjectileSettings->FireMode == EPdSkillProjectileFireMode::HoldThenConfirm);
}

FVector UProjectileAbility::GetConfiguredReadiedProjectileStartScale() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings ? ProjectileSettings->ProjectileStartScale : FVector::OneVector;
}

FVector UProjectileAbility::GetConfiguredReadiedProjectileTargetScale() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings ? ProjectileSettings->ProjectileFinalScale : FVector::OneVector;
}

float UProjectileAbility::GetConfiguredReadiedProjectileScaleDuration() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->ProjectileScaleDuration, 0.0))
		: 0.0f;
}

FName UProjectileAbility::GetConfiguredReadiedProjectileNiagaraVector2DParameterName() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings ? ProjectileSettings->GrowthUserParameterName : NAME_None;
}

FVector2D UProjectileAbility::GetConfiguredReadiedProjectileNiagaraStartSize() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings ? ProjectileSettings->GrowthUserParameterStartValue : FVector2D::UnitVector;
}

FVector2D UProjectileAbility::GetConfiguredReadiedProjectileNiagaraTargetSize() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings ? ProjectileSettings->GrowthUserParameterFinalValue : FVector2D::UnitVector;
}

void UProjectileAbility::ApplyConfiguredStatusEffect(AProjectileBase* Projectile) const
{
	if (!IsValid(Projectile))
	{
		return;
	}

	Projectile->SetDebuffEffectSpecHandle(MakeStatusEffectSpec());
}

void UProjectileAbility::ApplyReadiedProjectileScaleGrowth(AProjectileBase* Projectile) const
{
	if (!IsValid(Projectile))
	{
		return;
	}

	if (!IsConfiguredReadiedProjectileChargeGrowthEnabled())
	{

		return;
	}

	const float ScaleDuration = GetConfiguredReadiedProjectileScaleDuration();
	const FVector StartScale = GetConfiguredReadiedProjectileStartScale();
	const FVector TargetScale = GetConfiguredReadiedProjectileTargetScale();
	const FName NiagaraVector2DParameterName = GetConfiguredReadiedProjectileNiagaraVector2DParameterName();
	const FVector2D NiagaraStartSize = GetConfiguredReadiedProjectileNiagaraStartSize();
	const FVector2D NiagaraTargetSize = GetConfiguredReadiedProjectileNiagaraTargetSize();
	const bool bHasActorScaleGrowth = !StartScale.Equals(TargetScale);
	const bool bHasNiagaraSizeGrowth = !NiagaraVector2DParameterName.IsNone() && !NiagaraStartSize.Equals(NiagaraTargetSize);
	if (ScaleDuration <= 0.0f || (!bHasActorScaleGrowth && !bHasNiagaraSizeGrowth))
	{
		return;
	}

	Projectile->StartReadiedScaleGrowth(
		StartScale,
		TargetScale,
		ScaleDuration,
		NiagaraVector2DParameterName,
		NiagaraStartSize,
		NiagaraTargetSize);

}

bool UProjectileAbility::ShouldUseGroundTargeting() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings && ProjectileSettings->bUseGroundTargeting;
}

TSubclassOf<AGameplayAbilityTargetActor> UProjectileAbility::GetConfiguredGroundTargetActorClass() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	const TSubclassOf<AGameplayAbilityTargetActor> ConfiguredClass =
		ProjectileSettings ? ProjectileSettings->GroundTargetActorClass : nullptr;
	if (ConfiguredClass
		&& ConfiguredClass->IsChildOf(AGameplayAbilityTargetActor_GroundTrace::StaticClass())
		&& !ConfiguredClass->IsChildOf(ATargetActor_GroundTrace_Decal::StaticClass()))
	{
		return ATargetActor_GroundTrace_Decal::StaticClass();
	}

	return ConfiguredClass;
}

float UProjectileAbility::GetConfiguredGroundTargetingMaxRange() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->GroundTargetingMaxRange, 0.0))
		: 0.0f;
}

FCollisionProfileName UProjectileAbility::GetConfiguredGroundTargetingTraceProfile() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings
		? ProjectileSettings->GroundTargetingTraceProfile
		: FCollisionProfileName(TEXT("BlockAll"));
}

float UProjectileAbility::GetConfiguredGroundTargetingTraceStartHeight() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->TargetGroundTraceStartHeight, 0.0))
		: 0.0f;
}

float UProjectileAbility::GetConfiguredGroundTargetingTraceDepth() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->TargetGroundTraceDepth, 0.0))
		: 0.0f;
}

float UProjectileAbility::GetConfiguredGroundTargetingCollisionRadius() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->GroundTargetingCollisionRadius, 0.0))
		: 0.0f;
}

float UProjectileAbility::GetConfiguredGroundTargetingCollisionHeight() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->GroundTargetingCollisionHeight, 0.0))
		: 0.0f;
}

bool UProjectileAbility::GetConfiguredGroundTargetingTraceAffectsAimPitch() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings && ProjectileSettings->bGroundTargetingTraceAffectsAimPitch;
}

bool UProjectileAbility::GetConfiguredDrawGroundTargetingDebug() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return LabSkillDebug::IsDrawingEnabled()
		&& ProjectileSettings
		&& ProjectileSettings->bDrawGroundTargetingDebug;
}

UMaterialInterface* UProjectileAbility::GetConfiguredGroundTargetingDecal() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings ? ProjectileSettings->TargetDecal.Get() : nullptr;
}

float UProjectileAbility::GetConfiguredGroundTargetingDecalSize() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->TargetDecalSize, 0.0))
		: 0.0f;
}

float UProjectileAbility::GetConfiguredGroundTargetingDecalFinalSize() const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	if (const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(SkillDataAsset))
	{
		return ProjectileSettings->TargetDecalFinalSize > 0.0
			? static_cast<float>(ProjectileSettings->TargetDecalFinalSize)
			: GetConfiguredGroundTargetingDecalSize();
	}

	return GetConfiguredGroundTargetingDecalSize();
}

bool UProjectileAbility::ShouldGrowConfiguredGroundTargetingDecal() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings && ProjectileSettings->bGrowTargetDecalSize;
}

FLinearColor UProjectileAbility::GetConfiguredGroundTargetingDecalColor() const
{
	const FSkillProjectileSettings* ProjectileSettings = GetProjectileSettings(GetSourceSkillDataAsset());
	return ProjectileSettings ? ProjectileSettings->TargetDecalColor : FLinearColor::White;
}

float UProjectileAbility::CalculateConfiguredImpactAreaDamageRadius(
	const float ChargeDamageAlpha) const
{
	if (!ShouldUseGroundTargeting() || !GetConfiguredGroundTargetingDecal())
	{
		return 0.0f;
	}

	const float StartDiameter = FMath::Max(GetConfiguredGroundTargetingDecalSize(), 0.0f);
	const float FinalDiameter = FMath::Max(GetConfiguredGroundTargetingDecalFinalSize(), 0.0f);
	const float DamageDiameter = ShouldGrowConfiguredGroundTargetingDecal()
		? FMath::Lerp(StartDiameter, FinalDiameter, FMath::Clamp(ChargeDamageAlpha, 0.0f, 1.0f))
		: StartDiameter;

	// Targeting decal sizes are configured as diameters, matching the AOE decal convention.
	return DamageDiameter * 0.5f;
}

bool UProjectileAbility::TryBuildGroundTargetingDecalGrowth(
	float& OutStartSize,
	float& OutTargetSize,
	float& OutDuration) const
{
	OutStartSize = GetConfiguredGroundTargetingDecalSize();
	OutTargetSize = GetConfiguredGroundTargetingDecalFinalSize();
	OutDuration = 0.0f;

	if (!ShouldUseGroundTargeting()
		|| !ShouldGrowConfiguredGroundTargetingDecal()
		|| OutStartSize <= 0.0f
		|| OutTargetSize <= 0.0f)
	{
		return false;
	}

	OutDuration = GetConfiguredReadiedProjectileScaleDuration();
	if (OutDuration <= UE_SMALL_NUMBER)
	{
		return false;
	}

	return !FMath::IsNearlyEqual(OutStartSize, OutTargetSize);
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
