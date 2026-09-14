#include "AbilitySystem/Skill/Actions/SkillProjectileCastAction.h"

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
#include "Definition/AbilitySystem/SkillProjectileSettings.h"
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

namespace
{
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

UAnimMontage* USkillProjectileCastAction::GetConfiguredShootMontage() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	return SkillDataAsset && SkillDataAsset->Animation.PrimaryMontage
		? SkillDataAsset->Animation.PrimaryMontage.Get()
		: nullptr;
}

TSubclassOf<AProjectileBase> USkillProjectileCastAction::GetConfiguredProjectileClass() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings ? ProjectileSettings->ProjectileActorClass : nullptr;
}

TSubclassOf<UGameplayEffect> USkillProjectileCastAction::GetConfiguredDamageEffectClass() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->GetResolvedDamageConfig().GameplayEffectClass : nullptr;
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

	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings ? ProjectileSettings->StatusEffectClass : nullptr;
}

float USkillProjectileCastAction::GetConfiguredStatusEffectLevel() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (SkillDataAsset && SkillDataAsset->StatusEffectDataAsset)
	{
		return FMath::Max(SkillDataAsset->StatusEffectLevel, 1.0f);
	}

	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings ? FMath::Max(ProjectileSettings->StatusEffectLevel, 1.0f) : 1.0f;
}

float USkillProjectileCastAction::GetConfiguredStatusEffectDuration() const
{
	const UStatusEffectDefinition* StatusEffectDataAsset = GetConfiguredStatusEffectDataAsset();
	return StatusEffectDataAsset ? FMath::Max(StatusEffectDataAsset->StatusDuration, 0.0f) : 0.0f;
}

float USkillProjectileCastAction::GetConfiguredProjectileSpeed() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->ProjectileSpeed, 0.0))
		: 0.0f;
}

float USkillProjectileCastAction::GetConfiguredProjectileRadius() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->ProjectileRadius, 0.0))
		: 0.0f;
}

bool USkillProjectileCastAction::ShouldUseConfiguredProjectileArcTrajectory() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings && ProjectileSettings->bUseArcTrajectory;
}

float USkillProjectileCastAction::GetConfiguredProjectileArcHeight() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->ProjectileArcHeight, 0.0))
		: 0.0f;
}

float USkillProjectileCastAction::GetConfiguredProjectileArcGravityScale() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->ProjectileArcGravityScale, 0.0))
		: 1.0f;
}

FGameplayTag USkillProjectileCastAction::GetConfiguredDamageDataTag() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{
		return FGameplayTag();
	}

	return SkillDataAsset->GetResolvedDamageConfig().MagnitudeDataTag;
}

FGameplayTag USkillProjectileCastAction::GetConfiguredShootProjectileEventTag() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	if (!ProjectileSettings)
	{
		return FGameplayTag();
	}

	return ProjectileSettings->FireEventTag.IsValid()
		? ProjectileSettings->FireEventTag
		: SkillDataAsset->Animation.PrimaryEventTag;
}

bool USkillProjectileCastAction::IsConfiguredImmediateFireMode() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings && ProjectileSettings->FireMode == EProjectileFireMode::Immediate;
}

TArray<FName> USkillProjectileCastAction::GetConfiguredProjectileSocketNames() const
{
	TArray<FName> SocketNames;
	if (const FSkillProjectileSettings* ProjectileSettings = &Settings)
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

float USkillProjectileCastAction::GetConfiguredProjectileSocketFireInterval() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (const FSkillProjectileSettings* ProjectileSettings = &Settings)
	{
		return static_cast<float>(FMath::Max(ProjectileSettings->ProjectileSocketFireInterval, 0.0));
	}

	return 0.0f;
}

