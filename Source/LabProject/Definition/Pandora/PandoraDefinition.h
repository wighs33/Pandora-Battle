#pragma once

#include "CoreMinimal.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "UI/Shop/ShopTypes.h"
#include "UObject/PrimaryAssetId.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "PandoraDefinition.generated.h"

class UGameplayAbility;
class UItemDefinition;
class UTexture2D;
class UPandoraDefinition;

USTRUCT(BlueprintType, Blueprintable)
struct LABPROJECT_API FPandoraUnlockRule
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Unlock Rules")
	TObjectPtr<UPandoraDefinition> RequiredPandora;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Unlock Rules", meta = (ClampMin = "1"))
	int32 RequiredLevel = 1;
};

USTRUCT(BlueprintType, Blueprintable, meta = (DisplayName = "Skill"))
struct FSkill
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill", meta = (DisplayName = "Skill Definition"))
	TObjectPtr<USkillDefinition> SkillDefinition;

	FText GetDisplayName() const;
	FText GetDescription() const;
	UObject* GetIconResource() const;
	bool ShouldShowInAbilitiesBar() const;
	TArray<TSubclassOf<UGameplayAbility>> GetAbilitiesToGrant() const;
};

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UPandoraDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPandoraDefinition();
	virtual void PostLoad() override;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora")
	TObjectPtr<UTexture2D> IconTexture = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora")
	FGameplayTag IdTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Weapon", meta = (Categories = "Item.Weapon"))
	FGameplayTagContainer ActivatableWeaponTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Rules", meta = (ClampMin = "3", ClampMax = "3", UIMin = "3", UIMax = "3"))
	int32 MaxLevel = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Rules", meta = (ClampMin = "1"))
	TArray<int32> PointsRequiredPerLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Unlock Rules")
	TArray<FPandoraUnlockRule> UnlockRules;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Pandora", meta = (DisplayName = "Skills"))
	TArray<FSkill> Skill;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora")
	int32 Tier = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Shop")
	FShopProductDefinitionData ShopData;

	FText GetDisplayName() const;
	FText GetDescription() const;
	UObject* GetIconResource() const;
	int32 GetMaxLevel() const;
	int32 GetRequiredPointsForLevel(int32 Level) const;
	bool MatchesPandoraType(FGameplayTag PandoraTypeTag) const;

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	static int32 GetFixedMaxLevel();

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	static int32 GetRequiredLevelForSkillSlot(int32 SkillSlotIndex);

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	bool IsSkillSlotUnlocked(int32 SkillSlotIndex, int32 PandoraLevel) const;

	UFUNCTION(BlueprintPure, Category = "!Pandora|Weapon")
	bool IsCompatibleWithWeaponTag(FGameplayTag WeaponTag) const;

	UFUNCTION(BlueprintPure, Category = "!Pandora|Weapon")
	bool IsCompatibleWithWeaponDefinition(const UItemDefinition* WeaponDefinition) const;

private:
	void NormalizeLevelRules();
};
