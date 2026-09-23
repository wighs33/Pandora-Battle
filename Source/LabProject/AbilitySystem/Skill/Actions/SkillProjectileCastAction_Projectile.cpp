#include "AbilitySystem/Skill/Actions/SkillProjectileCastAction.h"

#include "AbilitySystem/Ability/SkillAbility.h"

#include "AbilitySystem/Projectiles/ProjectileBase.h"
#include "Character/CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

FVector USkillProjectileCastAction::GetSpawnLocation() const
{
    return GetSpawnLocationForSocket(GetConfiguredSpawnSocketName());
}

FVector USkillProjectileCastAction::GetSpawnLocationForSocket(const FName SocketName) const
{
    AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
    if (!AvatarActor)
    {
        return FVector::ZeroVector;
    }

    const ACharacterBase* Character = Cast<ACharacterBase>(AvatarActor);
    const USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;
    if (CharacterMesh && !SocketName.IsNone() && CharacterMesh->DoesSocketExist(SocketName))
    {
        const FTransform SocketTransform = CharacterMesh->GetSocketTransform(SocketName, RTS_World);
        const FVector OffsetLocation = SocketTransform.GetLocation()
            + SocketTransform.TransformVectorNoScale(Settings.SpawnLocationOffset);
        return OffsetLocation;
    }

    FVector SpawnForward = AvatarActor->GetActorForwardVector();
    const FVector HorizontalForward = FVector(SpawnForward.X, SpawnForward.Y, 0.0f).GetSafeNormal();
    if (!HorizontalForward.IsNearlyZero())
    {
        SpawnForward = HorizontalForward;
    }

    const float ForwardSpawnDistance = FMath::Max(
        GetConfiguredMinimumForwardSpawnOffset() + Settings.SpawnLocationOffset.X,
        0.0f);
    const FVector ForwardOffset = SpawnForward * ForwardSpawnDistance;
    const FVector RightOffset = AvatarActor->GetActorRightVector() * Settings.SpawnLocationOffset.Y;
    const FVector UpOffset = AvatarActor->GetActorUpVector() * Settings.SpawnLocationOffset.Z;
    return AvatarActor->GetActorLocation() + ForwardOffset + RightOffset + UpOffset;
}

void USkillProjectileCastAction::ShootProjectile_Implementation(FVector TargetLocation)
{
    AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
    UWorld* World = GetWorld();
    if (!AvatarActor || !AvatarActor->HasAuthority() || !World || !Settings.ProjectileActorClass)
    {
        return;
    }

    const FVector SpawnLocation = GetSpawnLocation();
    if (TargetLocation.IsNearlyZero()
        || FVector::DistSquared(SpawnLocation, TargetLocation) <= UE_KINDA_SMALL_NUMBER)
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
        Projectile = World->SpawnActorDeferred<AProjectileBase>(
            Settings.ProjectileActorClass,
            SpawnTransform,
            AvatarActor,
            Cast<APawn>(AvatarActor),
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

    Projectile->SetImpactAreaDamageRadius(CalculateConfiguredImpactAreaDamageRadius(ChargeDamageAlpha));
    ApplyConfiguredStatusEffect(Projectile);
    ApplyConfiguredProjectileTrajectory(Projectile);

    const FGameplayEffectSpecHandle DamageSpecHandle = MakeDamageEffectSpec(ChargeDamageAlpha);
    if (bUsingReadiedProjectile)
    {
        Projectile->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        Projectile->SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
        Projectile->LaunchProjectile(TargetLocation, GetConfiguredProjectileSpeed(), DamageSpecHandle);
        ReadiedProjectile = nullptr;
    }
    else
    {
        Projectile->InitializeProjectile(TargetLocation, GetConfiguredProjectileSpeed(), DamageSpecHandle);
        UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
        Projectile->ForceNetUpdate();
    }

    bProjectileSpawnSucceeded = true;
}

AProjectileBase* USkillProjectileCastAction::SpawnReadiedProjectile()
{
    AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
    UWorld* World = GetWorld();
    const APawn* AvatarPawn = Cast<APawn>(AvatarActor);
    const bool bAvatarHasAuthority = AvatarActor && AvatarActor->HasAuthority();
    const bool bLocallyControlledAvatar = AvatarPawn && AvatarPawn->IsLocallyControlled();

    if (!AvatarActor
        || (!bAvatarHasAuthority && !bLocallyControlledAvatar)
        || !World
        || !Settings.ProjectileActorClass)
    {
        return nullptr;
    }

    if (IsValid(ReadiedProjectile))
    {
        return ReadiedProjectile.Get();
    }

    const FVector SpawnLocation = GetSpawnLocation();
    FRotator SpawnRotation = AvatarActor->GetActorRotation();
    USkeletalMeshComponent* AttachMesh = nullptr;
    FName AttachSocketName = NAME_None;

    if (const ACharacterBase* Character = Cast<ACharacterBase>(AvatarActor))
    {
        USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
        const FName SpawnSocketName = GetConfiguredSpawnSocketName();
        if (CharacterMesh && !SpawnSocketName.IsNone() && CharacterMesh->DoesSocketExist(SpawnSocketName))
        {
            const FTransform SocketTransform = CharacterMesh->GetSocketTransform(SpawnSocketName, RTS_World);
            SpawnRotation = SocketTransform.GetRotation().Rotator();
            AttachMesh = CharacterMesh;
            AttachSocketName = SpawnSocketName;
        }
    }

    const FTransform SpawnTransform(SpawnRotation, SpawnLocation);
    AProjectileBase* Projectile = World->SpawnActorDeferred<AProjectileBase>(
        Settings.ProjectileActorClass,
        SpawnTransform,
        AvatarActor,
        Cast<APawn>(AvatarActor),
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
        ApplyConfiguredStatusEffect(Projectile);
        Projectile->PrepareProjectile(MakeDamageEffectSpec());
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

void USkillProjectileCastAction::DestroyReadiedProjectile()
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

FGameplayEffectSpecHandle USkillProjectileCastAction::MakeDamageEffectSpec(const float ChargeDamageAlpha) const
{
    const USkillDefinition* Skill = GetAbility()->GetSourceSkillDataAsset();
    if (!Skill)
    {
        return FGameplayEffectSpecHandle();
    }

    const FSkillGameplayEffectConfig DamageConfig = Skill->GetResolvedDamageConfig();
    if (!DamageConfig.GameplayEffectClass)
    {
        return FGameplayEffectSpecHandle();
    }

    const float FullDamage = GetAbility()->CalculateDamageMagnitude(DamageConfig);
    const float DamageAlpha = FMath::Clamp(ChargeDamageAlpha, 0.0f, 1.0f);
    return GetAbility()->MakeConfiguredDamageEffectSpec(DamageConfig, FullDamage * DamageAlpha);
}

FGameplayEffectSpecHandle USkillProjectileCastAction::MakeStatusEffectSpec() const
{
    return GetAbility()->MakeConfiguredStatusEffectSpec(
        GetAbility()->GetSourceSkillDataAsset(),
        GetConfiguredStatusEffectClass(),
        GetConfiguredStatusEffectLevel());
}