#include "AbilitySystem/Skills/SkillTypes.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/EffectActors/EffectAreaBase.h"
#include "AbilitySystem/Projectiles/ProjectileBase.h"
#include "AbilitySystem/TargetingActors/TargetActor_GroundTrace_Decal.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillTypes)

namespace
{
	FSkillLevelUnlock& FindOrAddLevelUnlock(TArray<FSkillLevelUnlock>& LevelUnlocks, const int32 RequiredLevel)
	{
		const int32 NormalizedLevel = FMath::Max(RequiredLevel, 1);
		for (FSkillLevelUnlock& LevelUnlock : LevelUnlocks)
		{
			if (FMath::Max(LevelUnlock.RequiredLevel, 1) == NormalizedLevel)
			{
				return LevelUnlock;
			}
		}

		FSkillLevelUnlock& AddedUnlock = LevelUnlocks.AddDefaulted_GetRef();
		AddedUnlock.RequiredLevel = NormalizedLevel;
		return AddedUnlock;
	}
}

USkillDataAsset::USkillDataAsset()
{
	ProjectileClass = AProjectileBase::StaticClass();
	ProjectileSpeed = 2000.0;
	ShootProjectileEventTag = LabGameplayTags::Event_ShootProjectile;
	ProjectileDamageDataTag = LabGameplayTags::Data_Damage;
	TargetTraceMaxRange = 999999.0;
	ProjectileCrosshairWidgetTag = LabGameplayTags::UI_Widget_AimCrosshair;

	ProjectileCameraSettings.TargetFOV = 50.0f;
	ProjectileCameraSettings.TargetBoomSocketOffset = FVector(30.0f, 200.0f, 65.0f);
	ProjectileCameraSettings.TargetCameraRotation = FRotator(0.0f, -20.0f, 0.0f);
	ProjectileCameraSettings.InterpSpeed = 10.0f;

	AOEDamageDataTag = LabGameplayTags::Data_Damage;
	AOEMontageTriggerEventTag = LabGameplayTags::Event_Montage_Trigger;
	AOEIndicatorCueTag = LabGameplayTags::GameplayCue_AOEIndicator;
	AOELightningBoltCueTag = LabGameplayTags::GameplayCue_LightningBolt;
	AOETargetActorClass = ATargetActor_GroundTrace_Decal::StaticClass();
	AOEDamageObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	AOETargetingMaxRange = 3000.0;
	AOERadius = 256.0;

	AOECameraSettings.TargetFOV = 70.0f;
	AOECameraSettings.TargetBoomSocketOffset = FVector(80.0f, 320.0f, 100.0f);
	AOECameraSettings.TargetCameraRotation = FRotator::ZeroRotator;
	AOECameraSettings.InterpSpeed = 10.0f;

	DashStrength = 2000.0;
	DashDuration = 0.3;
	DashCueTag = LabGameplayTags::GameplayCue_Dash_Active;
}

void USkillDataAsset::PostLoad()
{
	Super::PostLoad();

	MigrateDeprecatedBaseDefinitionToLegacyLevelUnlocks();
}

FPrimaryAssetId USkillDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Skill"), GetFName());
}

FText USkillDataAsset::GetDisplayName() const
{
	return !Name.IsNone() ? FText::FromName(Name) : FText::FromName(GetFName());
}

FText USkillDataAsset::GetDescriptionForLevel(const int32 Level) const
{
	if (Level <= 0)
	{
		return FText::GetEmpty();
	}

	const int32 DescriptionIndex = Level - 1;
	return DescriptionPerLevel.IsValidIndex(DescriptionIndex)
		? DescriptionPerLevel[DescriptionIndex]
		: FText::GetEmpty();
}

UObject* USkillDataAsset::GetIconResource() const
{
	return Icon.Get();
}

bool USkillDataAsset::ShouldShowInAbilitiesBar() const
{
	return bShowInAbilitiesBar && SkillType == EPdSkillType::Active;
}

bool USkillDataAsset::HasLegacyLevelUnlocks() const
{
	return !LevelUnlocks.IsEmpty();
}

