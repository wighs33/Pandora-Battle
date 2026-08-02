#include "Definition/AbilitySystem/SkillTypes.h"

#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetActor_GroundTrace.h"
#include "AbilitySystem/EffectActors/EffectAreaBase.h"
#include "AbilitySystem/Projectiles/ProjectileBase.h"
#include "AbilitySystem/TargetingActors/TargetActor_GroundTrace_Decal.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"
#include "HAL/IConsoleManager.h"
#include "UObject/ObjectSaveContext.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#include "Engine/AssetManager.h"
#include "Logging/PdLogRateLimiter.h"
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillTypes)

namespace
{
	constexpr int32 LegacyProjectileStructMigrationVersion = 1;
	constexpr int32 CurrentProjectileSettingsVersion = 2;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	TAutoConsoleVariable<int32> CVarSkillDebugDrawing(
		TEXT("lab.Skill.DebugDraw"),
		0,
		TEXT("Enables skill debug drawing for data assets whose individual debug flag is enabled.\n")
		TEXT("0: disabled (default), 1: enabled"),
		ECVF_Cheat);
#endif

	void NormalizeSkillTypeForDataType(USkillDefinition* SkillDataAsset)
	{
		if (!SkillDataAsset)
		{
			return;
		}

		if (SkillDataAsset->SkillType == EPdSkillType::Active
			|| SkillDataAsset->SkillType == EPdSkillType::Passive)
		{
			SkillDataAsset->SkillType = EPdSkillType::Instant;
		}
		else if (SkillDataAsset->SkillType == EPdSkillType::Triggered)
		{
			SkillDataAsset->SkillType = EPdSkillType::Duration;
		}

		if ((SkillDataAsset->SkillDataType == EPdSkillDataType::Missile
				|| SkillDataAsset->SkillDataType == EPdSkillDataType::Summon)
			&& SkillDataAsset->SkillType != EPdSkillType::Press
			&& SkillDataAsset->SkillType != EPdSkillType::Duration)
		{
			SkillDataAsset->SkillType = EPdSkillType::Instant;
		}

		SkillDataAsset->ProjectileSettings.bEnabled =
			SkillDataAsset->SkillDataType == EPdSkillDataType::Projectile;
	}

	void EnsureAreaAnimationDefaults(USkillDefinition* SkillDataAsset)
	{
		if (!SkillDataAsset || SkillDataAsset->SkillDataType != EPdSkillDataType::Area)
		{
			return;
		}

		if (!SkillDataAsset->Animation.PrimaryEventTag.IsValid())
		{
			SkillDataAsset->Animation.PrimaryEventTag = LabGameplayTags::Event_Montage_Trigger;
		}
	}

#if WITH_EDITOR
	constexpr double CookValidationLogIntervalSeconds = 30.0;
	TMap<FName, FPdLogRateLimiter> CookValidationLogLimiters;

