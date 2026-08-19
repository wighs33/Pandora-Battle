#pragma once

#include "CoreMinimal.h"
#include "Common/CollisionChannels.h"
#include "GameplayTagContainer.h"
#include "Common/WeaponDefinitionData.h"
#include "Engine/DataAsset.h"
#include "Engine/CollisionProfile.h"
#include "Engine/EngineTypes.h"
#include "UObject/PrimaryAssetId.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "SkillTypes.generated.h"

class AGameplayAbilityTargetActor;
class UGameplayAbility;
class UGameplayEffect;
class UAnimMontage;
class AActor;
class AProjectileBase;
class AEffectAreaBase;
class UMaterialInterface;
class UMaterialParameterCollection;
class UNiagaraSystem;
class UStatusEffectDefinition;

namespace LabSkillDebug
{
	/** Asset debug flags are honored only while this central development CVar is enabled. */
	LABPROJECT_API bool IsDrawingEnabled();
}

USTRUCT(BlueprintType)
struct LABPROJECT_API FProjectileImpactEffectAreaSpawnConfig
{
	GENERATED_BODY()

	FProjectileImpactEffectAreaSpawnConfig()
		: GroundTraceChannel(LabCollisionChannels::VisibilityTrace())
	{
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact Effect Area")
	TSubclassOf<AEffectAreaBase> EffectAreaClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact Effect Area", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double LifeSpan = 10.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact Effect Area")
	bool bRequireSuccessfulDamageApplication = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact Effect Area")
	bool bSpawnAtTargetFeet = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact Effect Area")
	TEnumAsByte<ETraceTypeQuery> GroundTraceChannel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact Effect Area", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTraceStartHeight = 100.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact Effect Area", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTraceDepth = 10000.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact Effect Area")
	FVector SpawnOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact Effect Area")
	bool bAlignToGroundNormal = false;
};

UENUM(BlueprintType)
enum class ESkillType : uint8
{
	Active = 0 UMETA(Hidden),
	Passive = 1 UMETA(Hidden),
	Triggered = 2 UMETA(Hidden),
	Instant = 4 UMETA(DisplayName = "Instant"),
	Press = 3 UMETA(DisplayName = "Press"),
	Duration = 5 UMETA(DisplayName = "Duration")
};

UENUM(BlueprintType)
enum class ESkillDataType : uint8
{
	Projectile UMETA(DisplayName = "Projectile"),
	Area UMETA(DisplayName = "Area"),
	Dash UMETA(DisplayName = "Dash"),
	Aura UMETA(DisplayName = "Aura"),
	Trail UMETA(DisplayName = "Trail"),
	ShieldBubble UMETA(Hidden),
	FillShield UMETA(Hidden),
	Default UMETA(DisplayName = "Default"),
	Missile UMETA(DisplayName = "Missile"),
	Summon UMETA(DisplayName = "Summon"),
	Static UMETA(DisplayName = "Static")
};

UENUM(BlueprintType)
enum class ETrailSlashSpawnTiming : uint8
{
	AbilityActivate UMETA(Hidden),
	AttackTraceStart UMETA(Hidden),
	AnimNotify UMETA(DisplayName = "Anim Notify")
};