TArray<TSubclassOf<UGameplayAbility>> USkillDataAsset::GetLegacyAbilitiesToGrantForLevel(const int32 Level) const
{
	TArray<TSubclassOf<UGameplayAbility>> Result;
	const int32 EffectiveLevel = FMath::Max(Level, 1);

	for (const FSkillLevelUnlock& LevelUnlock : LevelUnlocks)
	{
		if (EffectiveLevel < FMath::Max(LevelUnlock.RequiredLevel, 1))
		{
			continue;
		}

		for (const TSubclassOf<UGameplayAbility>& AbilityClass : LevelUnlock.AbilitiesToGrant)
		{
			if (AbilityClass)
			{
				Result.AddUnique(AbilityClass);
			}
		}
	}

	return Result;
}

TArray<TSubclassOf<UGameplayEffect>> USkillDataAsset::GetLegacyEffectsToApplyForLevel(const int32 Level) const
{
	TArray<TSubclassOf<UGameplayEffect>> Result;
	const int32 EffectiveLevel = FMath::Max(Level, 1);

	for (const FSkillLevelUnlock& LevelUnlock : LevelUnlocks)
	{
		if (EffectiveLevel < FMath::Max(LevelUnlock.RequiredLevel, 1))
		{
			continue;
		}

		for (const TSubclassOf<UGameplayEffect>& EffectClass : LevelUnlock.EffectsToApply)
		{
			if (EffectClass)
			{
				Result.AddUnique(EffectClass);
			}
		}
	}

	return Result;
}

TArray<TObjectPtr<UPdStatusEffectDataAsset>> USkillDataAsset::GetLegacyStatusEffectsToUnlockForLevel(const int32 Level) const
{
	TArray<TObjectPtr<UPdStatusEffectDataAsset>> Result;
	const int32 EffectiveLevel = FMath::Max(Level, 1);

	for (const FSkillLevelUnlock& LevelUnlock : LevelUnlocks)
	{
		if (EffectiveLevel < FMath::Max(LevelUnlock.RequiredLevel, 1))
		{
			continue;
		}

		for (const TObjectPtr<UPdStatusEffectDataAsset>& StatusEffectDataAsset : LevelUnlock.StatusEffectsToUnlock)
		{
			if (StatusEffectDataAsset)
			{
				Result.AddUnique(StatusEffectDataAsset);
			}
		}
	}

	return Result;
}

void USkillDataAsset::MigrateDeprecatedBaseDefinitionToLegacyLevelUnlocks()
{
	if (AbilitiesToGrant.IsEmpty() && EffectsToApply.IsEmpty() && StatusEffectsToUnlock.IsEmpty())
	{
		return;
	}

	FSkillLevelUnlock& LevelOneUnlock = FindOrAddLevelUnlock(LevelUnlocks, 1);

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : AbilitiesToGrant)
	{
		if (AbilityClass)
		{
			LevelOneUnlock.AbilitiesToGrant.AddUnique(AbilityClass);
		}
	}

	for (const TSubclassOf<UGameplayEffect>& EffectClass : EffectsToApply)
	{
		if (EffectClass)
		{
			LevelOneUnlock.EffectsToApply.AddUnique(EffectClass);
		}
	}

	for (const TObjectPtr<UPdStatusEffectDataAsset>& StatusEffectDataAsset : StatusEffectsToUnlock)
	{
		if (StatusEffectDataAsset)
		{
			LevelOneUnlock.StatusEffectsToUnlock.AddUnique(StatusEffectDataAsset);
		}
	}
}

TArray<FProjectileImpactEffectAreaSpawnConfig> USkillDataAsset::GetLegacyProjectileImpactEffectAreasForLevel(const int32 Level) const
{
	TArray<FProjectileImpactEffectAreaSpawnConfig> Result;
	const int32 EffectiveLevel = FMath::Max(Level, 1);

	for (const FSkillLevelUnlock& LevelUnlock : LevelUnlocks)
	{
		if (EffectiveLevel < FMath::Max(LevelUnlock.RequiredLevel, 1))
		{
			continue;
		}

		for (const FProjectileImpactEffectAreaSpawnConfig& ImpactEffectArea : LevelUnlock.ProjectileImpactEffectAreas)
		{
			if (ImpactEffectArea.EffectAreaClass)
			{
				Result.Add(ImpactEffectArea);
			}
		}
	}

	return Result;
}