	void MarkSkillTypeInvalid(FDataValidationContext& Context, EDataValidationResult& Result, const FText& Message)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(Message);
	}

	void ValidateUniqueSkillContentNames(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const USkillDefinition& SkillDefinition)
	{
		TSet<FName> LookupNames;
		LookupNames.Add(SkillDefinition.GetFName());
		if (!SkillDefinition.Name.IsNone())
		{
			LookupNames.Add(SkillDefinition.Name);
		}

		TArray<FAssetData> SkillAssets;
		UAssetManager::Get().GetPrimaryAssetDataList(FPrimaryAssetType(TEXT("Skill")), SkillAssets);
		for (const FAssetData& AssetData : SkillAssets)
		{
			if (AssetData.GetSoftObjectPath() == FSoftObjectPath(&SkillDefinition))
			{
				continue;
			}

			TSet<FName> OtherLookupNames;
			OtherLookupNames.Add(AssetData.AssetName);

			FName OtherLogicalName = NAME_None;
			if (AssetData.GetTagValue(GET_MEMBER_NAME_CHECKED(USkillDefinition, Name), OtherLogicalName)
				&& !OtherLogicalName.IsNone())
			{
				OtherLookupNames.Add(OtherLogicalName);
			}

			for (const FName LookupName : LookupNames)
			{
				if (!OtherLookupNames.Contains(LookupName))
				{
					continue;
				}

				MarkSkillTypeInvalid(
					Context,
					Result,
					FText::Format(
						NSLOCTEXT(
							"SkillDataAsset",
							"DuplicateContentName",
							"Skill content name '{0}' conflicts with asset '{1}'. Asset names and Skill.Name values must be unique."),
						FText::FromName(LookupName),
						FText::FromString(AssetData.GetSoftObjectPath().ToString())));
				break;
			}
		}
	}

	FText SkillFieldText(const TCHAR* FieldName)
	{
		return FText::FromString(FString(FieldName));
	}

	void ValidateFinite(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const double Value,
		const TCHAR* FieldName)
	{
		if (!FMath::IsFinite(Value))
		{
			MarkSkillTypeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("SkillDataAsset", "NonFiniteValue", "{0} must be a finite value."),
				SkillFieldText(FieldName)));
		}
	}

	void ValidateNonNegative(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const double Value,
		const TCHAR* FieldName)
	{
		if (!FMath::IsFinite(Value) || Value < 0.0)
		{
			MarkSkillTypeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("SkillDataAsset", "InvalidNonNegativeValue", "{0} must be a non-negative finite value."),
				SkillFieldText(FieldName)));
		}
	}

	void ValidatePositive(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const double Value,
		const TCHAR* FieldName)
	{
		if (!FMath::IsFinite(Value) || Value <= 0.0)
		{
			MarkSkillTypeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("SkillDataAsset", "InvalidPositiveValue", "{0} must be a positive finite value."),
				SkillFieldText(FieldName)));
		}
	}

	void ValidateVector(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FVector& Value,
		const TCHAR* FieldName)
	{
		if (Value.ContainsNaN())
		{
			MarkSkillTypeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("SkillDataAsset", "InvalidVector", "{0} contains NaN."),
				SkillFieldText(FieldName)));
		}
	}

	void ValidateVector2D(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FVector2D& Value,
		const TCHAR* FieldName)
	{
		if (!FMath::IsFinite(Value.X) || !FMath::IsFinite(Value.Y))
		{
			MarkSkillTypeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("SkillDataAsset", "InvalidVector2D", "{0} contains a non-finite value."),
				SkillFieldText(FieldName)));
		}
	}

	void ValidateRotator(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FRotator& Value,
		const TCHAR* FieldName)
	{
		if (Value.ContainsNaN())
		{
			MarkSkillTypeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("SkillDataAsset", "InvalidRotator", "{0} contains NaN."),
				SkillFieldText(FieldName)));
		}
	}

	void ValidateTransform(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FTransform& Value,
		const TCHAR* FieldName)
	{
		if (Value.ContainsNaN())
		{
			MarkSkillTypeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("SkillDataAsset", "InvalidTransform", "{0} contains NaN."),
				SkillFieldText(FieldName)));
		}
	}

	void ValidateLinearColor(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FLinearColor& Value,
		const TCHAR* FieldName)
	{
		if (!FMath::IsFinite(Value.R)
			|| !FMath::IsFinite(Value.G)
			|| !FMath::IsFinite(Value.B)
			|| !FMath::IsFinite(Value.A))
		{
			MarkSkillTypeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("SkillDataAsset", "InvalidLinearColor", "{0} contains a non-finite color channel."),
				SkillFieldText(FieldName)));
		}
	}

	void ValidateGameplayEffectConfig(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FSkillGameplayEffectConfig& Config,
		const TCHAR* FieldName)
	{
		ValidateFinite(Context, Result, Config.Magnitude, FieldName);

		if (Config.GameplayEffectClass
			&& FMath::Abs(Config.Magnitude) > KINDA_SMALL_NUMBER
			&& !Config.MagnitudeDataTag.IsValid())
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("SkillDataAsset", "GameplayEffectMissingMagnitudeTag", "{0} has a GameplayEffectClass, but MagnitudeDataTag is empty. SetByCaller magnitude may not be applied."),
				SkillFieldText(FieldName)));
		}

		if (!Config.GameplayEffectClass && FMath::Abs(Config.Magnitude) > KINDA_SMALL_NUMBER)
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("SkillDataAsset", "GameplayEffectSettingsWithoutClass", "{0} has magnitude settings, but GameplayEffectClass is empty."),
				SkillFieldText(FieldName)));
		}
	}

	void ValidateSkillAimCameraSettings(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FWeaponAimCameraSettings& CameraSettings,
		const TCHAR* FieldName)
	{
		if (!FMath::IsFinite(CameraSettings.TargetFOV) || CameraSettings.TargetFOV <= 0.0f)
		{
			MarkSkillTypeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("SkillDataAsset", "InvalidAimCameraFOV", "{0}.TargetFOV must be a positive finite value."),
				SkillFieldText(FieldName)));
		}
		else if (CameraSettings.TargetFOV < 5.0f || CameraSettings.TargetFOV > 170.0f)
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("SkillDataAsset", "OutOfRangeAimCameraFOV", "{0}.TargetFOV will be clamped to 5..170 at runtime."),
				SkillFieldText(FieldName)));
		}

		ValidateVector(Context, Result, CameraSettings.TargetBoomSocketOffset, TEXT("AOECameraSettings.TargetBoomSocketOffset"));
		ValidateRotator(Context, Result, CameraSettings.TargetCameraRotation, TEXT("AOECameraSettings.TargetCameraRotation"));
		ValidateNonNegative(Context, Result, CameraSettings.InterpSpeed, TEXT("AOECameraSettings.InterpSpeed"));
	}

	void ValidateCommonSkillData(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const USkillDefinition& SkillDataAsset)
	{
		const int32 SelectedEffectIconCount =
			static_cast<int32>(SkillDataAsset.bShowBurnEffectIcon)
			+ static_cast<int32>(SkillDataAsset.bShowFrostbiteEffectIcon)
			+ static_cast<int32>(SkillDataAsset.bShowElectricShockEffectIcon)
			+ static_cast<int32>(SkillDataAsset.bShowShieldEffectIcon);
		if (SelectedEffectIconCount > 1)
		{
			MarkSkillTypeInvalid(
				Context,
				Result,
				NSLOCTEXT(
					"SkillDataAsset",
					"MultipleEffectIconsSelected",
					"Only one UI Effect Icon checkbox can be enabled because each skill has one effect icon image slot."));
		}

		ValidateNonNegative(Context, Result, SkillDataAsset.ManaCost, TEXT("ManaCost"));
		ValidateNonNegative(Context, Result, SkillDataAsset.Time.CooldownDuration, TEXT("Time.CooldownDuration"));
		ValidateNonNegative(Context, Result, SkillDataAsset.Time.Duration, TEXT("Time.Duration"));

		ValidateGameplayEffectConfig(Context, Result, SkillDataAsset.Damage.ToGameplayEffectConfig(), TEXT("Damage"));
		if (SkillDataAsset.Damage.bRepeatTriggerDamageWhileOverlapping)
		{
			ValidatePositive(Context, Result, SkillDataAsset.Damage.TriggerDamageInterval, TEXT("Damage.TriggerDamageInterval"));
		}

		ValidateGameplayEffectConfig(Context, Result, SkillDataAsset.GameplayEffect, TEXT("GameplayEffect"));

		if (SkillDataAsset.SelfBuff.bEnabled)
		{
			ValidateFinite(Context, Result, SkillDataAsset.SelfBuff.Magnitude, TEXT("SelfBuff.Magnitude"));
			ValidateFinite(Context, Result, SkillDataAsset.SelfBuff.WeaponDamageBonus, TEXT("SelfBuff.WeaponDamageBonus"));
			ValidatePositive(Context, Result, SkillDataAsset.SelfBuff.CharacterScaleMultiplier, TEXT("SelfBuff.CharacterScaleMultiplier"));
			ValidatePositive(Context, Result, SkillDataAsset.SelfBuff.WeaponTraceEndZMultiplier, TEXT("SelfBuff.WeaponTraceEndZMultiplier"));
			if (SkillDataAsset.SelfBuff.GameplayEffectClass
				&& FMath::Abs(SkillDataAsset.SelfBuff.Magnitude) > KINDA_SMALL_NUMBER
				&& !SkillDataAsset.SelfBuff.MagnitudeDataTag.IsValid())
			{
				Context.AddWarning(NSLOCTEXT(
					"SkillDataAsset",
					"SelfBuffMissingMagnitudeTag",
					"SelfBuff has a non-zero SetByCaller magnitude, but MagnitudeDataTag is empty."));
			}
		}

		if (SkillDataAsset.StatusEffectDataAsset.Get()
			&& (!FMath::IsFinite(SkillDataAsset.StatusEffectLevel) || SkillDataAsset.StatusEffectLevel < 1.0f))
		{
			MarkSkillTypeInvalid(Context, Result, NSLOCTEXT("SkillDataAsset", "InvalidStatusEffectLevel", "StatusEffectLevel must be at least 1 when StatusEffectDataAsset is set."));
		}

		if (SkillDataAsset.Overlay.bUseCharacterOverlay && !SkillDataAsset.Overlay.CharacterOverlayMaterial.Get())
		{
			Context.AddWarning(NSLOCTEXT("SkillDataAsset", "OverlayMissingMaterial", "Overlay is enabled, but CharacterOverlayMaterial is empty."));
		}

		if (SkillDataAsset.CharacterDecal.bSpawnAtCharacterLocation)
		{
			if (!SkillDataAsset.CharacterDecal.DecalMaterial.Get())
			{
				Context.AddWarning(NSLOCTEXT("SkillDataAsset", "CharacterDecalMissingMaterial", "CharacterDecal is enabled, but DecalMaterial is empty."));
			}
			ValidateNonNegative(Context, Result, SkillDataAsset.CharacterDecal.DecalSize, TEXT("CharacterDecal.DecalSize"));
			if (SkillDataAsset.CharacterDecal.bGrowDecalSize)
			{
				ValidateNonNegative(Context, Result, SkillDataAsset.CharacterDecal.FinalDecalSize, TEXT("CharacterDecal.FinalDecalSize"));
			}
		}

		ValidateVector(Context, Result, SkillDataAsset.Niagara.AuraLocationOffset, TEXT("Niagara.AuraLocationOffset"));
		ValidateVector(Context, Result, SkillDataAsset.Niagara.AuraScale, TEXT("Niagara.AuraScale"));
		ValidateVector(Context, Result, SkillDataAsset.Niagara.SocketLocationOffset, TEXT("Niagara.SocketLocationOffset"));
		ValidateRotator(Context, Result, SkillDataAsset.Niagara.SocketRotationOffset, TEXT("Niagara.SocketRotationOffset"));
		ValidateVector(Context, Result, SkillDataAsset.Niagara.SocketScale, TEXT("Niagara.SocketScale"));
		ValidateNonNegative(Context, Result, SkillDataAsset.Niagara.AutoTargetSearchRadius, TEXT("Niagara.AutoTargetSearchRadius"));
		ValidateNonNegative(Context, Result, SkillDataAsset.Niagara.EffectRadius, TEXT("Niagara.EffectRadius"));
		ValidateNonNegative(Context, Result, SkillDataAsset.Niagara.EffectStartDelay, TEXT("Niagara.EffectStartDelay"));
		ValidateNonNegative(Context, Result, SkillDataAsset.Niagara.EffectDuration, TEXT("Niagara.EffectDuration"));
		if (SkillDataAsset.Niagara.EffectDuration > 0.0)
		{
			ValidatePositive(Context, Result, SkillDataAsset.Niagara.EffectInterval, TEXT("Niagara.EffectInterval"));
		}
		ValidateGameplayEffectConfig(Context, Result, SkillDataAsset.Niagara.GameplayEffect, TEXT("Niagara.GameplayEffect"));

		if (SkillDataAsset.SkillType == EPdSkillType::Active
			|| SkillDataAsset.SkillType == EPdSkillType::Passive
			|| SkillDataAsset.SkillType == EPdSkillType::Triggered)
		{
			Context.AddWarning(NSLOCTEXT(
				"SkillDataAsset",
				"DeprecatedSkillType",
				"SkillType uses a deprecated hidden value. It will be normalized by PostLoad/PostEdit."));
		}
	}

	void ValidateProjectileSkillData(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const USkillDefinition& SkillDataAsset)
	{
		const FSkillProjectileSettings& Settings = SkillDataAsset.ProjectileSettings;
		const bool bResolvedProjectile = SkillDataAsset.GetResolvedSkillDataType() == EPdSkillDataType::Projectile;
		if (!bResolvedProjectile)
		{
			return;
		}

		if (!Settings.ProjectileActorClass)
		{
			MarkSkillTypeInvalid(Context, Result, NSLOCTEXT("SkillDataAsset", "ProjectileMissingActorClass", "ProjectileSettings.ProjectileActorClass is required for projectile skills."));
		}

		ValidateNonNegative(Context, Result, Settings.ProjectileSocketFireInterval, TEXT("ProjectileSettings.ProjectileSocketFireInterval"));
		ValidatePositive(Context, Result, Settings.ProjectileSpeed, TEXT("ProjectileSettings.ProjectileSpeed"));
		ValidateNonNegative(Context, Result, Settings.ProjectileRadius, TEXT("ProjectileSettings.ProjectileRadius"));
		ValidateVector(Context, Result, Settings.SpawnLocationOffset, TEXT("ProjectileSettings.SpawnLocationOffset"));
		ValidateNonNegative(Context, Result, Settings.MinimumForwardSpawnOffset, TEXT("ProjectileSettings.MinimumForwardSpawnOffset"));
		ValidateNonNegative(Context, Result, Settings.TargetTraceMaxRange, TEXT("ProjectileSettings.TargetTraceMaxRange"));
		ValidateNonNegative(Context, Result, Settings.MinimumTargetDistanceFromSpawn, TEXT("ProjectileSettings.MinimumTargetDistanceFromSpawn"));
		ValidateNonNegative(Context, Result, Settings.TargetGroundTraceStartHeight, TEXT("ProjectileSettings.TargetGroundTraceStartHeight"));
		ValidatePositive(Context, Result, Settings.TargetGroundTraceDepth, TEXT("ProjectileSettings.TargetGroundTraceDepth"));
		ValidateGameplayEffectConfig(Context, Result, Settings.Damage, TEXT("ProjectileSettings.Damage"));

		if (Settings.FireMode == EPdSkillProjectileFireMode::HoldThenConfirm
			&& SkillDataAsset.Animation.PrimaryMontage
			&& !Settings.FireEventTag.IsValid()
			&& !SkillDataAsset.Animation.PrimaryEventTag.IsValid())
		{
			Context.AddWarning(NSLOCTEXT(
				"SkillDataAsset",
				"ProjectileMissingFireEventTag",
				"HoldThenConfirm uses a PrimaryMontage, but both ProjectileSettings.FireEventTag and Animation.PrimaryEventTag are empty."));
		}

		if (Settings.bGrowProjectileSize)
		{
			ValidateVector(Context, Result, Settings.ProjectileStartScale, TEXT("ProjectileSettings.ProjectileStartScale"));
			ValidateVector(Context, Result, Settings.ProjectileFinalScale, TEXT("ProjectileSettings.ProjectileFinalScale"));
			ValidateNonNegative(Context, Result, Settings.ProjectileScaleDuration, TEXT("ProjectileSettings.ProjectileScaleDuration"));
			ValidateVector2D(Context, Result, Settings.GrowthUserParameterFinalValue, TEXT("ProjectileSettings.GrowthUserParameterFinalValue"));
		}

		ValidateNonNegative(Context, Result, Settings.ProjectileArcHeight, TEXT("ProjectileSettings.ProjectileArcHeight"));
		ValidateNonNegative(Context, Result, Settings.ProjectileArcGravityScale, TEXT("ProjectileSettings.ProjectileArcGravityScale"));
		if (Settings.bStickOnImpact)
		{
			ValidatePositive(Context, Result, Settings.PostImpactLifeSpan, TEXT("ProjectileSettings.PostImpactLifeSpan"));
		}
		ValidateNonNegative(Context, Result, Settings.TargetDecalSize, TEXT("ProjectileSettings.TargetDecalSize"));
		if (Settings.bGrowTargetDecalSize)
		{
			ValidateNonNegative(Context, Result, Settings.TargetDecalFinalSize, TEXT("ProjectileSettings.TargetDecalFinalSize"));
		}

		if (Settings.bUseGroundTargeting)
		{
			ValidatePositive(Context, Result, Settings.GroundTargetingMaxRange, TEXT("ProjectileSettings.GroundTargetingMaxRange"));
			ValidateNonNegative(Context, Result, Settings.GroundTargetingCollisionRadius, TEXT("ProjectileSettings.GroundTargetingCollisionRadius"));
			ValidateNonNegative(Context, Result, Settings.GroundTargetingCollisionHeight, TEXT("ProjectileSettings.GroundTargetingCollisionHeight"));
			ValidateLinearColor(Context, Result, Settings.TargetDecalColor, TEXT("ProjectileSettings.TargetDecalColor"));

			if (!Settings.GroundTargetActorClass)
			{
				MarkSkillTypeInvalid(Context, Result, NSLOCTEXT(
					"SkillDataAsset",
					"ProjectileMissingGroundTargetActorClass",
					"ProjectileSettings.GroundTargetActorClass is required when ground targeting is enabled."));
			}

			if (!Settings.TargetDecal)
			{
				Context.AddWarning(NSLOCTEXT(
					"SkillDataAsset",
					"ProjectileTargetDecalMissingMaterial",
					"Projectile ground targeting is enabled, but TargetDecal is empty."));
			}

			if (Settings.GroundTargetingTraceProfile.Name.IsNone()
				|| Settings.GroundTargetingTraceProfile.Name == TEXT("NoCollision"))
			{
				MarkSkillTypeInvalid(Context, Result, NSLOCTEXT(
					"SkillDataAsset",
					"ProjectileInvalidGroundTargetingTraceProfile",
					"ProjectileSettings.GroundTargetingTraceProfile must use a blocking collision profile for server line-of-sight validation."));
			}
		}

		if (Settings.StatusEffectClass && (!FMath::IsFinite(Settings.StatusEffectLevel) || Settings.StatusEffectLevel < 1.0f))
		{
			MarkSkillTypeInvalid(Context, Result, NSLOCTEXT("SkillDataAsset", "ProjectileInvalidStatusEffectLevel", "ProjectileSettings.StatusEffectLevel must be at least 1 when StatusEffectClass is set."));
		}
	}

	void ValidateAreaSkillData(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const USkillDefinition& SkillDataAsset)
	{
		if (SkillDataAsset.GetResolvedSkillDataType() != EPdSkillDataType::Area)
		{
			return;
		}

		if (!SkillDataAsset.AOETargetActorClass)
		{
			MarkSkillTypeInvalid(Context, Result, NSLOCTEXT("SkillDataAsset", "AreaMissingTargetActorClass", "AOETargetActorClass is required for area skills."));
		}

		ValidatePositive(Context, Result, SkillDataAsset.AOETargetingMaxRange, TEXT("AOETargetingMaxRange"));
		ValidateNonNegative(Context, Result, SkillDataAsset.AOETargetingCollisionRadius, TEXT("AOETargetingCollisionRadius"));
		ValidateNonNegative(Context, Result, SkillDataAsset.AOETargetingCollisionHeight, TEXT("AOETargetingCollisionHeight"));
		ValidateLinearColor(Context, Result, SkillDataAsset.AOETargetingDecalColor, TEXT("AOETargetingDecalColor"));
		ValidatePositive(Context, Result, SkillDataAsset.AOETargetGroundTraceDepth, TEXT("AOETargetGroundTraceDepth"));
		ValidatePositive(Context, Result, SkillDataAsset.AOERadius, TEXT("AOERadius"));
		ValidateNonNegative(Context, Result, SkillDataAsset.AOELightningDamageDelay, TEXT("AOELightningDamageDelay"));

		if (SkillDataAsset.AOETargetingTraceProfileName.IsNone()
			|| SkillDataAsset.AOETargetingTraceProfileName == TEXT("NoCollision"))
		{
			MarkSkillTypeInvalid(Context, Result, NSLOCTEXT(
				"SkillDataAsset",
				"AreaInvalidTargetingTraceProfile",
				"AOETargetingTraceProfileName must use a blocking collision profile for server line-of-sight validation."));
		}

		if (SkillDataAsset.bAOEDrawDebugDamageRadius)
		{
			ValidateNonNegative(Context, Result, SkillDataAsset.AOEDebugDamageRadiusDrawTime, TEXT("AOEDebugDamageRadiusDrawTime"));
		}

		if (SkillDataAsset.bUseAOECameraSettings)
		{
			ValidateSkillAimCameraSettings(Context, Result, SkillDataAsset.AOECameraSettings, TEXT("AOECameraSettings"));
		}
	}

	void ValidateDashSkillData(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const USkillDefinition& SkillDataAsset)
	{
		if (SkillDataAsset.GetResolvedSkillDataType() != EPdSkillDataType::Dash && !SkillDataAsset.Movement.bUseOneShotDash)
		{
			return;
		}

		ValidatePositive(Context, Result, SkillDataAsset.Movement.DashStrength, TEXT("Movement.DashStrength"));
		ValidatePositive(Context, Result, SkillDataAsset.Movement.DashDuration, TEXT("Movement.DashDuration"));

		if (SkillDataAsset.Movement.bDamageEnemiesOnContact)
		{
			ValidateGameplayEffectConfig(Context, Result, SkillDataAsset.Movement.ContactDamage, TEXT("Movement.ContactDamage"));
		}
	}

	void ValidateAuraSkillData(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const USkillDefinition& SkillDataAsset)
	{
		if (SkillDataAsset.GetResolvedSkillDataType() != EPdSkillDataType::Aura)
		{
			return;
		}

		if (SkillDataAsset.Niagara.SocketNiagaraSystem.Get()
			&& SkillDataAsset.Niagara.SocketName.IsNone()
			&& !SkillDataAsset.Niagara.bSpawnSocketNiagaraAtCharacterLocation)
		{
			Context.AddWarning(NSLOCTEXT("SkillDataAsset", "AuraSocketNiagaraMissingSocket", "SocketNiagaraSystem is set, but SocketName is None and bSpawnSocketNiagaraAtCharacterLocation is false."));
		}

		if (SkillDataAsset.Movement.bOverrideMovementSpeedWhileActive)
		{
			ValidateNonNegative(Context, Result, SkillDataAsset.Movement.DashStrength, TEXT("Movement.DashStrength"));
		}

		if (SkillDataAsset.Heal.bEnabled)
		{
			if (!SkillDataAsset.Heal.TeamHealEffect.GameplayEffectClass)
			{
				Context.AddWarning(NSLOCTEXT("SkillDataAsset", "HealMissingEffectClass", "Heal is enabled, but TeamHealEffect.GameplayEffectClass is empty."));
			}
			ValidateGameplayEffectConfig(Context, Result, SkillDataAsset.Heal.TeamHealEffect, TEXT("Heal.TeamHealEffect"));
			ValidateNonNegative(Context, Result, SkillDataAsset.Heal.HealRadius, TEXT("Heal.HealRadius"));
			ValidatePositive(Context, Result, SkillDataAsset.Heal.HealInterval, TEXT("Heal.HealInterval"));
		}
	}

	void ValidateTrailSkillData(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const USkillDefinition& SkillDataAsset)
	{
		if (SkillDataAsset.GetResolvedSkillDataType() != EPdSkillDataType::Trail && !SkillDataAsset.SwordTrail.bEnabled)
		{
			return;
		}

		ValidatePositive(Context, Result, SkillDataAsset.SwordTrail.TrailEndZLengthMultiplier, TEXT("SwordTrail.TrailEndZLengthMultiplier"));
		ValidateTransform(Context, Result, SkillDataAsset.SwordTrail.SlashTransformOffset, TEXT("SwordTrail.SlashTransformOffset"));
	}

	void ValidateMissileSkillData(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const USkillDefinition& SkillDataAsset)
	{
		if (SkillDataAsset.GetResolvedSkillDataType() != EPdSkillDataType::Missile)
		{
			return;
		}

		if (!SkillDataAsset.Missile.TargetActorClass)
		{
			MarkSkillTypeInvalid(Context, Result, NSLOCTEXT("SkillDataAsset", "MissileMissingTargetActorClass", "Missile.TargetActorClass is required for missile skills."));
		}

		ValidateVector(Context, Result, SkillDataAsset.Missile.NiagaraSpawnLocationOffset, TEXT("Missile.NiagaraSpawnLocationOffset"));
		ValidateRotator(Context, Result, SkillDataAsset.Missile.NiagaraSpawnRotationOffset, TEXT("Missile.NiagaraSpawnRotationOffset"));
		ValidateVector(Context, Result, SkillDataAsset.Missile.NiagaraScale, TEXT("Missile.NiagaraScale"));
		ValidateNonNegative(Context, Result, SkillDataAsset.Missile.MissileDuration, TEXT("Missile.MissileDuration"));
		ValidateNonNegative(Context, Result, SkillDataAsset.Missile.AutoTargetSearchRadius, TEXT("Missile.AutoTargetSearchRadius"));
		ValidateFinite(Context, Result, SkillDataAsset.Missile.AutoTargetForwardOffset, TEXT("Missile.AutoTargetForwardOffset"));
		ValidatePositive(Context, Result, SkillDataAsset.Missile.TargetingMaxRange, TEXT("Missile.TargetingMaxRange"));
		ValidateNonNegative(Context, Result, SkillDataAsset.Missile.TargetingCollisionRadius, TEXT("Missile.TargetingCollisionRadius"));
		ValidateNonNegative(Context, Result, SkillDataAsset.Missile.TargetingCollisionHeight, TEXT("Missile.TargetingCollisionHeight"));
		ValidateNonNegative(Context, Result, SkillDataAsset.Missile.DamageStartDelay, TEXT("Missile.DamageStartDelay"));
		ValidateNonNegative(Context, Result, SkillDataAsset.Missile.DamageApplicationDuration, TEXT("Missile.DamageApplicationDuration"));
		ValidatePositive(Context, Result, SkillDataAsset.Missile.DamageInterval, TEXT("Missile.DamageInterval"));
		ValidateNonNegative(Context, Result, SkillDataAsset.Missile.DamageRadius, TEXT("Missile.DamageRadius"));
		if (SkillDataAsset.Missile.bDrawDebugDamageRadius)
		{
			ValidateNonNegative(Context, Result, SkillDataAsset.Missile.DebugDamageRadiusDrawTime, TEXT("Missile.DebugDamageRadiusDrawTime"));
		}
	}

	void ValidateSummonSkillData(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const USkillDefinition& SkillDataAsset)
	{
		if (SkillDataAsset.GetResolvedSkillDataType() != EPdSkillDataType::Summon && !SkillDataAsset.SummonSettings.bEnabled)
		{
			return;
		}

		if (!SkillDataAsset.SummonSettings.SummonedActorClass)
		{
			MarkSkillTypeInvalid(Context, Result, NSLOCTEXT("SkillDataAsset", "SummonMissingActorClass", "SummonSettings.SummonedActorClass is required for summon skills."));
		}

		ValidateNonNegative(Context, Result, SkillDataAsset.SummonSettings.SpawnForwardDistance, TEXT("SummonSettings.SpawnForwardDistance"));
		ValidateNonNegative(Context, Result, SkillDataAsset.SummonSettings.MinimumReplicatedActorLifetime, TEXT("SummonSettings.MinimumReplicatedActorLifetime"));
		ValidateVector(Context, Result, SkillDataAsset.SummonSettings.SpawnLocationOffset, TEXT("SummonSettings.SpawnLocationOffset"));
		ValidateRotator(Context, Result, SkillDataAsset.SummonSettings.SpawnRotationOffset, TEXT("SummonSettings.SpawnRotationOffset"));
		if (SkillDataAsset.SummonSettings.bRiseFromUnderground)
		{
			ValidateNonNegative(Context, Result, SkillDataAsset.SummonSettings.RiseDistanceBelowGround, TEXT("SummonSettings.RiseDistanceBelowGround"));
			ValidatePositive(Context, Result, SkillDataAsset.SummonSettings.RiseSpeed, TEXT("SummonSettings.RiseSpeed"));
			ValidateVector(Context, Result, SkillDataAsset.SummonSettings.FinalLocationOffset, TEXT("SummonSettings.FinalLocationOffset"));
		}

		if (SkillDataAsset.SummonSettings.bShowDecalWhileSummoning)
		{
			if (!SkillDataAsset.SummonSettings.SummonDecalMaterial.Get())
			{
				Context.AddWarning(NSLOCTEXT("SkillDataAsset", "SummonDecalMissingMaterial", "Summon decal is enabled, but SummonDecalMaterial is empty."));
			}
			ValidateNonNegative(Context, Result, SkillDataAsset.SummonSettings.SummonDecalSize, TEXT("SummonSettings.SummonDecalSize"));
		}

		ValidateGameplayEffectConfig(Context, Result, SkillDataAsset.SummonSettings.TriggerDamage, TEXT("SummonSettings.TriggerDamage"));
		ValidateNonNegative(Context, Result, SkillDataAsset.SummonSettings.TriggerDamageDelay, TEXT("SummonSettings.TriggerDamageDelay"));
	}

	void ValidateStaticSkillData(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const USkillDefinition& SkillDataAsset)
	{
		if (SkillDataAsset.GetResolvedSkillDataType() != EPdSkillDataType::Static && !SkillDataAsset.StaticSettings.bEnabled)
		{
			return;
		}

		if (!SkillDataAsset.StaticSettings.StaticActorClass)
		{
			MarkSkillTypeInvalid(Context, Result, NSLOCTEXT("SkillDataAsset", "StaticMissingActorClass", "StaticSettings.StaticActorClass is required for static skills."));
		}

		if (SkillDataAsset.StaticSettings.bUseSpawnSockets && SkillDataAsset.StaticSettings.SpawnSocketNames.Num() != 6)
		{
			Context.AddWarning(NSLOCTEXT("SkillDataAsset", "StaticSocketCountMismatch", "StaticSettings.SpawnSocketNames should contain exactly 6 entries. Runtime sync will resize it."));
		}

		ValidateNonNegative(Context, Result, SkillDataAsset.StaticSettings.SpawnInterval, TEXT("StaticSettings.SpawnInterval"));
		if (SkillDataAsset.StaticSettings.bRepeatSpawnSequence)
		{
			ValidatePositive(Context, Result, SkillDataAsset.StaticSettings.RepeatSpawnInterval, TEXT("StaticSettings.RepeatSpawnInterval"));
		}
		ValidateVector(Context, Result, SkillDataAsset.StaticSettings.SpawnLocationOffset, TEXT("StaticSettings.SpawnLocationOffset"));
		ValidateRotator(Context, Result, SkillDataAsset.StaticSettings.SpawnRotationOffset, TEXT("StaticSettings.SpawnRotationOffset"));
		if (SkillDataAsset.StaticSettings.bProjectSpawnToGround)
		{
			ValidateNonNegative(Context, Result, SkillDataAsset.StaticSettings.GroundTraceStartHeight, TEXT("StaticSettings.GroundTraceStartHeight"));
			ValidatePositive(Context, Result, SkillDataAsset.StaticSettings.GroundTraceDepth, TEXT("StaticSettings.GroundTraceDepth"));
		}
		if (SkillDataAsset.StaticSettings.bUseSpawnedActorLifeSpan)
		{
			ValidateNonNegative(Context, Result, SkillDataAsset.StaticSettings.SpawnedActorLifeSpan, TEXT("StaticSettings.SpawnedActorLifeSpan"));
		}
		ValidateNonNegative(Context, Result, SkillDataAsset.StaticSettings.MinimumReplicatedActorLifetime, TEXT("StaticSettings.MinimumReplicatedActorLifetime"));
		ValidateNonNegative(Context, Result, SkillDataAsset.StaticSettings.TriggerActiveDurationAfterLastSpawn, TEXT("StaticSettings.TriggerActiveDurationAfterLastSpawn"));
		ValidateGameplayEffectConfig(Context, Result, SkillDataAsset.StaticSettings.TriggerDamage, TEXT("StaticSettings.TriggerDamage"));
		if (SkillDataAsset.StaticSettings.bRepeatTriggerDamageWhileOverlapping)
		{
			ValidatePositive(Context, Result, SkillDataAsset.StaticSettings.TriggerDamageInterval, TEXT("StaticSettings.TriggerDamageInterval"));
		}
		if (SkillDataAsset.StaticSettings.bOmenOrbPullEnemiesDuringGrowth)
		{
			ValidateNonNegative(Context, Result, SkillDataAsset.StaticSettings.OmenOrbPullRadius, TEXT("StaticSettings.OmenOrbPullRadius"));
			ValidateNonNegative(Context, Result, SkillDataAsset.StaticSettings.OmenOrbPullSpeed, TEXT("StaticSettings.OmenOrbPullSpeed"));
		}
		if (SkillDataAsset.StaticSettings.bOmenOrbApplyFinishAreaDamage)
		{
			ValidatePositive(Context, Result, SkillDataAsset.StaticSettings.OmenOrbFinishDamageRadius, TEXT("StaticSettings.OmenOrbFinishDamageRadius"));
		}
	}

