#include "AbilitySystem/Ability/ProjectileAbility.h"

#include "AbilitySystem/Projectiles/ProjectileBase.h"
#include "Character/CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FName ProjectileBarrageTargetSocketName(TEXT("Socket_Target"));
	constexpr float SocketBarrageMinimumInitialReplicationDelay = 0.1f;
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