void USkillProjectileCastAction::GetConfiguredProjectileVisuals(
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

	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (const FSkillProjectileSettings* ProjectileSettings = &Settings)
	{
		OutMuzzleFX = ProjectileSettings->MuzzleNiagaraSystem.Get();
		OutProjectileFX = ProjectileSettings->ProjectileNiagaraSystem.Get();
		OutHitFX = ProjectileSettings->HitNiagaraSystem.Get();
		bOutSpawnHitNiagaraOnGround = ProjectileSettings->bSpawnHitNiagaraOnGround;
	}
}

void USkillProjectileCastAction::ApplyConfiguredProjectileVisuals(AProjectileBase* Projectile) const
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

void USkillProjectileCastAction::ApplyConfiguredProjectileTrajectory(AProjectileBase* Projectile) const
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

void USkillProjectileCastAction::ApplyConfiguredProjectileImpactPersistence(AProjectileBase* Projectile) const
{
	if (!IsValid(Projectile))
	{
		return;
	}

	const FSkillProjectileSettings* ProjectileSettings =
		&Settings;
	Projectile->ConfigureImpactPersistence(
		ProjectileSettings && ProjectileSettings->bStickOnImpact,
		ProjectileSettings
			? static_cast<float>(FMath::Max(ProjectileSettings->PostImpactLifeSpan, 0.0))
			: 0.0f);
}

float USkillProjectileCastAction::GetConfiguredTargetTraceMaxRange() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->TargetTraceMaxRange, 0.0))
		: 0.0f;
}

FCollisionProfileName USkillProjectileCastAction::GetConfiguredTargetTraceProfile() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings
		? ProjectileSettings->TargetTraceProfile
		: FCollisionProfileName(TEXT("NoCollision"));
}

float USkillProjectileCastAction::GetConfiguredMinimumTargetDistanceFromSpawn() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->MinimumTargetDistanceFromSpawn, 0.0))
		: 0.0f;
}

bool USkillProjectileCastAction::GetConfiguredTraceAffectsAimPitch() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings && ProjectileSettings->bTraceAffectsAimPitch;
}

bool USkillProjectileCastAction::GetConfiguredDrawTargetTraceDebug() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return LabSkillDebug::IsDrawingEnabled()
		&& ProjectileSettings
		&& ProjectileSettings->bDrawTargetTraceDebug;
}

FName USkillProjectileCastAction::GetConfiguredSpawnSocketName() const
{
	if (const FSkillProjectileSettings* ProjectileSettings = &Settings)
	{
		return GetFirstConfiguredProjectileSocketName(*ProjectileSettings);
	}

	return NAME_None;
}

FVector USkillProjectileCastAction::GetConfiguredSpawnLocationOffset() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings ? ProjectileSettings->SpawnLocationOffset : FVector::ZeroVector;
}

float USkillProjectileCastAction::GetConfiguredMinimumForwardSpawnOffset() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->MinimumForwardSpawnOffset, 0.0))
		: 0.0f;
}

bool USkillProjectileCastAction::IsConfiguredReadiedProjectileChargeGrowthEnabled() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings
		&& (ProjectileSettings->bGrowProjectileSize
			|| ProjectileSettings->FireMode == EProjectileFireMode::HoldThenConfirm);
}

FVector USkillProjectileCastAction::GetConfiguredReadiedProjectileStartScale() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings ? ProjectileSettings->ProjectileStartScale : FVector::OneVector;
}

FVector USkillProjectileCastAction::GetConfiguredReadiedProjectileTargetScale() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings ? ProjectileSettings->ProjectileFinalScale : FVector::OneVector;
}

float USkillProjectileCastAction::GetConfiguredReadiedProjectileScaleDuration() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->ProjectileScaleDuration, 0.0))
		: 0.0f;
}

FName USkillProjectileCastAction::GetConfiguredReadiedProjectileNiagaraVector2DParameterName() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings ? ProjectileSettings->GrowthUserParameterName : NAME_None;
}