#endif
}

USkillDefinition::USkillDefinition()
{
	NormalizeSkillTypeForDataType(this);

	AOEIndicatorCueTag = LabGameplayTags::GameplayCue_AOEIndicator;
	AOELightningBoltCueTag = LabGameplayTags::GameplayCue_LightningBolt;
	AOETargetActorClass = ATargetActor_GroundTrace_Decal::StaticClass();
	AOETargetingMaxRange = 3000.0;
	AOERadius = 256.0;
	AOECameraSettings.TargetFOV = 70.0f;
	AOECameraSettings.TargetBoomSocketOffset = FVector(80.0f, 320.0f, 100.0f);
	AOECameraSettings.TargetCameraRotation = FRotator::ZeroRotator;
	AOECameraSettings.InterpSpeed = 10.0f;
	Aura.TeamHealMagnitudeDataTag = LabGameplayTags::Data_Heal;

	Dash.Strength = 2000.0;
	Dash.Duration = 0.3;
	Movement.DashGameplayCueTag = LabGameplayTags::GameplayCue_Dash_Active;

	Default.Animation.PrimaryEventTag = LabGameplayTags::Event_Montage_Trigger;
	Missile.Animation.PrimaryEventTag = LabGameplayTags::Event_Montage_Trigger;
	Missile.Damage.DamageDataTag = LabGameplayTags::Data_Damage;
	Missile.TargetActorClass = AGameplayAbilityTargetActor_GroundTrace::StaticClass();
	Summon.Animation.PrimaryEventTag = LabGameplayTags::Event_Montage_Trigger;
	ProjectileSettings.FireEventTag = LabGameplayTags::Event_ShootProjectile;
	ProjectileSettings.ProjectileActorClass = AProjectileBase::StaticClass();
	ProjectileSettings.ProjectileSpeed = 2000.0;
	ProjectileSettings.ProjectileRadius = 50.0;
	ProjectileSettings.SpawnLocationOffset = FVector(0.0, 0.0, 80.0);
	ProjectileSettings.MinimumForwardSpawnOffset = 140.0;
	ProjectileSettings.TargetTraceMaxRange = 999999.0;
	ProjectileSettings.TargetDecalSize = 512.0;
	ProjectileSettings.GroundTargetActorClass = ATargetActor_GroundTrace_Decal::StaticClass();
	ProjectileSettings.Damage.MagnitudeDataTag = LabGameplayTags::Data_Damage;
	StaticSettings.SpawnSocketNames.SetNum(6);
	StaticSettings.TriggerDamage.MagnitudeDataTag = LabGameplayTags::Data_Damage;
	Heal.TeamHealEffect.MagnitudeDataTag = LabGameplayTags::Data_Heal;
	Damage.MagnitudeDataTag = LabGameplayTags::Data_Damage;
	GameplayEffect.MagnitudeDataTag = LabGameplayTags::Data_Damage;
	Niagara.GameplayEffect.MagnitudeDataTag = LabGameplayTags::Data_Damage;
	SummonSettings.TriggerDamage.MagnitudeDataTag = LabGameplayTags::Data_Damage;
	Movement.ContactDamage.MagnitudeDataTag = LabGameplayTags::Data_Damage;
}

