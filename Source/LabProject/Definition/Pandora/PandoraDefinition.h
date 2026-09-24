#pragma once

#include "CoreMinimal.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "UI/Shop/ShopTypes.h"
#include "UObject/PrimaryAssetId.h"

#include "PandoraDefinition.generated.h"

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

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UPandoraDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// Public API ------------------------------------------------------------------------------------------------------
	FText GetDisplayName() const;
	FText GetDescription() const;
	UTexture2D* GetIconTexture() const { return IconTexture.Get(); }
	UObject* GetIconResource() const;

	FGameplayTag GetIdTag() const { return IdTag; }
	const FGameplayTagContainer& GetActivatableWeaponTags() const { return ActivatableWeaponTags; }

	int32 GetMaxLevel() const;
	int32 GetRequiredPointsForLevel(int32 Level) const;
	const TArray<FPandoraUnlockRule>& GetUnlockRules() const { return UnlockRules; }

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	static int32 GetFixedMaxLevel();

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	static int32 GetRequiredLevelForSkillSlot(int32 SkillSlotIndex);

	int32 GetSkillCount() const { return Skills.Num(); }

	/** 슬롯이 없거나 비어 있으면 nullptr를 반환한다. */
	const USkillDefinition* GetSkillDefinition(int32 SkillSlotIndex) const;

	const FShopProductDefinitionData& GetShopData() const { return ShopData; }

	bool MatchesPandoraType(FGameplayTag PandoraTypeTag) const;

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	bool IsSkillSlotUnlocked(int32 SkillSlotIndex, int32 PandoraLevel) const;

	UFUNCTION(BlueprintPure, Category = "!Pandora|Weapon")
	bool IsCompatibleWithWeaponTag(FGameplayTag WeaponTag) const;

	UFUNCTION(BlueprintPure, Category = "!Pandora|Weapon")
	bool IsCompatibleWithWeaponDefinition(const UItemDefinition* WeaponDefinition) const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora", meta = (AllowPrivateAccess = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> IconTexture = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora", meta = (AllowPrivateAccess = "true"))
	FGameplayTag IdTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Weapon", meta = (Categories = "Item.Weapon", AllowPrivateAccess = "true"))
	FGameplayTagContainer ActivatableWeaponTags;

	/** 판도라의 최대 레벨. */
	static constexpr int32 MaxLevel = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Rules", meta = (ClampMin = "1", AllowPrivateAccess = "true"))
	TArray<int32> PointsRequiredPerLevel = {1, 1, 1};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora|Unlock Rules", meta = (AllowPrivateAccess = "true"))
	TArray<FPandoraUnlockRule> UnlockRules;

	/** 슬롯 순서대로 참조하는 스킬 정의. 빈 슬롯은 nullptr로 유지한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<USkillDefinition>> Skills;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora", meta = (AllowPrivateAccess = "true"))
	int32 Tier = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Shop", meta = (AllowPrivateAccess = "true"))
	FShopProductDefinitionData ShopData;
};