UENUM(BlueprintType)
enum class ESkillProjectileFireMode : uint8
{
	Immediate UMETA(DisplayName = "Immediate"),
	HoldThenConfirm UMETA(DisplayName = "Hold Then Confirm")
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillAnimationConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Animation")
	TObjectPtr<UAnimMontage> PrimaryMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Animation")
	TObjectPtr<UAnimMontage> SecondaryMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Animation", meta = (Categories = "Event"))
	FGameplayTag PrimaryEventTag;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillDamageConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Damage", meta = (ClampMin = "0.0"))
	double DamageMagnitude = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Damage", meta = (Categories = "Data"))
	FGameplayTag DamageDataTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Damage|Status")
	TSubclassOf<UGameplayEffect> StatusEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Damage|Status", meta = (ClampMin = "1.0"))
	float StatusEffectLevel = 1.0f;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FProjectileSkillConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Animation")
	FSkillAnimationConfig Animation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Damage")
	FSkillDamageConfig Damage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Niagara")
	TObjectPtr<UNiagaraSystem> MuzzleFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Niagara")
	TObjectPtr<UNiagaraSystem> ProjectileFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Niagara")
	TObjectPtr<UNiagaraSystem> HitFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Niagara")
	bool bSpawnHitNiagaraOnGround = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile")
	TSubclassOf<AProjectileBase> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	double ProjectileSpeed = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double ProjectileRadius = 50.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double TargetTraceMaxRange = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Targeting")
	FCollisionProfileName TargetTraceProfile = FCollisionProfileName(TEXT("NoCollision"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double MinimumTargetDistanceFromSpawn = 1000.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Targeting")
	bool bTraceAffectsAimPitch = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Targeting")
	bool bDrawTargetTraceDebug = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Spawn")
	FName SpawnSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Spawn")
	FVector SpawnLocationOffset = FVector(0.0, 0.0, 80.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Spawn", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double MinimumForwardSpawnOffset = 140.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Readied Scale")
	bool bEnableReadiedProjectileChargeGrowth = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Readied Scale")
	FVector ReadiedProjectileStartScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Readied Scale")
	FVector ReadiedProjectileTargetScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Readied Scale", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double ReadiedProjectileScaleDuration = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Readied Scale")
	FName ReadiedProjectileNiagaraVector2DParameterName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Readied Niagara")
	FVector2D ReadiedProjectileNiagaraStartSize = FVector2D::UnitVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Readied Niagara")
	FVector2D ReadiedProjectileNiagaraTargetSize = FVector2D::UnitVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting")
	bool bUseGroundTargeting = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bUseGroundTargeting"))
	TSubclassOf<AGameplayAbilityTargetActor> GroundTargetActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bUseGroundTargeting", ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTargetingMaxRange = 3000.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bUseGroundTargeting"))
	FCollisionProfileName GroundTargetingTraceProfile = FCollisionProfileName(TEXT("BlockAll"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bUseGroundTargeting", ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTargetingTraceStartHeight = 500.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bUseGroundTargeting", ClampMin = "100.0", ForceUnits = "cm"))
	double GroundTargetingTraceDepth = 100000.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bUseGroundTargeting", ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTargetingCollisionRadius = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bUseGroundTargeting", ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTargetingCollisionHeight = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bUseGroundTargeting"))
	bool bGroundTargetingTraceAffectsAimPitch = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bUseGroundTargeting"))
	bool bDrawGroundTargetingDebug = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting|Decal", meta = (EditCondition = "bUseGroundTargeting"))
	TObjectPtr<UMaterialInterface> GroundTargetingDecal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting|Decal", meta = (EditCondition = "bUseGroundTargeting", ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTargetingDecalSize = 512.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting|Decal", meta = (EditCondition = "bUseGroundTargeting"))
	FLinearColor GroundTargetingDecalColor = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FDashSkillConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Dash|Animation")
	FSkillAnimationConfig Animation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Dash|Damage")
	FSkillDamageConfig Damage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Dash", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	double Strength = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Dash", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double Duration = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Dash")
	bool bEnableGravityDuringDash = true;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FAuraSkillConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Body")
	TObjectPtr<UNiagaraSystem> BodyAuraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Body")
	FName BodyAuraComponentName = TEXT("AuraNiagara");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Body")
	bool bActivateBodyAura = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Body")
	bool bResetBodyAuraSystem = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Duration", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double Duration = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Duration")
	bool bClearBodyAuraWhenAuraEnds = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Presentation")
	bool bApplyFOVOverrideWhileActive = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Presentation", meta = (EditCondition = "bApplyFOVOverrideWhileActive", ClampMin = "1.0"))
	double ActiveFOV = 130.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Presentation", meta = (EditCondition = "bApplyFOVOverrideWhileActive", ClampMin = "0.0"))
	double FOVInterpSpeed = 10.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Presentation")
	bool bApplyOverlayMaterialWhileActive = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Presentation", meta = (EditCondition = "bApplyOverlayMaterialWhileActive"))
	TObjectPtr<UMaterialInterface> ActiveOverlayMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Attached Niagara")
	bool bSpawnAttachedNiagaraWhileActive = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Attached Niagara", meta = (EditCondition = "bSpawnAttachedNiagaraWhileActive", DisplayName = "Spawn At Character Location"))
	bool bSpawnAttachedNiagaraAtCharacterLocation = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Attached Niagara", meta = (EditCondition = "bSpawnAttachedNiagaraWhileActive"))
	TObjectPtr<UNiagaraSystem> AttachedNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Attached Niagara", meta = (EditCondition = "bSpawnAttachedNiagaraWhileActive"))
	FName AttachedNiagaraSocketName = TEXT("hand_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Attached Niagara", meta = (EditCondition = "bSpawnAttachedNiagaraWhileActive"))
	FVector AttachedNiagaraLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Attached Niagara", meta = (EditCondition = "bSpawnAttachedNiagaraWhileActive"))
	FRotator AttachedNiagaraRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Attached Niagara", meta = (EditCondition = "bSpawnAttachedNiagaraWhileActive"))
	FVector AttachedNiagaraScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Interaction Heal")
	bool bHealTeamInInteractionBox = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Interaction Heal", meta = (EditCondition = "bHealTeamInInteractionBox"))
	FName HealingInteractionComponentName = TEXT("InteractionBox");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Interaction Heal", meta = (EditCondition = "bHealTeamInInteractionBox"))
	TSubclassOf<UGameplayEffect> TeamHealEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Interaction Heal", meta = (EditCondition = "bHealTeamInInteractionBox", Categories = "Data"))
	FGameplayTag TeamHealMagnitudeDataTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Interaction Heal", meta = (EditCondition = "bHealTeamInInteractionBox", ClampMin = "0.0"))
	double TeamHealMagnitude = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Interaction Heal", meta = (EditCondition = "bHealTeamInInteractionBox", ClampMin = "0.05", ForceUnits = "s"))
	double TeamHealInterval = 1.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Interaction Heal", meta = (EditCondition = "bHealTeamInInteractionBox"))
	bool bHealSelfInInteractionBox = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Movement Speed")
	bool bIncreaseMovementSpeedOnActivate = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Movement Speed", meta = (EditCondition = "bIncreaseMovementSpeedOnActivate", ClampMin = "0.0", ForceUnits = "%", DisplayName = "Movement Speed Increase Percent"))
	double MovementSpeedIncrease = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Effect Area")
	bool bSpawnEffectArea = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Effect Area", meta = (EditCondition = "bSpawnEffectArea"))
	TSubclassOf<AEffectAreaBase> EffectAreaClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Effect Area", meta = (EditCondition = "bSpawnEffectArea", ClampMin = "0.0", ForceUnits = "s"))
	double EffectAreaLifeSpan = 10.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Effect Area", meta = (EditCondition = "bSpawnEffectArea"))
	FVector EffectAreaSpawnOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Ground Effects",
		meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Ground Effect Z Offset"))
	double GroundEffectZOffset = 3.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Effect Area|Repeat", meta = (EditCondition = "bSpawnEffectArea"))
	bool bRepeatEffectAreaSpawn = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Effect Area|Repeat", meta = (EditCondition = "bSpawnEffectArea && bRepeatEffectAreaSpawn", ClampMin = "0.1", ForceUnits = "s"))
	double EffectAreaSpawnInterval = 2.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Effect Area|Filter", meta = (EditCondition = "bSpawnEffectArea"))
	bool bEffectAreaIgnoreSourceActor = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Effect Area|Filter", meta = (EditCondition = "bSpawnEffectArea"))
	bool bEffectAreaAffectEnemiesOnly = false;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FTrailSkillConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Trail|Animation")
	FSkillAnimationConfig Animation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Trail|Niagara")
	TObjectPtr<UNiagaraSystem> TrailSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Trail|Slash")
	bool bEnableSlashHitTrace = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Trail|Slash", meta = (EditCondition = "bEnableSlashHitTrace"))
	TObjectPtr<UNiagaraSystem> SlashSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Trail|Slash|Timing", meta = (EditCondition = "bEnableSlashHitTrace"))
	ETrailSlashSpawnTiming SlashSpawnTiming = ETrailSlashSpawnTiming::AnimNotify;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Trail|Slash", meta = (EditCondition = "bEnableSlashHitTrace"))
	FVector SlashScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Trail|Slash", meta = (EditCondition = "bEnableSlashHitTrace"))
	FVector SlashSpawnLocationOffset = FVector(40.0, 0.0, 0.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Trail|Slash", meta = (EditCondition = "bEnableSlashHitTrace"))
	FName SlashSpawnSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Trail|Slash", meta = (EditCondition = "bEnableSlashHitTrace"))
	FRotator SlashSpawnRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Trail|Slash", meta = (EditCondition = "bEnableSlashHitTrace", ClampMin = "1.0"))
	double SlashAttackTraceEndMultiplier = 1.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Trail|Timing", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double Duration = 1.0;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FShieldSkillConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Shield|Animation")
	FSkillAnimationConfig Animation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Shield|Effect")
	TSubclassOf<UGameplayEffect> GameplayEffectClass;

};