void USkillDefinition::PostLoad()
{
	Super::PostLoad();

	MigrateLegacyProjectileSettings();
	NormalizeSkillTypeForDataType(this);
	EnsureAreaAnimationDefaults(this);
	SyncTopLevelDamageToRuntimeConfig();
	ApplyCurrentSettingsToRuntimeConfig();
}

void USkillDefinition::PreSave(FObjectPreSaveContext SaveContext)
{
	// Newly created assets do not pass through PostLoad before their first save.
	// Persist the current schema explicitly so legacy migration remains one-shot.
	ProjectileSettingsVersion = CurrentProjectileSettingsVersion;

#if WITH_EDITOR
	if (SaveContext.IsCooking() && HasEnabledDebugDrawingFlags())
	{
		uint32 SuppressedCount = 0;
		FPdLogRateLimiter& LogLimiter =
			CookValidationLogLimiters.FindOrAdd(FName(*GetPathName()));
		if (LogLimiter.TryAcquire(CookValidationLogIntervalSeconds, SuppressedCount))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("Skill data asset '%s' has per-asset debug drawing enabled. "
					"Clear all skill debug flags before cooking. SuppressedSinceLast=%u"),
				*GetPathName(),
				SuppressedCount);
		}
	}
#endif

	Super::PreSave(SaveContext);
}

