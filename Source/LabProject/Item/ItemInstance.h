#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "ItemInstance.generated.h"

class UItemDefinition;
DECLARE_LOG_CATEGORY_EXTERN(ItemInstanceLog, Log, All);

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UItemInstance : public UObject
{
	GENERATED_BODY()

public:
	static constexpr float UpgradeStatBonusRatePerLevel = 0.5f;

	UFUNCTION(BlueprintCallable, Category = "!Item")
	FGuid GetOrCreateItemId();

	UFUNCTION(BlueprintPure, Category = "!Item")
	FGuid GetItemId() const { return ItemId; }

	UFUNCTION(BlueprintPure, Category = "!Item|Upgrade")
	int32 GetUpgradeLevel() const { return FMath::Max(UpgradeLevel, 0); }

	float GetUpgradeBonusStatMagnitude(FGameplayTag StatTag) const;
	float GetEffectiveStatMagnitude(FGameplayTag StatTag) const;
	void BuildUpgradeBonusStatMagnitudes(TMap<FGameplayTag, float>& OutMagnitudes) const;
	void BuildEffectiveStatMagnitudes(TMap<FGameplayTag, float>& OutMagnitudes) const;
	void SetUpgradeLevel(int32 InUpgradeLevel);

	void EnsureItemId();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Item")
	TObjectPtr<const UItemDefinition> ItemDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Item|Stat")
	TMap<FGameplayTag, float> Map_EnhancedStat_Magnitude;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Item")
	int32 Quantity = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "!Item|Upgrade")
	int32 UpgradeLevel = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "!Item")
	FGuid ItemId;
};
