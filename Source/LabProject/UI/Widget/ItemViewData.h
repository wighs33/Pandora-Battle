#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ItemViewData.generated.h"

class UItemInstance;
class UObject;
class USkinDefinition;
class USkinInstance;

USTRUCT(BlueprintType)
struct LABPROJECT_API FItemViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ViewData")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ViewData")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ViewData", meta = (AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> IconResource = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ViewData")
	TMap<FGameplayTag, float> Stats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ViewData")
	TMap<FGameplayTag, float> UpgradeBonusStats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ViewData")
	int32 Quantity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ViewData")
	int32 UpgradeLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ViewData", meta = (Categories = "Item.Weapon"))
	FGameplayTagContainer RequiredWeaponTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ViewData")
	bool bOwned = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ViewData")
	bool bActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ViewData")
	bool bEnabled = true;

	bool HasContent() const
	{
		return !DisplayName.IsEmpty() || !Description.IsEmpty() || IconResource != nullptr || !Stats.IsEmpty() || Quantity > 0;
	}
};

class LABPROJECT_API FItemViewDataBuilder
{
public:
	static FItemViewData FromItemInstance(const UItemInstance* ItemInstance, bool bOwned = true, bool bActive = true);
	static FItemViewData FromSkinInstance(const USkinInstance* SkinInstance, bool bOwned = true, bool bActive = true);
	static FItemViewData FromSkinDefinition(const USkinDefinition* SkinDefinition, bool bOwned = true, bool bActive = true);

private:
	static TMap<FGameplayTag, float> BuildItemStatMap(const UItemInstance* ItemInstance);
};
