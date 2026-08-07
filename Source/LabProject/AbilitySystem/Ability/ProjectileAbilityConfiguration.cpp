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

namespace
{
	const FSkillProjectileSettings* GetProjectileSettings(const USkillDefinition* SkillDataAsset)
	{
		return SkillDataAsset
			&& SkillDataAsset->SkillDataType == ESkillDataType::Projectile
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

UStatusEffectDefinition* UProjectileAbility::GetConfiguredStatusEffectDataAsset() const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->StatusEffectDataAsset.Get() : nullptr;
}

TSubclassOf<UGameplayEffect> UProjectileAbility::GetConfiguredStatusEffectClass() const
{
	if (const UStatusEffectDefinition* StatusEffectDataAsset = GetConfiguredStatusEffectDataAsset())
	{
		if (StatusEffectDataAsset->DebuffGameplayEffectClass)
		{
			return StatusEffectDataAsset->DebuffGameplayEffectClass;
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
	return ProjectileSettings && ProjectileSettings->FireMode == ESkillProjectileFireMode::Immediate;
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
			|| ProjectileSettings->FireMode == ESkillProjectileFireMode::HoldThenConfirm);
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

	UStatusEffectDefinition* StatusEffectDataAsset =
		GetConfiguredStatusEffectDataAsset();
	Projectile->SetDebuffEffectSpecHandle(
		MakeStatusEffectSpec(),
		StatusEffectDataAsset);
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