#if WITH_EDITOR
void USkillDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	NormalizeSkillTypeForDataType(this);
	EnsureAreaAnimationDefaults(this);
	ProjectileSettingsVersion = CurrentProjectileSettingsVersion;
	SyncTopLevelDamageToRuntimeConfig();
	ApplyCurrentSettingsToRuntimeConfig();
}

EDataValidationResult USkillDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	ValidateCommonSkillData(Context, Result, *this);
	ValidateProjectileSkillData(Context, Result, *this);
	ValidateAreaSkillData(Context, Result, *this);
	ValidateDashSkillData(Context, Result, *this);
	ValidateAuraSkillData(Context, Result, *this);
	ValidateTrailSkillData(Context, Result, *this);
	ValidateMissileSkillData(Context, Result, *this);
	ValidateSummonSkillData(Context, Result, *this);
	ValidateStaticSkillData(Context, Result, *this);
	ValidateUniqueSkillContentNames(Context, Result, *this);
	if (HasEnabledDebugDrawingFlags())
	{
		MarkSkillTypeInvalid(Context, Result, NSLOCTEXT(
			"SkillDataAsset",
			"PerAssetDebugDrawingEnabled",
			"Per-asset skill debug drawing flags must be disabled before validation or cooking. Use lab.Skill.DebugDraw for temporary development visualization."));
	}

	return Result;
}
#endif