USTRUCT(BlueprintType)
struct LABPROJECT_API FMissileSkillConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Animation")
	FSkillAnimationConfig Animation;

	UPROPERTY()
	FSkillDamageConfig Damage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Niagara")
	TObjectPtr<UNiagaraSystem> MissileSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Niagara")
	FName AimPositionParameterName = TEXT("aim_position");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Niagara", meta = (DisplayName = "Source Socket Name"))
	FName NiagaraSpawnSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Niagara")
	FVector NiagaraSpawnLocationOffset = FVector(0.0, 0.0, 80.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Niagara")
	FRotator NiagaraSpawnRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Niagara")
	FVector NiagaraScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Timing", meta = (ClampMin = "0.0", ForceUnits = "s", DisplayName = "Missile Duration", ToolTip = "How long the spawned missile Niagara should stay active. Set to 0 to let the Niagara system finish itself."))
	double MissileDuration = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting")
	TSubclassOf<AGameplayAbilityTargetActor> TargetActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double AutoTargetSearchRadius = 3000.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting", meta = (ForceUnits = "cm"))
	double AutoTargetForwardOffset = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double TargetingMaxRange = 3000.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting")
	FName TargetingTraceProfileName = TEXT("BlockAll");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double TargetingCollisionRadius = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double TargetingCollisionHeight = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting")
	bool bTargetingTraceAffectsAimPitch = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting")
	bool bDebugTargeting = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting")
	FName TargetSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Damage Over Time", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double DamageStartDelay = 1.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Damage Over Time", meta = (ClampMin = "0.0", ForceUnits = "s", DisplayName = "Damage Application Duration"))
	double DamageApplicationDuration = 3.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Damage Over Time", meta = (ClampMin = "0.05", ForceUnits = "s"))
	double DamageInterval = 0.5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Damage Over Time", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double DamageRadius = 256.0;

	UPROPERTY()
	double DamageDuration = 0.0;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSummonSkillConfig
{
	GENERATED_BODY()

	FSummonSkillConfig()
		: GroundTraceChannel(LabCollisionChannels::VisibilityTrace())
	{
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Animation")
	FSkillAnimationConfig Animation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Actor")
	TSubclassOf<AActor> SummonedActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Actor")
	bool bForceReplicateSpawnedActor = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Actor", meta = (EditCondition = "bForceReplicateSpawnedActor", ClampMin = "0.0", ForceUnits = "s"))
	double MinimumReplicatedActorLifetime = 0.5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Actor", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double SummonedActorLifeSpan = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Actor")
	bool bDestroySummonedActorOnAbilityCancel = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Spawn")
	FName SpawnSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Spawn", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double SpawnForwardDistance = 300.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Spawn")
	FVector SpawnLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Spawn")
	FRotator SpawnRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Spawn")
	bool bUseOwnerYawOnly = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Spawn")
	ESpawnActorCollisionHandlingMethod SpawnCollisionHandling = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Ground")
	bool bProjectSpawnToGround = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Ground", meta = (EditCondition = "bProjectSpawnToGround"))
	TEnumAsByte<ETraceTypeQuery> GroundTraceChannel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Ground", meta = (EditCondition = "bProjectSpawnToGround", ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTraceStartHeight = 500.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Ground", meta = (EditCondition = "bProjectSpawnToGround", ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTraceDepth = 2000.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Rise", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double RiseDistanceBelowGround = 250.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Rise", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double RiseDuration = 1.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Rise", meta = (ClampMin = "0.005", ForceUnits = "s"))
	double RiseTickInterval = 0.02;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Laser")
	FName LaserNiagaraComponentName = TEXT("LaserNiagara");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Laser")
	bool bActivateAllNiagaraComponentsWhenNameNone = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Laser")
	bool bDeactivateLaserNiagaraOnSpawn = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Laser")
	bool bResetLaserNiagaraOnActivate = true;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillGameplayEffectConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect")
	TSubclassOf<UGameplayEffect> GameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect", meta = (Categories = "Data"))
	FGameplayTag MagnitudeDataTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect")
	double Magnitude = 0.0;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillTopLevelDamageConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	TSubclassOf<UGameplayEffect> GameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (Categories = "Data"))
	FGameplayTag MagnitudeDataTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	double Magnitude = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (DisplayName = "Repeat Trigger Damage While Overlapping"))
	bool bRepeatTriggerDamageWhileOverlapping = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (EditCondition = "bRepeatTriggerDamageWhileOverlapping", EditConditionHides, ClampMin = "0.05", ForceUnits = "s", DisplayName = "Trigger Damage Interval"))
	double TriggerDamageInterval = 1.0;

	FSkillGameplayEffectConfig ToGameplayEffectConfig() const
	{
		FSkillGameplayEffectConfig Config;
		Config.GameplayEffectClass = GameplayEffectClass;
		Config.MagnitudeDataTag = MagnitudeDataTag;
		Config.Magnitude = Magnitude;
		return Config;
	}

	void FromGameplayEffectConfig(const FSkillGameplayEffectConfig& Config)
	{
		GameplayEffectClass = Config.GameplayEffectClass;
		MagnitudeDataTag = Config.MagnitudeDataTag;
		Magnitude = Config.Magnitude;
	}
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillSelfBuffSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Self Buff")
	bool bEnabled = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Self Buff", meta = (EditCondition = "bEnabled"))
	TSubclassOf<UGameplayEffect> GameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Self Buff", meta = (EditCondition = "bEnabled", Categories = "Data"))
	FGameplayTag MagnitudeDataTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Self Buff", meta = (EditCondition = "bEnabled"))
	double Magnitude = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Self Buff|Weapon Damage", meta = (EditCondition = "bEnabled", DisplayName = "Weapon Damage Bonus"))
	double WeaponDamageBonus = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Self Buff|Character", meta = (EditCondition = "bEnabled", ClampMin = "1.0"))
	double CharacterScaleMultiplier = 1.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Self Buff|Weapon Trace", meta = (EditCondition = "bEnabled", ClampMin = "1.0", DisplayName = "Trace End Z Multiplier"))
	double WeaponTraceEndZMultiplier = 1.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Self Buff", meta = (EditCondition = "bEnabled"))
	bool bRemoveOnAbilityEnd = true;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillTimeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Time", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double CooldownDuration = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Time", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double Duration = 0.0;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillMovementSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Movement")
	bool bUseOneShotDash = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Movement", meta = (DisplayName = "Use Strength While Active"))
	bool bOverrideMovementSpeedWhileActive = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Movement", meta = (DisplayName = "Lock Movement During Duration"))
	bool bLockMovementDuringDuration = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Movement", meta = (EditCondition = "bUseOneShotDash || bOverrideMovementSpeedWhileActive", ClampMin = "0.0", ForceUnits = "cm/s", DisplayName = "Strength"))
	double DashStrength = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Movement", meta = (EditCondition = "bUseOneShotDash", ClampMin = "0.0", ForceUnits = "s"))
	double DashDuration = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Movement", meta = (EditCondition = "bUseOneShotDash"))
	bool bEnableGravityDuringDash = true;

	UPROPERTY()
	FGameplayTag DashGameplayCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Movement", meta = (EditCondition = "bUseOneShotDash"))
	bool bHideCharacterDuringDash = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Movement|Contact Damage")
	bool bDamageEnemiesOnContact = false;

	UPROPERTY()
	FSkillGameplayEffectConfig ContactDamage;

	UPROPERTY()
	double MovementSpeedWhileActive = 0.0;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillOverlaySettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Overlay")
	bool bUseCharacterOverlay = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Overlay", meta = (EditCondition = "bUseCharacterOverlay"))
	TObjectPtr<UMaterialInterface> CharacterOverlayMaterial;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillDecalSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Decal")
	bool bSpawnAtCharacterLocation = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Decal", meta = (EditCondition = "bSpawnAtCharacterLocation"))
	TObjectPtr<UMaterialInterface> DecalMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Decal", meta = (EditCondition = "bSpawnAtCharacterLocation", ClampMin = "0.0", ForceUnits = "cm"))
	double DecalSize = 512.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Decal", meta = (EditCondition = "bSpawnAtCharacterLocation"))
	bool bGrowDecalSize = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Decal", meta = (EditCondition = "bSpawnAtCharacterLocation && bGrowDecalSize", ClampMin = "0.0", ForceUnits = "cm"))
	double FinalDecalSize = 0.0;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillNiagaraSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|GameplayCue", meta = (Categories = "GameplayCue"))
	FGameplayTag GameplayCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Aura")
	TObjectPtr<UNiagaraSystem> AuraNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Aura")
	FName AuraNiagaraComponentName = TEXT("AuraNiagara");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Aura")
	FVector AuraLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Aura")
	FVector AuraScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Ground")
	TObjectPtr<UNiagaraSystem> GroundNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Ground", meta = (DisplayName = "Follow Character"))
	bool bGroundNiagaraFollowsCharacter = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Ground", meta = (EditCondition = "bGroundNiagaraFollowsCharacter", DisplayName = "Follow Socket Name"))
	FName GroundFollowSocketName = TEXT("root");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Ground")
	FVector GroundLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Ground")
	FRotator GroundRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Ground")
	FVector GroundScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Socket")
	TObjectPtr<UNiagaraSystem> SocketNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Socket")
	FName SocketNiagaraComponentName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Socket")
	FName SocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Socket")
	FVector SocketLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Socket")
	FRotator SocketRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Socket")
	FVector SocketScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect", meta = (ShowOnlyInnerProperties))
	FSkillGameplayEffectConfig GameplayEffect;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillSummonSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon")
	bool bEnabled = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon", meta = (EditCondition = "bEnabled"))
	TSubclassOf<AActor> SummonedActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Network", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ForceUnits = "s"))
	double MinimumReplicatedActorLifetime = 0.5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon", meta = (EditCondition = "bEnabled"))
	FName SpawnSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ForceUnits = "cm"))
	double SpawnForwardDistance = 300.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon", meta = (EditCondition = "bEnabled"))
	FVector SpawnLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon", meta = (EditCondition = "bEnabled"))
	FRotator SpawnRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon", meta = (EditCondition = "bEnabled"))
	bool bProjectSpawnToGround = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Rise", meta = (EditCondition = "bEnabled"))
	bool bRiseFromUnderground = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Rise", meta = (EditCondition = "bEnabled && bRiseFromUnderground", ClampMin = "0.0", ForceUnits = "cm"))
	double RiseDistanceBelowGround = 250.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Rise", meta = (EditCondition = "bEnabled && bRiseFromUnderground", ClampMin = "0.0", ForceUnits = "cm/s"))
	double RiseSpeed = 250.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Rise", meta = (EditCondition = "bEnabled && bRiseFromUnderground"))
	FVector FinalLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Decal", meta = (EditCondition = "bEnabled"))
	bool bShowDecalWhileSummoning = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Decal", meta = (EditCondition = "bEnabled && bShowDecalWhileSummoning"))
	TObjectPtr<UMaterialInterface> SummonDecalMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Decal", meta = (EditCondition = "bEnabled && bShowDecalWhileSummoning", ClampMin = "0.0", ForceUnits = "cm"))
	double SummonDecalSize = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Laser", meta = (EditCondition = "bEnabled"))
	FName LaserNiagaraComponentName = TEXT("LaserNiagara");

	UPROPERTY()
	FSkillGameplayEffectConfig TriggerDamage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Damage", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ForceUnits = "s"))
	double TriggerDamageDelay = 0.0;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillProjectileSettings
{
	GENERATED_BODY()

	FSkillProjectileSettings()
	{
		ProjectileSocketNames.SetNum(6);
	}

	// Compatibility field used only by editor conditions. SkillDataType is the
	// sole runtime variant discriminator.
	UPROPERTY()
	bool bEnabled = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile", meta = (EditCondition = "bEnabled", EditFixedSize))
	TArray<FName> ProjectileSocketNames;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ForceUnits = "s", DisplayName = "Socket Fire Interval"))
	double ProjectileSocketFireInterval = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile", meta = (EditCondition = "bEnabled"))
	ESkillProjectileFireMode FireMode = ESkillProjectileFireMode::Immediate;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile", meta = (EditCondition = "bEnabled", Categories = "Event"))
	FGameplayTag FireEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Scale", meta = (EditCondition = "bEnabled"))
	bool bGrowProjectileSize = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Scale", meta = (EditCondition = "bEnabled && bGrowProjectileSize"))
	FVector ProjectileStartScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Scale", meta = (EditCondition = "bEnabled && bGrowProjectileSize"))
	FVector ProjectileFinalScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Scale", meta = (EditCondition = "bEnabled && bGrowProjectileSize", ClampMin = "0.0", ForceUnits = "s"))
	double ProjectileScaleDuration = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Scale", meta = (EditCondition = "bEnabled && bGrowProjectileSize"))
	FName GrowthUserParameterName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Scale", meta = (EditCondition = "bEnabled && bGrowProjectileSize"))
	FVector2D GrowthUserParameterStartValue = FVector2D::UnitVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Scale", meta = (EditCondition = "bEnabled && bGrowProjectileSize"))
	FVector2D GrowthUserParameterFinalValue = FVector2D::UnitVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile", meta = (EditCondition = "bEnabled"))
	TSubclassOf<AProjectileBase> ProjectileActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ForceUnits = "cm/s"))
	double ProjectileSpeed = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ForceUnits = "cm"))
	double ProjectileRadius = 50.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Trajectory", meta = (EditCondition = "bEnabled"))
	bool bUseArcTrajectory = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Trajectory", meta = (EditCondition = "bEnabled && bUseArcTrajectory", ClampMin = "0.0", ForceUnits = "cm"))
	double ProjectileArcHeight = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Trajectory", meta = (EditCondition = "bEnabled && bUseArcTrajectory", ClampMin = "0.0"))
	double ProjectileArcGravityScale = 1.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact", meta = (EditCondition = "bEnabled"))
	bool bStickOnImpact = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact", meta = (EditCondition = "bEnabled && bStickOnImpact", EditConditionHides, ClampMin = "0.01", ForceUnits = "s"))
	double PostImpactLifeSpan = 1.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Spawn", meta = (EditCondition = "bEnabled"))
	FVector SpawnLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Spawn", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ForceUnits = "cm"))
	double MinimumForwardSpawnOffset = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Targeting", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ForceUnits = "cm"))
	double TargetTraceMaxRange = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Targeting", meta = (EditCondition = "bEnabled"))
	FCollisionProfileName TargetTraceProfile = FCollisionProfileName(TEXT("NoCollision"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Targeting", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ForceUnits = "cm"))
	double MinimumTargetDistanceFromSpawn = 1000.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Targeting", meta = (EditCondition = "bEnabled"))
	bool bTraceAffectsAimPitch = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Debug", meta = (EditCondition = "bEnabled"))
	bool bDrawTargetTraceDebug = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Targeting", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Ground Trace Start Height"))
	double TargetGroundTraceStartHeight = 500.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Targeting", meta = (EditCondition = "bEnabled", ClampMin = "100.0", ForceUnits = "cm", DisplayName = "Ground Trace Depth"))
	double TargetGroundTraceDepth = 100000.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Niagara", meta = (EditCondition = "bEnabled"))
	TObjectPtr<UNiagaraSystem> MuzzleNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Niagara", meta = (EditCondition = "bEnabled"))
	TObjectPtr<UNiagaraSystem> ProjectileNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Niagara", meta = (EditCondition = "bEnabled"))
	TObjectPtr<UNiagaraSystem> HitNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Niagara", meta = (EditCondition = "bEnabled"))
	bool bSpawnHitNiagaraOnGround = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bEnabled"))
	bool bUseGroundTargeting = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bEnabled && bUseGroundTargeting"))
	TSubclassOf<AGameplayAbilityTargetActor> GroundTargetActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bEnabled && bUseGroundTargeting", ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTargetingMaxRange = 3000.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bEnabled && bUseGroundTargeting"))
	FCollisionProfileName GroundTargetingTraceProfile = FCollisionProfileName(TEXT("BlockAll"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bEnabled && bUseGroundTargeting", ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTargetingCollisionRadius = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bEnabled && bUseGroundTargeting", ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTargetingCollisionHeight = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bEnabled && bUseGroundTargeting"))
	bool bGroundTargetingTraceAffectsAimPitch = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Debug", meta = (EditCondition = "bEnabled && bUseGroundTargeting"))
	bool bDrawGroundTargetingDebug = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Decal", meta = (EditCondition = "bEnabled"))
	TObjectPtr<UMaterialInterface> TargetDecal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Decal", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ForceUnits = "cm"))
	double TargetDecalSize = 512.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Decal", meta = (EditCondition = "bEnabled"))
	bool bGrowTargetDecalSize = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Decal", meta = (EditCondition = "bEnabled && bGrowTargetDecalSize", ClampMin = "0.0", ForceUnits = "cm"))
	double TargetDecalFinalSize = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Decal", meta = (EditCondition = "bEnabled && bUseGroundTargeting"))
	FLinearColor TargetDecalColor = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);

	UPROPERTY()
	FSkillGameplayEffectConfig Damage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Status")
	TSubclassOf<UGameplayEffect> StatusEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Status", meta = (ClampMin = "1.0"))
	float StatusEffectLevel = 1.0f;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FAnimeAuraPresentationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Anime Aura Presentation")
	TObjectPtr<UAnimMontage> PowerUpMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Anime Aura Presentation")
	TObjectPtr<UMaterialInterface> OverlayMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Anime Aura Presentation")
	TObjectPtr<UNiagaraSystem> AttachedNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Anime Aura Presentation")
	TObjectPtr<UMaterialParameterCollection> MaterialParameterCollection;

	bool IsComplete() const
	{
		return PowerUpMontage
			&& OverlayMaterial
			&& AttachedNiagaraSystem
			&& MaterialParameterCollection;
	}
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillStaticSettings
{
	GENERATED_BODY()

	FSkillStaticSettings()
		: GroundTraceChannel(LabCollisionChannels::VisibilityTrace())
	{
		SpawnSocketNames.SetNum(6);
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static")
	bool bEnabled = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Spawn", meta = (EditCondition = "bEnabled"))
	bool bUseSpawnSockets = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Spawn", meta = (EditCondition = "bEnabled && bUseSpawnSockets", DisplayName = "Attach Spawned Actor To Socket"))
	bool bAttachSpawnedActorToSocket = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static", meta = (EditCondition = "bEnabled", EditFixedSize))
	TArray<FName> SpawnSocketNames;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ForceUnits = "s"))
	double SpawnInterval = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Repeat", meta = (EditCondition = "bEnabled"))
	bool bRepeatSpawnSequence = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Repeat", meta = (EditCondition = "bEnabled && bRepeatSpawnSequence", ClampMin = "0.1", ForceUnits = "s"))
	double RepeatSpawnInterval = 2.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static", meta = (EditCondition = "bEnabled"))
	TSubclassOf<AActor> StaticActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static", meta = (EditCondition = "bEnabled", ShowOnlyInnerProperties))
	FAnimeAuraPresentationSettings AnimeAuraPresentation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Spawn", meta = (EditCondition = "bEnabled"))
	FVector SpawnLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Spawn", meta = (EditCondition = "bEnabled"))
	FRotator SpawnRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Spawn", meta = (EditCondition = "bEnabled"))
	bool bProjectSpawnToGround = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Spawn", meta = (EditCondition = "bEnabled && bProjectSpawnToGround"))
	TEnumAsByte<ETraceTypeQuery> GroundTraceChannel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Spawn", meta = (EditCondition = "bEnabled && bProjectSpawnToGround", ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTraceStartHeight = 100.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Spawn", meta = (EditCondition = "bEnabled && bProjectSpawnToGround", ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTraceDepth = 10000.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Spawn", meta = (EditCondition = "bEnabled"))
	ESpawnActorCollisionHandlingMethod SpawnCollisionHandling = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Actor", meta = (EditCondition = "bEnabled"))
	bool bForceReplicateSpawnedActor = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Actor|Network", meta = (EditCondition = "bEnabled && bForceReplicateSpawnedActor", ClampMin = "0.0", ForceUnits = "s"))
	double MinimumReplicatedActorLifetime = 1.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Actor", meta = (EditCondition = "bEnabled", DisplayName = "Use Spawned Actor Life Span"))
	bool bUseSpawnedActorLifeSpan = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Actor", meta = (EditCondition = "bEnabled && bUseSpawnedActorLifeSpan", EditConditionHides, ClampMin = "0.0", ForceUnits = "s"))
	double SpawnedActorLifeSpan = 2.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Actor", meta = (EditCondition = "bEnabled"))
	bool bDestroySpawnedActorsOnAbilityEnd = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Trigger", meta = (EditCondition = "bEnabled"))
	FName TriggerComponentName = TEXT("Box");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Trigger", meta = (EditCondition = "bEnabled"))
	bool bDamageExistingOverlapsOnSpawn = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Trigger", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ForceUnits = "s"))
	double TriggerActiveDurationAfterLastSpawn = 0.5;

	UPROPERTY()
	FSkillGameplayEffectConfig TriggerDamage;

	UPROPERTY()
	bool bRepeatTriggerDamageWhileOverlapping = false;

	UPROPERTY()
	double TriggerDamageInterval = 1.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Damage", meta = (EditCondition = "bEnabled"))
	bool bIgnoreSourceActor = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Omen Orb|Pull", meta = (EditCondition = "bEnabled", DisplayName = "Pull Enemies For Entire Skill Duration"))
	bool bOmenOrbPullEnemiesDuringGrowth = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Omen Orb|Growth",
		meta = (EditCondition = "bEnabled", DisplayName = "Growth Duration", ToolTip = "Time in seconds for the Omen Orb and floor effects to grow from their start scale to their end scale.", ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	double OmenOrbGrowthDuration = 3.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Omen Orb|Pull", meta = (EditCondition = "bEnabled && bOmenOrbPullEnemiesDuringGrowth", ClampMin = "0.0", ForceUnits = "cm"))
	double OmenOrbPullRadius = 1200.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Omen Orb|Pull", meta = (EditCondition = "bEnabled && bOmenOrbPullEnemiesDuringGrowth", ClampMin = "0.0", ForceUnits = "cm/s", DisplayName = "Pull Speed (Additive)", ToolTip = "Velocity added toward the Black Hole. Opposite movement remains active, so the effective pull is Pull Speed minus the target's movement speed."))
	double OmenOrbPullSpeed = 650.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Omen Orb|Finish Damage", meta = (EditCondition = "bEnabled"))
	bool bOmenOrbApplyFinishAreaDamage = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Omen Orb|Finish Damage", meta = (EditCondition = "bEnabled && bOmenOrbApplyFinishAreaDamage", ClampMin = "0.0", ForceUnits = "cm"))
	double OmenOrbFinishDamageRadius = 1200.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Effect Area", meta = (EditCondition = "bEnabled"))
	bool bEffectAreaIgnoreSourceActor = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static|Effect Area", meta = (EditCondition = "bEnabled"))
	bool bEffectAreaAffectEnemiesOnly = false;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillHealSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Heal")
	bool bEnabled = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Heal", meta = (EditCondition = "bEnabled"))
	FSkillGameplayEffectConfig TeamHealEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Heal", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ForceUnits = "cm"))
	double HealRadius = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Heal", meta = (EditCondition = "bEnabled", ClampMin = "0.05", ForceUnits = "s"))
	double HealInterval = 1.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Heal", meta = (EditCondition = "bEnabled"))
	bool bHealSelf = false;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillSwordTrailSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Sword Trail")
	bool bEnabled = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Sword Trail", meta = (EditCondition = "bEnabled", ClampMin = "1.0"))
	double TrailEndZLengthMultiplier = 1.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Sword Trail", meta = (EditCondition = "bEnabled"))
	TObjectPtr<UNiagaraSystem> TrailNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Sword Trail", meta = (EditCondition = "bEnabled"))
	TObjectPtr<UNiagaraSystem> SlashNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Sword Trail", meta = (EditCondition = "bEnabled"))
	FTransform SlashTransformOffset = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Sword Trail", meta = (EditCondition = "bEnabled", DisplayName = "Slash Spawn Socket Name"))
	FName SlashSpawnSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Sword Trail", meta = (EditCondition = "bEnabled"))
	ETrailSlashSpawnTiming SlashSpawnTiming = ETrailSlashSpawnTiming::AnimNotify;
};

UCLASS(BlueprintType, Blueprintable, meta = (PrioritizeCategories = "!Skill|Properties !Skill|UI !Skill|Cost Damage !Skill|Animation"))
class LABPROJECT_API USkillDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USkillDefinition();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual void PostLoad() override;
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Properties")
	ESkillType SkillType = ESkillType::Instant;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Properties", meta = (DisplayName = "Cancel On Hit"))
	bool bCancelOnHit = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Properties", meta = (DisplayName = "Skill Data Type"))
	ESkillDataType SkillDataType = ESkillDataType::Projectile;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI", AssetRegistrySearchable)
	FName Name;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI")
	bool bShowInAbilitiesBar = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI", meta = (AssetBundles = "Client", AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI|Effect Icon", meta = (DisplayName = "Show Burn Effect Icon"))
	bool bShowBurnEffectIcon = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI|Effect Icon", meta = (DisplayName = "Show Frostbite Effect Icon"))
	bool bShowFrostbiteEffectIcon = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI|Effect Icon", meta = (DisplayName = "Show Electric Shock Effect Icon"))
	bool bShowElectricShockEffectIcon = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI|Effect Icon", meta = (DisplayName = "Show Shield Effect Icon"))
	bool bShowShieldEffectIcon = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Cost", meta = (ClampMin = "0.0", DisplayName = "Mana Cost"))
	double ManaCost = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ShowOnlyInnerProperties))
	FSkillTopLevelDamageConfig Damage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Animation", meta = (ShowOnlyInnerProperties))
	FSkillAnimationConfig Animation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Time", meta = (ShowOnlyInnerProperties))
	FSkillTimeSettings Time;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Movement", meta = (ShowOnlyInnerProperties))
	FSkillMovementSettings Movement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Overlay", meta = (ShowOnlyInnerProperties))
	FSkillOverlaySettings Overlay;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Decal", meta = (ShowOnlyInnerProperties))
	FSkillDecalSettings CharacterDecal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX", meta = (ShowOnlyInnerProperties))
	FSkillNiagaraSettings Niagara;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect", meta = (ShowOnlyInnerProperties))
	FSkillGameplayEffectConfig GameplayEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Self Buff", meta = (ShowOnlyInnerProperties))
	FSkillSelfBuffSettings SelfBuff;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Status")
	TObjectPtr<UStatusEffectDefinition> StatusEffectDataAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Status", meta = (ClampMin = "1.0"))
	float StatusEffectLevel = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Status",
		meta = (ClampMin = "1", UIMin = "1",
			ToolTip = "Number of status-effect stacks applied by one successful skill hit."))
	int32 StackCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon", meta = (ShowOnlyInnerProperties))
	FSkillSummonSettings SummonSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile", meta = (ShowOnlyInnerProperties))
	FSkillProjectileSettings ProjectileSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Static", meta = (ShowOnlyInnerProperties))
	FSkillStaticSettings StaticSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Heal", meta = (ShowOnlyInnerProperties))
	FSkillHealSettings Heal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Sword Trail", meta = (ShowOnlyInnerProperties))
	FSkillSwordTrailSettings SwordTrail;

	UPROPERTY()
	FDashSkillConfig Dash;

	UPROPERTY()
	FAuraSkillConfig Aura;

	UPROPERTY()
	FTrailSkillConfig Trail;

	UPROPERTY()
	FShieldSkillConfig Default;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile", meta = (ShowOnlyInnerProperties))
	FMissileSkillConfig Missile;

	UPROPERTY()
	FSummonSkillConfig Summon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Ability", meta = (DisplayName = "Abilities to Grant"))
	TArray<TSubclassOf<UGameplayAbility>> AbilitiesToGrant;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Targeting Max Range"))
	double AOETargetingMaxRange = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (DisplayName = "Target Actor Class"))
	TSubclassOf<AGameplayAbilityTargetActor> AOETargetActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (DisplayName = "Targeting Decal"))
	TObjectPtr<UMaterialInterface> AOETargetingDecal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (DisplayName = "Targeting Decal Color"))
	FLinearColor AOETargetingDecalColor = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (DisplayName = "Targeting Trace Profile Name"))
	FName AOETargetingTraceProfileName = TEXT("BlockAll");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Targeting Collision Radius"))
	double AOETargetingCollisionRadius = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Targeting Collision Height"))
	double AOETargetingCollisionHeight = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (DisplayName = "Targeting Trace Affects Aim Pitch"))
	bool bAOETargetingTraceAffectsAimPitch = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (DisplayName = "Debug Targeting"))
	bool bAOEDebugTargeting = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (DisplayName = "Targeting Socket Name"))
	FName AOETargetingSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Camera", meta = (DisplayName = "Use Camera Settings"))
	bool bUseAOECameraSettings = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Camera", meta = (EditCondition = "bUseAOECameraSettings", EditConditionHides, DisplayName = "Camera Settings"))
	FWeaponAimCameraSettings AOECameraSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|AI Targeting", meta = (DisplayName = "Target Ground Trace Channel"))
	TEnumAsByte<ETraceTypeQuery> AOETargetGroundTraceChannel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|AI Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Target Ground Trace Depth"))
	double AOETargetGroundTraceDepth = 10000.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Radius", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Radius"))
	double AOERadius = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Debug", meta = (DisplayName = "Draw Debug Damage Radius"))
	bool bAOEDrawDebugDamageRadius = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Debug", meta = (EditCondition = "bAOEDrawDebugDamageRadius", EditConditionHides, ClampMin = "0.0", ForceUnits = "s", DisplayName = "Debug Damage Radius Draw Time"))
	double AOEDebugDamageRadiusDrawTime = 5.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|GameplayCue", meta = (Categories = "GameplayCue", DisplayName = "Indicator Cue Tag"))
	FGameplayTag AOEIndicatorCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|GameplayCue", meta = (Categories = "GameplayCue", DisplayName = "Impact Cue Tag"))
	FGameplayTag AOELightningBoltCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Timing", meta = (ClampMin = "0.0", ForceUnits = "s", DisplayName = "Damage Delay"))
	double AOELightningDamageDelay = 0.2;

	FText GetDisplayName() const;
	UObject* GetIconResource() const;
	bool ShouldShowInAbilitiesBar() const;
	ESkillDataType GetResolvedSkillDataType() const;
	FSkillGameplayEffectConfig GetResolvedDamageConfig() const;
	FSkillGameplayEffectConfig GetResolvedStaticFinishDamageConfig() const;
	const FShieldSkillConfig* GetDefensiveSkillConfig() const;
	const TArray<TSubclassOf<UGameplayAbility>>& GetExplicitAbilitiesToGrant() const { return AbilitiesToGrant; }

private:
	bool HasEnabledDebugDrawingFlags() const;
	void MigrateLegacyProjectileSettings();
	void ApplyCurrentSettingsToRuntimeConfig();
	void SyncTopLevelDamageToRuntimeConfig();

	UPROPERTY(meta = (
		DeprecatedProperty,
		DeprecationMessage = "Legacy migration payload. Use ProjectileSettings.",
		FormerlySerializedAs = "Projectile"))
	FProjectileSkillConfig Projectile_DEPRECATED;

	UPROPERTY()
	int32 ProjectileSettingsVersion = 0;
};
