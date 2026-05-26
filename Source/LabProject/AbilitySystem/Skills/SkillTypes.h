#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Common/WeaponDefinitionData.h"
#include "Engine/DataAsset.h"
#include "Engine/CollisionProfile.h"
#include "Engine/EngineTypes.h"
#include "SkillTypes.generated.h"

class AGameplayAbilityTargetActor;
class UGameplayAbility;
class UGameplayEffect;
class UAnimMontage;
class AProjectileBase;
class AEffectAreaBase;
class UPdStatusEffectDataAsset;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct LABPROJECT_API FProjectileImpactEffectAreaSpawnConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact Effect Area")
	TSubclassOf<AEffectAreaBase> EffectAreaClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact Effect Area", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double LifeSpan = 10.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact Effect Area")
	bool bRequireSuccessfulDamageApplication = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact Effect Area")
	bool bSpawnAtTargetFeet = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact Effect Area")
	TEnumAsByte<ETraceTypeQuery> GroundTraceChannel = TraceTypeQuery1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact Effect Area", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTraceStartHeight = 100.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact Effect Area", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTraceDepth = 10000.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact Effect Area")
	FVector SpawnOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact Effect Area")
	bool bAlignToGroundNormal = false;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillLevelUnlock
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Legacy Level Unlocks", meta = (ClampMin = "1"))
	int32 RequiredLevel = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Legacy Level Unlocks")
	TArray<TSubclassOf<UGameplayAbility>> AbilitiesToGrant;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Legacy Level Unlocks")
	TArray<TSubclassOf<UGameplayEffect>> EffectsToApply;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Legacy Level Unlocks")
	TArray<TObjectPtr<UPdStatusEffectDataAsset>> StatusEffectsToUnlock;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Legacy Level Unlocks|Projectile")
	TArray<FProjectileImpactEffectAreaSpawnConfig> ProjectileImpactEffectAreas;
};

UENUM(BlueprintType)
enum class EPdSkillType : uint8
{
	Active UMETA(DisplayName = "Active"),
	Passive UMETA(DisplayName = "Passive"),
	Triggered UMETA(DisplayName = "Triggered")
};

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API USkillDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USkillDataAsset();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual void PostLoad() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Properties")
	EPdSkillType SkillType = EPdSkillType::Passive;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI")
	FName Name;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI")
	bool bShowInAbilitiesBar = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI", meta = (AssetBundles = "Client", AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI", meta = (MultiLine = "true"))
	TArray<FText> DescriptionPerLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile")
	TObjectPtr<UAnimMontage> ProjectileShootMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile")
	TSubclassOf<AProjectileBase> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	double ProjectileSpeed = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Event", meta = (Categories = "Event"))
	FGameplayTag ShootProjectileEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Damage")
	TSubclassOf<UGameplayEffect> ProjectileDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Damage", meta = (ClampMin = "0.0"))
	double ProjectileDamageMagnitude = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Damage", meta = (ClampMin = "0.0"))
	double ProjectileDamagePercentIncreasePerLevel = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Damage", meta = (Categories = "Data"))
	FGameplayTag ProjectileDamageDataTag;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Camera")
	bool bUseProjectileCameraSettings = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Camera", meta = (EditCondition = "bUseProjectileCameraSettings"))
	FWeaponAimCameraSettings ProjectileCameraSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Camera", meta = (Categories = "UI.Widget"))
	FGameplayTag ProjectileCrosshairWidgetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Camera", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double ProjectileAimReleaseDelay = 2.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Animation")
	TObjectPtr<UAnimMontage> AOETargetingMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Animation")
	TObjectPtr<UAnimMontage> AOETriggerMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Damage")
	TSubclassOf<UGameplayEffect> AOEDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Damage", meta = (ClampMin = "0.0"))
	double AOEDamageMagnitude = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Damage", meta = (ClampMin = "0.0"))
	double AOEDamagePercentIncreasePerLevel = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Damage", meta = (Categories = "Data"))
	FGameplayTag AOEDamageDataTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Damage")
	TArray<TEnumAsByte<EObjectTypeQuery>> AOEDamageObjectTypes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double AOETargetingMaxRange = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Targeting")
	TSubclassOf<AGameplayAbilityTargetActor> AOETargetActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Targeting")
	TObjectPtr<UMaterialInterface> AOETargetingDecal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Targeting")
	FLinearColor AOETargetingDecalColor = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Targeting")
	FName AOETargetingTraceProfileName = TEXT("BlockAll");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double AOETargetingCollisionRadius = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double AOETargetingCollisionHeight = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Targeting")
	bool bAOETargetingTraceAffectsAimPitch = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Targeting")
	bool bAOEDebugTargeting = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Targeting")
	FName AOETargetingSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Camera")
	bool bUseAOECameraSettings = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Camera", meta = (EditCondition = "bUseAOECameraSettings"))
	FWeaponAimCameraSettings AOECameraSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|AI Targeting")
	TEnumAsByte<ETraceTypeQuery> AOETargetGroundTraceChannel = TraceTypeQuery1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|AI Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double AOETargetGroundTraceDepth = 10000.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Radius", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double AOERadius = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Radius", meta = (ClampMin = "0.0"))
	double AOERadiusPercentIncreasePerLevel = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Debug")
	bool bAOEDrawDebugDamageRadius = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Debug", meta = (EditCondition = "bAOEDrawDebugDamageRadius", ClampMin = "0.0", ForceUnits = "s"))
	double AOEDebugDamageRadiusDrawTime = 5.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Event", meta = (Categories = "Event"))
	FGameplayTag AOEMontageTriggerEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|GameplayCue", meta = (Categories = "GameplayCue"))
	FGameplayTag AOEIndicatorCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|GameplayCue", meta = (Categories = "GameplayCue"))
	FGameplayTag AOELightningBoltCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|AOE|Timing", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double AOELightningDamageDelay = 0.2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Dash", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	double DashStrength = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Dash", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double DashDuration = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Dash")
	bool bEnableGravityDuringDash = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Dash|GameplayCue", meta = (Categories = "GameplayCue"))
	FGameplayTag DashCueTag;

	FText GetDisplayName() const;
	FText GetDescriptionForLevel(int32 Level) const;
	UObject* GetIconResource() const;
	bool ShouldShowInAbilitiesBar() const;
	bool HasLegacyLevelUnlocks() const;
	TArray<TSubclassOf<UGameplayAbility>> GetLegacyAbilitiesToGrantForLevel(int32 Level) const;
	TArray<TSubclassOf<UGameplayEffect>> GetLegacyEffectsToApplyForLevel(int32 Level) const;
	TArray<TObjectPtr<UPdStatusEffectDataAsset>> GetLegacyStatusEffectsToUnlockForLevel(int32 Level) const;
	TArray<FProjectileImpactEffectAreaSpawnConfig> GetLegacyProjectileImpactEffectAreasForLevel(int32 Level) const;

private:
	void MigrateDeprecatedBaseDefinitionToLegacyLevelUnlocks();

	// Deprecated serialized fields kept only so old Skill assets can be read while PandoraDefinition becomes the unlock source.
	UPROPERTY()
	TArray<FSkillLevelUnlock> LevelUnlocks;

	UPROPERTY()
	TArray<TSubclassOf<UGameplayAbility>> AbilitiesToGrant;

	UPROPERTY()
	TArray<TSubclassOf<UGameplayEffect>> EffectsToApply;

	UPROPERTY()
	TArray<TObjectPtr<UPdStatusEffectDataAsset>> StatusEffectsToUnlock;
};