FPrimaryAssetId USkillDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Skill"), GetFName());
}

FText USkillDefinition::GetDisplayName() const
{
	return !Name.IsNone() ? FText::FromName(Name) : FText::FromName(GetFName());
}

UObject* USkillDefinition::GetIconResource() const
{
	return Icon.Get();
}

EPdSkillDataType USkillDefinition::GetResolvedSkillDataType() const
{
	if (StaticSettings.bEnabled)
	{
		return EPdSkillDataType::Static;
	}

	return SkillDataType;
}

FSkillGameplayEffectConfig USkillDefinition::GetResolvedDamageConfig() const
{
	FSkillGameplayEffectConfig ResolvedDamage = Damage.ToGameplayEffectConfig();
	if (!ResolvedDamage.MagnitudeDataTag.IsValid())
	{
		ResolvedDamage.MagnitudeDataTag = LabGameplayTags::Data_Damage;
	}
	return ResolvedDamage;
}

FSkillGameplayEffectConfig USkillDefinition::GetResolvedStaticFinishDamageConfig() const
{
	FSkillGameplayEffectConfig ResolvedDamage = Damage.ToGameplayEffectConfig();
	if (!ResolvedDamage.MagnitudeDataTag.IsValid())
	{
		ResolvedDamage.MagnitudeDataTag = LabGameplayTags::Data_Damage;
	}
	return ResolvedDamage;
}

const FShieldSkillConfig* USkillDefinition::GetDefensiveSkillConfig() const
{
	return GetResolvedSkillDataType() == EPdSkillDataType::Default ? &Default : nullptr;
}

bool USkillDefinition::HasEnabledDebugDrawingFlags() const
{
	return ProjectileSettings.bDrawTargetTraceDebug
		|| ProjectileSettings.bDrawGroundTargetingDebug
		|| bAOEDebugTargeting
		|| bAOEDrawDebugDamageRadius
		|| Missile.bDebugTargeting
		|| Missile.bDrawDebugDamageRadius;
}