FVector2D USkillProjectileCastAction::GetConfiguredReadiedProjectileNiagaraStartSize() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings ? ProjectileSettings->GrowthUserParameterStartValue : FVector2D::UnitVector;
}

FVector2D USkillProjectileCastAction::GetConfiguredReadiedProjectileNiagaraTargetSize() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings ? ProjectileSettings->GrowthUserParameterFinalValue : FVector2D::UnitVector;
}

void USkillProjectileCastAction::ApplyConfiguredStatusEffect(AProjectileBase* Projectile) const
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

void USkillProjectileCastAction::ApplyReadiedProjectileScaleGrowth(AProjectileBase* Projectile) const
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

bool USkillProjectileCastAction::ShouldUseGroundTargeting() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings && ProjectileSettings->bUseGroundTargeting;
}

TSubclassOf<AGameplayAbilityTargetActor> USkillProjectileCastAction::GetConfiguredGroundTargetActorClass() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
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

float USkillProjectileCastAction::GetConfiguredGroundTargetingMaxRange() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->GroundTargetingMaxRange, 0.0))
		: 0.0f;
}

FCollisionProfileName USkillProjectileCastAction::GetConfiguredGroundTargetingTraceProfile() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings
		? ProjectileSettings->GroundTargetingTraceProfile
		: FCollisionProfileName(TEXT("BlockAll"));
}

float USkillProjectileCastAction::GetConfiguredGroundTargetingTraceStartHeight() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->TargetGroundTraceStartHeight, 0.0))
		: 0.0f;
}

float USkillProjectileCastAction::GetConfiguredGroundTargetingTraceDepth() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->TargetGroundTraceDepth, 0.0))
		: 0.0f;
}

float USkillProjectileCastAction::GetConfiguredGroundTargetingCollisionRadius() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->GroundTargetingCollisionRadius, 0.0))
		: 0.0f;
}

float USkillProjectileCastAction::GetConfiguredGroundTargetingCollisionHeight() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->GroundTargetingCollisionHeight, 0.0))
		: 0.0f;
}

bool USkillProjectileCastAction::GetConfiguredGroundTargetingTraceAffectsAimPitch() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings && ProjectileSettings->bGroundTargetingTraceAffectsAimPitch;
}

bool USkillProjectileCastAction::GetConfiguredDrawGroundTargetingDebug() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return LabSkillDebug::IsDrawingEnabled()
		&& ProjectileSettings
		&& ProjectileSettings->bDrawGroundTargetingDebug;
}

UMaterialInterface* USkillProjectileCastAction::GetConfiguredGroundTargetingDecal() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings ? ProjectileSettings->TargetDecal.Get() : nullptr;
}

float USkillProjectileCastAction::GetConfiguredGroundTargetingDecalSize() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings
		? static_cast<float>(FMath::Max(ProjectileSettings->TargetDecalSize, 0.0))
		: 0.0f;
}

float USkillProjectileCastAction::GetConfiguredGroundTargetingDecalFinalSize() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (const FSkillProjectileSettings* ProjectileSettings = &Settings)
	{
		return ProjectileSettings->TargetDecalFinalSize > 0.0
			? static_cast<float>(ProjectileSettings->TargetDecalFinalSize)
			: GetConfiguredGroundTargetingDecalSize();
	}

	return GetConfiguredGroundTargetingDecalSize();
}

bool USkillProjectileCastAction::ShouldGrowConfiguredGroundTargetingDecal() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings && ProjectileSettings->bGrowTargetDecalSize;
}

FLinearColor USkillProjectileCastAction::GetConfiguredGroundTargetingDecalColor() const
{
	const FSkillProjectileSettings* ProjectileSettings = &Settings;
	return ProjectileSettings ? ProjectileSettings->TargetDecalColor : FLinearColor::White;
}

float USkillProjectileCastAction::CalculateConfiguredImpactAreaDamageRadius(
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

bool USkillProjectileCastAction::TryBuildGroundTargetingDecalGrowth(
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
