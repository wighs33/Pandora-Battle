#pragma once

#include "CoreMinimal.h"
#include "Common/WeaponDefinitionData.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UI/Shop/ShopTypes.h"
#include "UObject/PrimaryAssetId.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "ItemDefinition.generated.h"

class UGameplayEffect;
class UTexture2D;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UItemDefinition();
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	static FName GetWeaponPresentationBundleName();
	void GetWeaponPresentationAssetPaths(TArray<FSoftObjectPath>& OutAssetPaths) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	bool MatchesItemType(FGameplayTag ItemTypeTag) const;
	bool HasWeaponData() const;
	bool IsWeaponDefinition(FGameplayTag WeaponTypeTag) const;

	UFUNCTION(BlueprintPure, Category = "!Item|Weapon")
	float GetSafeAttackStaminaCost() const;

	UFUNCTION(BlueprintPure, Category = "!Item|Weapon")
	float GetEquippedMovementSpeedMultiplier() const;

	bool IsConsumableDefinition(FGameplayTag ConsumableTypeTag) const;
	bool CanDropFromRewardChest() const;
	float GetRewardChestDropWeight() const;
	int32 GetSafeQuantityToConsume() const;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> IconTexture = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	FGameplayTag IdTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	TMap<FGameplayTag, float> Map_Stat_Magnitude;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Consumable")
	TSubclassOf<UGameplayEffect> ConsumeGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Consumable")
	TMap<FGameplayTag, float> Map_Consume_Magnitude;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Consumable", meta = (ClampMin = "1", UIMin = "1"))
	int32 QuantityToConsume = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Drop")
	bool bCanDropFromRewardChest = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Drop", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DropRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Shop")
	FShopProductDefinitionData ShopData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon",
		meta = (AssetBundles = "WeaponPresentation"))
	FWeaponDefinitionData WeaponData;
};