void USkillDefinition::MigrateLegacyProjectileSettings()
{
	if (ProjectileSettingsVersion >= CurrentProjectileSettingsVersion)
	{
		return;
	}

	if (GetResolvedSkillDataType() != EPdSkillDataType::Projectile)
	{
		ProjectileSettingsVersion = CurrentProjectileSettingsVersion;
		Projectile_DEPRECATED = FProjectileSkillConfig();
		return;
	}

	if (ProjectileSettingsVersion < LegacyProjectileStructMigrationVersion)
	{
		const bool bLegacyIsPrimary = !ProjectileSettings.bEnabled;
		if (bLegacyIsPrimary)
		{
			ProjectileSettings.bEnabled = true;
			ProjectileSettings.ProjectileActorClass = Projectile_DEPRECATED.ProjectileClass;
			ProjectileSettings.ProjectileSpeed = Projectile_DEPRECATED.ProjectileSpeed;
			ProjectileSettings.ProjectileRadius = Projectile_DEPRECATED.ProjectileRadius;
			ProjectileSettings.SpawnLocationOffset = Projectile_DEPRECATED.SpawnLocationOffset;
			ProjectileSettings.MinimumForwardSpawnOffset = Projectile_DEPRECATED.MinimumForwardSpawnOffset;
			ProjectileSettings.TargetTraceMaxRange = Projectile_DEPRECATED.TargetTraceMaxRange;
			ProjectileSettings.MuzzleNiagaraSystem = Projectile_DEPRECATED.MuzzleFX;
			ProjectileSettings.ProjectileNiagaraSystem = Projectile_DEPRECATED.ProjectileFX;
			ProjectileSettings.HitNiagaraSystem = Projectile_DEPRECATED.HitFX;
			ProjectileSettings.bSpawnHitNiagaraOnGround = Projectile_DEPRECATED.bSpawnHitNiagaraOnGround;
			ProjectileSettings.bGrowProjectileSize = Projectile_DEPRECATED.bEnableReadiedProjectileChargeGrowth;
			ProjectileSettings.ProjectileStartScale = Projectile_DEPRECATED.ReadiedProjectileStartScale;
			ProjectileSettings.ProjectileFinalScale = Projectile_DEPRECATED.ReadiedProjectileTargetScale;
			ProjectileSettings.ProjectileScaleDuration = Projectile_DEPRECATED.ReadiedProjectileScaleDuration;
			ProjectileSettings.GrowthUserParameterName =
				Projectile_DEPRECATED.ReadiedProjectileNiagaraVector2DParameterName;
			ProjectileSettings.GrowthUserParameterFinalValue =
				Projectile_DEPRECATED.ReadiedProjectileNiagaraTargetSize;
			ProjectileSettings.TargetDecal = Projectile_DEPRECATED.GroundTargetingDecal;
			ProjectileSettings.TargetDecalSize = Projectile_DEPRECATED.GroundTargetingDecalSize;
			ProjectileSettings.StatusEffectClass = Projectile_DEPRECATED.Damage.StatusEffectClass;
			ProjectileSettings.StatusEffectLevel =
				FMath::Max(Projectile_DEPRECATED.Damage.StatusEffectLevel, 1.0f);

			ProjectileSettings.ProjectileSocketNames.SetNum(6);
			if (!Projectile_DEPRECATED.SpawnSocketName.IsNone())
			{
				ProjectileSettings.ProjectileSocketNames[0] = Projectile_DEPRECATED.SpawnSocketName;
			}

			if (!ProjectileSettings.FireEventTag.IsValid())
			{
				ProjectileSettings.FireEventTag = Projectile_DEPRECATED.Animation.PrimaryEventTag;
			}

			if (!Damage.GameplayEffectClass && Projectile_DEPRECATED.Damage.DamageEffectClass)
			{
				Damage.GameplayEffectClass = Projectile_DEPRECATED.Damage.DamageEffectClass;
				Damage.MagnitudeDataTag = Projectile_DEPRECATED.Damage.DamageDataTag;
				Damage.Magnitude = Projectile_DEPRECATED.Damage.DamageMagnitude;
			}
		}

		// These settings existed only in the legacy structure. Copy them exactly once
		// regardless of which structure supplied the overlapping fields.
		ProjectileSettings.TargetTraceProfile = Projectile_DEPRECATED.TargetTraceProfile;
		ProjectileSettings.MinimumTargetDistanceFromSpawn =
			Projectile_DEPRECATED.MinimumTargetDistanceFromSpawn;
		ProjectileSettings.bTraceAffectsAimPitch = Projectile_DEPRECATED.bTraceAffectsAimPitch;
		ProjectileSettings.bDrawTargetTraceDebug = Projectile_DEPRECATED.bDrawTargetTraceDebug;
		ProjectileSettings.GrowthUserParameterStartValue =
			Projectile_DEPRECATED.ReadiedProjectileNiagaraStartSize;
		ProjectileSettings.bUseGroundTargeting =
			ProjectileSettings.bUseGroundTargeting
			|| Projectile_DEPRECATED.bUseGroundTargeting
			|| ProjectileSettings.TargetDecal != nullptr
			|| ProjectileSettings.bGrowTargetDecalSize
			|| ProjectileSettings.TargetDecalFinalSize > 0.0;
		ProjectileSettings.GroundTargetActorClass =
			Projectile_DEPRECATED.GroundTargetActorClass
				? Projectile_DEPRECATED.GroundTargetActorClass
				: ProjectileSettings.GroundTargetActorClass;
		ProjectileSettings.GroundTargetingMaxRange = Projectile_DEPRECATED.GroundTargetingMaxRange;
		ProjectileSettings.GroundTargetingTraceProfile =
			Projectile_DEPRECATED.GroundTargetingTraceProfile;
		ProjectileSettings.GroundTargetingCollisionRadius =
			Projectile_DEPRECATED.GroundTargetingCollisionRadius;
		ProjectileSettings.GroundTargetingCollisionHeight =
			Projectile_DEPRECATED.GroundTargetingCollisionHeight;
		ProjectileSettings.bGroundTargetingTraceAffectsAimPitch =
			Projectile_DEPRECATED.bGroundTargetingTraceAffectsAimPitch;
		ProjectileSettings.bDrawGroundTargetingDebug =
			Projectile_DEPRECATED.bDrawGroundTargetingDebug;
		ProjectileSettings.TargetDecalColor = Projectile_DEPRECATED.GroundTargetingDecalColor;

		Projectile_DEPRECATED = FProjectileSkillConfig();
	}

	// Version 2 introduced opt-in post-impact persistence. Preserve the intended
	// behavior of the existing Icicle asset without requiring an asset resave before play.
	static const FName IcicleSkillPackageName(TEXT("/Game/Pandora/Skill/Ice/DA_Skill_Icicle"));
	if (ProjectileSettingsVersion < CurrentProjectileSettingsVersion
		&& GetOutermost()->GetFName() == IcicleSkillPackageName)
	{
		ProjectileSettings.bStickOnImpact = true;
		ProjectileSettings.PostImpactLifeSpan = 1.0;
	}

	ProjectileSettingsVersion = CurrentProjectileSettingsVersion;

#if WITH_EDITOR
	if (!IsTemplate() && !IsRunningCookCommandlet())
	{
		MarkPackageDirty();
	}
#endif
}

