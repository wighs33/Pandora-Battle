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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat Upgrade")
	float Magnitude = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat Upgrade")
	EEnum_Operation Operation = EEnum_Operation::Add;

	bool IsValid() const
	{
		return RootTag.IsValid() && !FMath::IsNearlyZero(Magnitude);
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

UCLASS(BlueprintType, Const)
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
	const TArray<FPdStatUpgradeRule>& GetUpgradeRules() const { return UpgradeRules; }
	const TArray<FPdPairedResourceStatTag>& GetPairedResourceStatTags() const { return PairedResourceStatTags; }

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Gameplay Effect
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat Upgrade|Effect", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> StatUpGameplayEffectClass;

	//------------------------------------------------------------------------------------------------------------------
	//--- Upgrade Rules
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat Upgrade|Rules", meta = (TitleProperty = "RootTag", AllowPrivateAccess = "true"))
	TArray<FPdStatUpgradeRule> UpgradeRules;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Stat Upgrade|Rules", meta = (TitleProperty = "MaxStatTag", AllowPrivateAccess = "true"))
	TArray<FPdPairedResourceStatTag> PairedResourceStatTags;
};
