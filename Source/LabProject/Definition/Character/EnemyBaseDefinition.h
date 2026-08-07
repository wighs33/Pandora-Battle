#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "EnemyBaseDefinition.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UItemDefinition;
class UStatUpgradeDefinition;
class AMonsterCharacter;

USTRUCT(BlueprintType)
struct LABPROJECT_API FMonsterPresentationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Monster|Damage")
	TSubclassOf<UGameplayEffect> ContactDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Monster|Animation")
	TObjectPtr<UAnimMontage> HitReactMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Monster|Animation",
		meta = (ClampMin = "0.01"))
	float HitReactPlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Monster|Animation")
	TObjectPtr<UAnimMontage> DeathMontage;
};

/**
 * Data shared by enemy combat archetypes.
 *
 * Runtime state deliberately does not live here. AEnemyBase resolves this
 * immutable configuration once during component initialization and copies it
 * into its focused runtime components.
 */
USTRUCT(BlueprintType)
struct LABPROJECT_API FEnemyCombatSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Targeting")
	bool bUseNearestPlayerWhenTargetUnset = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Spawning",
		meta = (AssetBundles = "Server"))
	TSoftClassPtr<AMonsterCharacter> DefaultMonsterClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Equipment", meta = (AssetBundles = "Server"))
	TSoftObjectPtr<UItemDefinition> StartingWeaponDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Stats", meta = (AssetBundles = "Server"))
	TSoftObjectPtr<UStatUpgradeDefinition> DefaultStatDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Combat")
	bool bRequestComboWhenAttackIsActive = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Combat")
	bool bAttackEnabled = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Combat")
	bool bStartCombatOnPossess = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Combat")
	bool bUseBehaviorTreeCombat = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Combat")
	bool bAttackImmediatelyAfterStart = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Combat", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float InitialCombatDelay = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Combat", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float AttackInterval = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Combat")
	bool bMoveToTargetBeforeAttack = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Combat", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Attack Range", ToolTip = "Distance used for starting attacks, continuing combos, and deciding when AI should chase instead of attacking."))
	float AttackStartDistance = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Combat", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Ranged Attack Range", ToolTip = "Distance used by AI for ranged weapons such as bows and guns."))
	float RangedAttackStartDistance = 2500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Combat", meta = (DisplayName = "Move To Target Before Ranged Attack"))
	bool bMoveToTargetBeforeRangedAttack = false;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FEnemyTrainingBotSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Training Bot|Hit Reaction")
	bool bEnableHitReaction = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Training Bot|Hit Reaction", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float HitStunDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Training Bot|Respawn")
	bool bRespawnOnDeath = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Training Bot|Respawn", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float RespawnDelay = 3.0f;
};

UCLASS(BlueprintType, Const)
class LABPROJECT_API UEnemyBaseDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	static FPrimaryAssetId GetDefaultPrimaryAssetId();
	static FSoftObjectPath GetDefaultDefinitionPath();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	const FEnemyCombatSettings& GetCombatSettings() const { return Combat; }
	TSoftObjectPtr<UStatUpgradeDefinition> GetEffectiveDefaultStatDefinition() const;
	const FEnemyTrainingBotSettings& GetTrainingBotSettings() const { return TrainingBot; }
	const FMonsterPresentationSettings& GetMonsterPresentationSettings() const { return MonsterPresentation; }
	float GetMonsterMaxHealth() const { return MonsterMaxHealth; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy", meta = (AllowPrivateAccess = "true"))
	FEnemyCombatSettings Combat;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy", meta = (AllowPrivateAccess = "true"))
	FEnemyTrainingBotSettings TrainingBot;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Monster",
		meta = (AllowPrivateAccess = "true"))
	FMonsterPresentationSettings MonsterPresentation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Enemy|Monster|Stats",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float MonsterMaxHealth = 50.0f;
};
