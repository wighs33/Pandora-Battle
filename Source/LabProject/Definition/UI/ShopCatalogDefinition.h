#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UI/Shop/ShopTypes.h"
#include "ShopCatalogDefinition.generated.h"

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UShopCatalogDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Shop")
	FText ShopName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Shop", meta = (MultiLine = "true"))
	FText ShopDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Shop|Auto Include")
	bool bAutoIncludeAllPandoras = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Shop|Auto Include")
	bool bAutoIncludeAllSkins = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Shop", meta = (AssetBundles = "Client"))
	TArray<FShopCatalogProductReference> ProductList;
};
