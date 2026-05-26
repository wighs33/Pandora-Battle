#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Skills/SkillTypes.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "PandoraDefinition.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UItemDefinition;
class UPdStatusEffectDataAsset;
class UTexture2D;
class UPandoraDefinition;

DECLARE_LOG_CATEGORY_EXTERN(PandoraDefinitionLog, Log, All);

USTRUCT(BlueprintType, Blueprintable)
struct LABPROJECT_API FPandoraUnlockRule
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Unlock Rules")
	TObjectPtr<UPandoraDefinition> RequiredPandora;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Unlock Rules", meta = (ClampMin = "1"))
	int32 RequiredLevel = 1;
};

USTRUCT(BlueprintType, Blueprintable)
struct LABPROJECT_API FPandoraSkillLevelUnlock
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Skill Unlocks", meta = (ClampMin = "1"))
	int32 RequiredLevel = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Skill Unlocks")
	TArray<TSubclassOf<UGameplayAbility>> AbilitiesToGrant;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Skill Unlocks")
	TArray<TSubclassOf<UGameplayEffect>> EffectsToApply;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Skill Unlocks")
	TArray<TObjectPtr<UPdStatusEffectDataAsset>> StatusEffectsToUnlock;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Skill Unlocks|Projectile")
	TArray<FProjectileImpactEffectAreaSpawnConfig> ProjectileImpactEffectAreas;
};

USTRUCT(BlueprintType, Blueprintable, meta = (DisplayName = "Skill"))
struct FSkill
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill", meta = (DisplayName = "Skill Definition"))
	TObjectPtr<USkillDataAsset> SkillDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Skill Unlocks")
	TArray<FPandoraSkillLevelUnlock> LevelUnlocks;

	FName GetSkillName() const;
	FText GetDisplayName() const;
	FText GetDescription() const;
	FText GetDescriptionForLevel(int32 Level) const;
	UObject* GetIconResource() const;
	bool ShouldShowInAbilitiesBar() const;
	TArray<TSubclassOf<UGameplayAbility>> GetAbilitiesToGrant() const;
	TArray<TSubclassOf<UGameplayAbility>> GetAbilitiesToGrantForLevel(int32 Level) const;
	TArray<TSubclassOf<UGameplayEffect>> GetEffectsToApply() const;
	TArray<TSubclassOf<UGameplayEffect>> GetEffectsToApplyForLevel(int32 Level) const;
	TArray<TObjectPtr<UPdStatusEffectDataAsset>> GetStatusEffectsToUnlock() const;
	TArray<TObjectPtr<UPdStatusEffectDataAsset>> GetStatusEffectsToUnlockForLevel(int32 Level) const;
	TArray<FProjectileImpactEffectAreaSpawnConfig> GetProjectileImpactEffectAreasForLevel(int32 Level) const;
	int32 GetMaxLevel() const;
};

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UPandoraDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora", meta = (MultiLine = "true"))
	TArray<FText> DescriptionPerLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora")
	TObjectPtr<UTexture2D> IconTexture = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora", meta = (AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> IconOverride = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora", meta = (AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> ActiveIconOverride = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora")
	FGameplayTag IdTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Weapon", meta = (Categories = "Item.Weapon"))
	FGameplayTagContainer ActivatableWeaponTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Rules", meta = (ClampMin = "1"))
	int32 MaxLevel = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Rules", meta = (ClampMin = "1"))
	TArray<int32> PointsRequiredPerLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Unlock Rules")
	TArray<FPandoraUnlockRule> UnlockRules;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Pandora", meta = (DisplayName = "Skills"))
	TArray<FSkill> Skill;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora")
	int32 Tier = 0;

	FText GetDisplayName() const;
	FText GetDescription() const;
	FText GetDescriptionForLevel(int32 Level) const;
	UObject* GetIconResource() const;
	UObject* GetActiveIconResource() const;
	int32 GetMaxLevel() const;
	int32 GetRequiredPointsForLevel(int32 Level) const;

	UFUNCTION(BlueprintPure, Category = "!Pandora|Weapon")
	bool IsCompatibleWithWeaponTag(FGameplayTag WeaponTag) const;

	UFUNCTION(BlueprintPure, Category = "!Pandora|Weapon")
	bool IsCompatibleWithWeaponDefinition(const UItemDefinition* WeaponDefinition) const;
};
