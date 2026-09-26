#include "Skill/Actions/SkillProjectileCastAction.h"

#include "AbilitySystem/Ability/SkillAbility.h"

#include "Abilities/GameplayAbilityTargetActor_GroundTrace.h"
#include "Definition/AbilitySystem/StatusEffectDefinition.h"
#include "Skill/Actors/SkillProjectile.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "AbilitySystem/TargetingActors/GroundTargetActor.h"
#include "Materials/MaterialInterface.h"

// 스킬 공통 데이터와 액션 설정의 우선순위 및 입력값 보정을 처리한다.
UAnimMontage* USkillProjectileCastAction::GetConfiguredShootMontage() const
{
    const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
    return SkillDataAsset ? SkillDataAsset->Animation.PrimaryMontage.Get() : nullptr;
}

UStatusEffectDefinition* USkillProjectileCastAction::GetConfiguredStatusEffectDataAsset() const
{
    const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
    return SkillDataAsset ? SkillDataAsset->StatusEffectDataAsset.Get() : nullptr;
}

TSubclassOf<UGameplayEffect> USkillProjectileCastAction::GetConfiguredStatusEffectClass() const
{
    if (const UStatusEffectDefinition* StatusEffectDataAsset = GetConfiguredStatusEffectDataAsset())
    {
       if (StatusEffectDataAsset->StackGameplayEffectClass)
       {
          return StatusEffectDataAsset->StackGameplayEffectClass;
       }
    }
    return Settings.StatusEffectClass;
}

float USkillProjectileCastAction::GetConfiguredStatusEffectLevel() const
{
    const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
    return FMath::Max(SkillDataAsset && SkillDataAsset->StatusEffectDataAsset
       ? SkillDataAsset->StatusEffectLevel : Settings.StatusEffectLevel, 1.0f);
}

float USkillProjectileCastAction::GetConfiguredProjectileSpeed() const
{
    return static_cast<float>(FMath::Max(Settings.ProjectileSpeed, 0.0));
}

float USkillProjectileCastAction::GetConfiguredProjectileRadius() const
{
    return static_cast<float>(FMath::Max(Settings.ProjectileRadius, 0.0));
}

float USkillProjectileCastAction::GetConfiguredProjectileArcHeight() const
{
    return static_cast<float>(FMath::Max(Settings.ProjectileArcHeight, 0.0));
}

float USkillProjectileCastAction::GetConfiguredProjectileArcGravityScale() const
{
    return static_cast<float>(FMath::Max(Settings.ProjectileArcGravityScale, 0.0));
}

FGameplayTag USkillProjectileCastAction::GetConfiguredShootProjectileEventTag() const
{
    if (Settings.FireEventTag.IsValid())
    {
       return Settings.FireEventTag;
    }
    const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
    return SkillDataAsset ? SkillDataAsset->Animation.PrimaryEventTag : FGameplayTag();
}

TArray<FName> USkillProjectileCastAction::GetConfiguredProjectileSocketNames() const
{
    TArray<FName> SocketNames;
    for (const FName& SocketName : Settings.ProjectileSocketNames)
    {
       if (!SocketName.IsNone())
       {
          SocketNames.Add(SocketName);
       }
    }
    return SocketNames;
}

float USkillProjectileCastAction::GetConfiguredProjectileSocketFireInterval() const
{
    return static_cast<float>(FMath::Max(Settings.ProjectileSocketFireInterval, 0.0));
}

void USkillProjectileCastAction::ApplyConfiguredProjectileVisuals(ASkillProjectile* Projectile) const
{
    if (!IsValid(Projectile))
    {
       return;
    }
    Projectile->ConfigureProjectileVisuals(
       Settings.MuzzleNiagaraSystem.Get(),
       Settings.ProjectileNiagaraSystem.Get(),
       Settings.HitNiagaraSystem.Get(),
       Settings.bSpawnHitNiagaraOnGround,
       FGameplayTag(),
       FGameplayTag());
}

void USkillProjectileCastAction::ApplyConfiguredProjectileTrajectory(ASkillProjectile* Projectile) const
{
    if (!IsValid(Projectile))
    {
       return;
    }

    Projectile->ConfigureArcTrajectory(
       Settings.bUseArcTrajectory,
       GetConfiguredProjectileArcHeight(),
       GetConfiguredProjectileArcGravityScale());
}

void USkillProjectileCastAction::ApplyConfiguredProjectileImpactPersistence(ASkillProjectile* Projectile) const
{
    if (!IsValid(Projectile))
    {
       return;
    }
    Projectile->ConfigureImpactPersistence(
       Settings.bStickOnImpact,
       static_cast<float>(FMath::Max(Settings.PostImpactLifeSpan, 0.0)));
}

float USkillProjectileCastAction::GetConfiguredTargetTraceMaxRange() const
{
    return static_cast<float>(FMath::Max(Settings.TargetTraceMaxRange, 0.0));
}

float USkillProjectileCastAction::GetConfiguredMinimumTargetDistanceFromSpawn() const
{
    return static_cast<float>(FMath::Max(Settings.MinimumTargetDistanceFromSpawn, 0.0));
}

bool USkillProjectileCastAction::GetConfiguredDrawTargetTraceDebug() const
{
    return LabSkillDebug::IsDrawingEnabled() && Settings.bDrawTargetTraceDebug;
}

