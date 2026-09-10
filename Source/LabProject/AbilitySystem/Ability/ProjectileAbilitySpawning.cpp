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
			{},
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
				{},
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
