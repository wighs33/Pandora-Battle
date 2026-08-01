#pragma once

#include "CoreMinimal.h"
#include "Common/Enum_Operation.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "StatUpgradeDefinition.generated.h"

class UGameplayEffect;

USTRUCT(BlueprintType)
struct FPdStatUpgradeRule
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat Upgrade", meta = (Categories = "Status"))
	FGameplayTag RootTag;

	// Compatibility fields consumed by the pre-refactor StatUpgradeComponent.
	// The data-driven runtime replaces these with AttributeDefaultValues.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat Upgrade|Legacy")
	float Magnitude = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat Upgrade|Legacy")
	EEnum_Operation Operation = EEnum_Operation::Add;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat Upgrade|Cost", meta = (Categories = "Status"))
	FGameplayTag CostPointTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat Upgrade|Cost", meta = (ClampMin = "0.0"))
	float Cost = 0.f;

	bool IsValid() const
	{
		return RootTag.IsValid();
	}
};

USTRUCT(BlueprintType)
struct FPdStatAttributeDefaultValue
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat", meta = (Categories = "Status"))
	FGameplayTag StatTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat")
	float DefaultValue = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat", meta = (DisplayName = "Value Per Upgrade"))
	float ValuePerUpgrade = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat")
	int32 Priority = 0;

	bool IsValid() const
	{
		return StatTag.IsValid();
	}
};

USTRUCT(BlueprintType)
struct FPdPairedResourceStatTag
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat Upgrade", meta = (Categories = "Status"))
	FGameplayTag MaxStatTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat Upgrade", meta = (Categories = "Status"))
	FGameplayTag CurrentStatTag;

	bool IsValid() const
	{
		return MaxStatTag.IsValid() && CurrentStatTag.IsValid();
	}
};

UCLASS(BlueprintType, Const, meta = (DisplayName = "DA Stat"))
class LABPROJECT_API UStatUpgradeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UStatUpgradeDefinition();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	TSubclassOf<UGameplayEffect> GetStatUpGameplayEffectClass() const { return StatUpGameplayEffectClass; }
	bool ShouldEnableRecoveryHealthRegen() const { return bEnableRecoveryHealthRegen; }
	TSubclassOf<UGameplayEffect> GetRecoveryHealGameplayEffectClass() const { return RecoveryHealGameplayEffectClass; }
	const TArray<FPdStatUpgradeRule>& GetUpgradeRules() const { return UpgradeRules; }
	const TArray<FPdPairedResourceStatTag>& GetPairedResourceStatTags() const { return PairedResourceStatTags; }
	const TArray<FPdStatAttributeDefaultValue>& GetAttributeValues() const { return AttributeDefaultValues; }
	const TArray<FPdStatAttributeDefaultValue>& GetAttributeDefaultValues() const { return AttributeDefaultValues; }
	float GetMaxInvestedLevel() const;
	const FPdStatUpgradeRule* FindUpgradeRuleForStat(const FGameplayTag& StatTag) const;
	bool TryGetAttributeValuePerUpgrade(const FGameplayTag& StatTag, float& OutValue) const;
	float GetAttributeValuePerUpgrade(const FGameplayTag& StatTag) const;
	bool TryGetExactAttributeDefaultValue(const FGameplayTag& StatTag, float& OutValue) const;
	static bool TryResolveDefaultStatLevelTag(const FGameplayTag& StatTag, FGameplayTag& OutLevelTag);

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Gameplay Effect
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat Upgrade|Effect", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> StatUpGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat Upgrade|Rules", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", ClampMax = "100.0", UIMin = "1.0", UIMax = "100.0"))
	float MaxInvestedLevel = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat Upgrade|Recovery", meta = (AllowPrivateAccess = "true"))
	bool bEnableRecoveryHealthRegen = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat Upgrade|Recovery", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> RecoveryHealGameplayEffectClass;

	//------------------------------------------------------------------------------------------------------------------
	//--- Upgrade Rules
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat Upgrade|Rules", meta = (TitleProperty = "RootTag", AllowPrivateAccess = "true"))
	TArray<FPdStatUpgradeRule> UpgradeRules;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat Upgrade|Rules", meta = (TitleProperty = "MaxStatTag", AllowPrivateAccess = "true"))
	TArray<FPdPairedResourceStatTag> PairedResourceStatTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat|Attribute Values", meta = (TitleProperty = "StatTag", AllowPrivateAccess = "true", DisplayName = "Attribute Values"))
	TArray<FPdStatAttributeDefaultValue> AttributeDefaultValues;
};