FName USkillProjectileCastAction::GetConfiguredSpawnSocketName() const
{
    for (const FName& SocketName : Settings.ProjectileSocketNames)
    {
       if (!SocketName.IsNone())
       {
          return SocketName;
       }
    }
    return NAME_None;
}

float USkillProjectileCastAction::GetConfiguredMinimumForwardSpawnOffset() const
{
    return static_cast<float>(FMath::Max(Settings.MinimumForwardSpawnOffset, 0.0));
}

bool USkillProjectileCastAction::IsConfiguredReadiedProjectileChargeGrowthEnabled() const
{
    return Settings.bGrowProjectileSize;
}

float USkillProjectileCastAction::GetConfiguredReadiedProjectileScaleDuration() const
{
    return static_cast<float>(FMath::Max(Settings.ProjectileScaleDuration, 0.0));
}

void USkillProjectileCastAction::ApplyConfiguredStatusEffect(ASkillProjectile* Projectile) const
{
    if (!IsValid(Projectile))
    {
       return;
    }

    UStatusEffectDefinition* StatusEffectDataAsset =
       GetConfiguredStatusEffectDataAsset();
    Projectile->SetDebuffEffectSpecHandle(
       MakeStatusEffectSpec(),
       StatusEffectDataAsset);
}

void USkillProjectileCastAction::ApplyReadiedProjectileScaleGrowth(ASkillProjectile* Projectile) const
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
    const FVector StartScale = Settings.ProjectileStartScale;
    const FVector TargetScale = Settings.ProjectileFinalScale;
    const FName NiagaraVector2DParameterName = Settings.GrowthUserParameterName;
    const FVector2D NiagaraStartSize = Settings.GrowthUserParameterStartValue;
    const FVector2D NiagaraTargetSize = Settings.GrowthUserParameterFinalValue;
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

TSubclassOf<AGameplayAbilityTargetActor> USkillProjectileCastAction::GetConfiguredGroundTargetActorClass() const
{
    const TSubclassOf<AGameplayAbilityTargetActor> ConfiguredClass = Settings.GroundTargetActorClass;
    if (ConfiguredClass
       && ConfiguredClass->IsChildOf(AGameplayAbilityTargetActor_GroundTrace::StaticClass())
       && !ConfiguredClass->IsChildOf(AGroundTargetActor::StaticClass()))
    {
       return AGroundTargetActor::StaticClass();
    }
    return ConfiguredClass;
}

float USkillProjectileCastAction::GetConfiguredGroundTargetingMaxRange() const
{
    return static_cast<float>(FMath::Max(Settings.GroundTargetingMaxRange, 0.0));
}

float USkillProjectileCastAction::GetConfiguredGroundTargetingTraceStartHeight() const
{
    return static_cast<float>(FMath::Max(Settings.TargetGroundTraceStartHeight, 0.0));
}

float USkillProjectileCastAction::GetConfiguredGroundTargetingTraceDepth() const
{
    return static_cast<float>(FMath::Max(Settings.TargetGroundTraceDepth, 0.0));
}

float USkillProjectileCastAction::GetConfiguredGroundTargetingCollisionRadius() const
{
    return static_cast<float>(FMath::Max(Settings.GroundTargetingCollisionRadius, 0.0));
}

float USkillProjectileCastAction::GetConfiguredGroundTargetingCollisionHeight() const
{
    return static_cast<float>(FMath::Max(Settings.GroundTargetingCollisionHeight, 0.0));
}

bool USkillProjectileCastAction::GetConfiguredDrawGroundTargetingDebug() const
{
    return LabSkillDebug::IsDrawingEnabled() && Settings.bDrawGroundTargetingDebug;
}

float USkillProjectileCastAction::GetConfiguredGroundTargetingDecalSize() const
{
    return static_cast<float>(FMath::Max(Settings.TargetDecalSize, 0.0));
}

float USkillProjectileCastAction::GetConfiguredGroundTargetingDecalFinalSize() const
{
    return Settings.TargetDecalFinalSize > 0.0
       ? static_cast<float>(Settings.TargetDecalFinalSize)
       : GetConfiguredGroundTargetingDecalSize();
}

float USkillProjectileCastAction::CalculateConfiguredImpactAreaDamageRadius(
    const float ChargeDamageAlpha) const
{
    if (!Settings.bUseGroundTargeting || !Settings.TargetDecal.Get())
    {
       return 0.0f;
    }

    const float StartDiameter = GetConfiguredGroundTargetingDecalSize();
    const float FinalDiameter = GetConfiguredGroundTargetingDecalFinalSize();
    const float DamageDiameter = Settings.bGrowTargetDecalSize
       ? FMath::Lerp(StartDiameter, FinalDiameter, FMath::Clamp(ChargeDamageAlpha, 0.0f, 1.0f))
       : StartDiameter;

    // Targeting decal sizes are configured as diameters, matching the AOE decal convention.
    return DamageDiameter * 0.5f;
}

bool USkillProjectileCastAction::TryBuildGroundTargetingDecalGrowth(
    float& OutStartSize,
    float& OutTargetSize,
    float& OutDuration) const
{
    OutStartSize = GetConfiguredGroundTargetingDecalSize();
    OutTargetSize = GetConfiguredGroundTargetingDecalFinalSize();
    OutDuration = 0.0f;

    if (!Settings.bUseGroundTargeting
       || !Settings.bGrowTargetDecalSize
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
