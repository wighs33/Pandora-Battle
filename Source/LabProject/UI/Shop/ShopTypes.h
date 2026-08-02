#pragma once

#include "CoreMinimal.h"
#include "ShopTypes.generated.h"

class UItemDefinition;
class UPandoraDefinition;
class USkinDefinition;

UENUM(BlueprintType)
enum class EShopProductType : uint8
{
	Pandora UMETA(DisplayName = "Pandora"),
	Skin UMETA(DisplayName = "Skin"),
	Item UMETA(DisplayName = "Item")
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FShopProductDefinitionData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop", meta = (ClampMin = "0", UIMin = "0"))
	int32 GoldPrice = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	bool bCanSell = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	int32 SortOrder = 0;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FShopCatalogProductReference
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	EShopProductType ProductType = EShopProductType::Pandora;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop", meta = (EditCondition = "ProductType == EShopProductType::Pandora", EditConditionHides))
	TSoftObjectPtr<UPandoraDefinition> PandoraDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop", meta = (EditCondition = "ProductType == EShopProductType::Skin", EditConditionHides))
	TSoftObjectPtr<USkinDefinition> SkinDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop", meta = (EditCondition = "ProductType == EShopProductType::Item", EditConditionHides))
	TSoftObjectPtr<UItemDefinition> ItemDefinition;

	bool HasValidProduct() const
	{
		switch (ProductType)
		{
		case EShopProductType::Pandora:
			return !PandoraDefinition.IsNull();
		case EShopProductType::Skin:
			return !SkinDefinition.IsNull();
		case EShopProductType::Item:
			return !ItemDefinition.IsNull();
		default:
			return false;
		}
	}
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FShopCatalogEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	FShopCatalogProductReference Product;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	FShopProductDefinitionData ShopData;

	bool HasValidProduct() const
	{
		return Product.HasValidProduct();
	}
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FShopEntryUiData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	TObjectPtr<UObject> IconResource = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	TObjectPtr<UObject> ProductObject = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	EShopProductType ProductType = EShopProductType::Pandora;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop", meta = (ClampMin = "0", UIMin = "0"))
	int32 GoldPrice = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	bool bOwned = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	bool bCanAfford = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	bool bCanSell = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	bool bValid = false;
};