void USkillDefinition::ApplyCurrentSettingsToRuntimeConfig()
{
	const EPdSkillDataType ResolvedType = GetResolvedSkillDataType();
	ProjectileSettings.ProjectileSocketNames.SetNum(6);
	StaticSettings.SpawnSocketNames.SetNum(6);

	// These fields are derived compatibility copies. Clear inactive copies before
	// rebuilding the active configuration so stale GameplayEffect references do not
	// survive a skill data type change.
	if (ResolvedType != EPdSkillDataType::Default)
	{
		Default.GameplayEffectClass = nullptr;
	}

	if (ResolvedType != EPdSkillDataType::Default
		&& ResolvedType != EPdSkillDataType::Missile)
	{
		Niagara.GameplayEffect = FSkillGameplayEffectConfig();
		Niagara.GameplayEffect.MagnitudeDataTag = LabGameplayTags::Data_Damage;
	}

	switch (ResolvedType)
	{
	case EPdSkillDataType::Projectile:
		break;

	case EPdSkillDataType::Dash:
		Dash.Animation = Animation;
		Dash.Strength = Movement.DashStrength;
		Dash.Duration = Movement.DashDuration;
		Dash.bEnableGravityDuringDash = Movement.bEnableGravityDuringDash;
		Movement.DashGameplayCueTag = Niagara.GameplayCueTag;
		break;

	case EPdSkillDataType::Aura:
		Aura.Duration = Time.Duration;
		Aura.BodyAuraSystem = Niagara.AuraNiagaraSystem;
		Aura.BodyAuraComponentName = Niagara.AuraNiagaraComponentName;
		Aura.AttachedNiagaraSystem = Niagara.SocketNiagaraSystem;
		Aura.bSpawnAttachedNiagaraAtCharacterLocation = Niagara.bSpawnSocketNiagaraAtCharacterLocation;
		Aura.AttachedNiagaraSocketName = Niagara.SocketName;
		Aura.AttachedNiagaraLocationOffset = Niagara.SocketLocationOffset;
		Aura.AttachedNiagaraRotationOffset = Niagara.SocketRotationOffset;
		Aura.AttachedNiagaraScale = Niagara.SocketScale;
		Aura.bSpawnAttachedNiagaraWhileActive = Niagara.SocketNiagaraSystem != nullptr;
		Aura.bApplyOverlayMaterialWhileActive = Overlay.bUseCharacterOverlay;
		Aura.ActiveOverlayMaterial = Overlay.CharacterOverlayMaterial;
		Aura.bIncreaseMovementSpeedOnActivate = Movement.bOverrideMovementSpeedWhileActive;
		Aura.MovementSpeedIncrease = Movement.DashStrength > 0.0
			? Movement.DashStrength
			: Movement.MovementSpeedWhileActive;
		Movement.MovementSpeedWhileActive = Aura.MovementSpeedIncrease;
		Aura.bHealTeamInInteractionBox = Heal.bEnabled;
		Aura.TeamHealEffectClass = Heal.TeamHealEffect.GameplayEffectClass;
		Aura.TeamHealMagnitudeDataTag = Heal.TeamHealEffect.MagnitudeDataTag;
		Aura.TeamHealMagnitude = Heal.TeamHealEffect.Magnitude;
		Aura.TeamHealInterval = Heal.HealInterval;
		Aura.bHealSelfInInteractionBox = Heal.bHealSelf;
		break;

	case EPdSkillDataType::Trail:
		Trail.Animation = Animation;
		Trail.Duration = Time.Duration;
		Trail.TrailSystem = SwordTrail.TrailNiagaraSystem;
		Trail.SlashSystem = SwordTrail.SlashNiagaraSystem;
		Trail.SlashAttackTraceEndMultiplier = SwordTrail.TrailEndZLengthMultiplier;
		Trail.SlashSpawnTiming = ETrailSlashSpawnTiming::AnimNotify;
		Trail.SlashSpawnLocationOffset = SwordTrail.SlashTransformOffset.GetLocation();
		Trail.SlashSpawnSocketName = SwordTrail.SlashSpawnSocketName;
		Trail.SlashSpawnRotationOffset = SwordTrail.SlashTransformOffset.Rotator();
		Trail.SlashScale = SwordTrail.SlashTransformOffset.GetScale3D();
		break;

	case EPdSkillDataType::Default:
		Default.Animation = Animation;
		Default.GameplayEffectClass = GameplayEffect.GameplayEffectClass;
		Niagara.GameplayEffect = GameplayEffect;
		break;

	case EPdSkillDataType::Missile:
	{
		Missile.Animation = Animation;
		Missile.MissileDuration = Time.Duration;
		Missile.MissileSystem = Niagara.SocketNiagaraSystem;
		Missile.NiagaraSpawnSocketName = Niagara.SocketName;
		Missile.NiagaraSpawnLocationOffset = Niagara.SocketLocationOffset;
		Missile.NiagaraSpawnRotationOffset = Niagara.SocketRotationOffset;
		Missile.NiagaraScale = Niagara.SocketScale;
		Missile.AimPositionParameterName = Niagara.AimPositionParameterName;
		Missile.TargetSocketName = Niagara.TargetSocketName;
		Missile.AutoTargetSearchRadius = Niagara.AutoTargetSearchRadius;
		Missile.DamageRadius = Niagara.EffectRadius;
		Missile.DamageStartDelay = Niagara.EffectStartDelay;
		Missile.DamageApplicationDuration = Niagara.EffectDuration;
		Missile.DamageInterval = Niagara.EffectInterval;
		const FSkillGameplayEffectConfig ResolvedDamage = GetResolvedDamageConfig();
		GameplayEffect = ResolvedDamage;
		Missile.Damage.DamageEffectClass = ResolvedDamage.GameplayEffectClass;
		Missile.Damage.DamageDataTag = ResolvedDamage.MagnitudeDataTag;
		Missile.Damage.DamageMagnitude = ResolvedDamage.Magnitude;
		Niagara.GameplayEffect = ResolvedDamage;
		break;
	}

	case EPdSkillDataType::Summon:
		Summon.Animation = Animation;
		SummonSettings.TriggerDamage = GetResolvedDamageConfig();
		Summon.SummonedActorClass = SummonSettings.SummonedActorClass;
		Summon.MinimumReplicatedActorLifetime = SummonSettings.MinimumReplicatedActorLifetime;
		Summon.SpawnSocketName = SummonSettings.SpawnSocketName;
		Summon.SpawnForwardDistance = SummonSettings.SpawnForwardDistance;
		Summon.SpawnLocationOffset = SummonSettings.SpawnLocationOffset;
		Summon.SpawnRotationOffset = SummonSettings.SpawnRotationOffset;
		Summon.bProjectSpawnToGround = SummonSettings.bProjectSpawnToGround;
		Summon.RiseDistanceBelowGround = SummonSettings.bRiseFromUnderground
			? SummonSettings.RiseDistanceBelowGround
			: 0.0;
		Summon.RiseDuration = SummonSettings.RiseSpeed > 0.0
			? SummonSettings.RiseDistanceBelowGround / SummonSettings.RiseSpeed
			: 0.0;
		Summon.LaserNiagaraComponentName = SummonSettings.LaserNiagaraComponentName;
		break;

	case EPdSkillDataType::Static:
		StaticSettings.TriggerDamage = GetResolvedDamageConfig();
		break;

	case EPdSkillDataType::Area:
	case EPdSkillDataType::ShieldBubble:
	case EPdSkillDataType::FillShield:
		break;
	}
}

bool LabSkillDebug::IsDrawingEnabled()
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	return false;
#else
	return CVarSkillDebugDrawing.GetValueOnAnyThread() != 0;
#endif
}

void USkillDefinition::SyncTopLevelDamageToRuntimeConfig()
{
	if (!Damage.MagnitudeDataTag.IsValid())
	{
		Damage.MagnitudeDataTag = LabGameplayTags::Data_Damage;
	}

	Damage.TriggerDamageInterval = FMath::Max(Damage.TriggerDamageInterval, 0.05);

	const FSkillGameplayEffectConfig ResolvedDamage = Damage.ToGameplayEffectConfig();
	ProjectileSettings.Damage = ResolvedDamage;
	StaticSettings.TriggerDamage = ResolvedDamage;
	StaticSettings.bRepeatTriggerDamageWhileOverlapping = Damage.bRepeatTriggerDamageWhileOverlapping;
	StaticSettings.TriggerDamageInterval = Damage.TriggerDamageInterval;
	SummonSettings.TriggerDamage = ResolvedDamage;
	Movement.ContactDamage = ResolvedDamage;
}

bool USkillDefinition::ShouldShowInAbilitiesBar() const
{
	return bShowInAbilitiesBar
		&& (SkillType == EPdSkillType::Instant
			|| SkillType == EPdSkillType::Press
			|| SkillType == EPdSkillType::Duration);
}
