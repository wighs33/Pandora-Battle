#pragma once

#include "CoreMinimal.h"
#include "UI/Shop/ShopEntryUiData.h"
#include "ShopEntryViewData.generated.h"

class UPandoraDefinition;
enum class EShopProductType : uint8;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnShopEntryClicked, class UShopEntryViewData*);

UCLASS(BlueprintType)
class LABPROJECT_API UShopEntryViewData : public UObject
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	void Initialize(const FShopCatalogEntry& InCatalogEntry, UObject* InProductObject, int32 InPlayerGold, bool bInOwned);

	UObject* GetProductObject() const { return ProductObject.Get(); }
	UPandoraDefinition* GetPandoraDefinition() const;
	const FShopEntryUiData& GetUiData() const { return UiData; }

	UFUNCTION(BlueprintPure, Category = "!Shop")
	FText GetDisplayName() const { return UiData.DisplayName; }

	UFUNCTION(BlueprintPure, Category = "!Shop")
	FText GetDescription() const { return UiData.Description; }

	UFUNCTION(BlueprintPure, Category = "!Shop")
	UObject* GetIconResource() const { return UiData.IconResource; }

	UFUNCTION(BlueprintPure, Category = "!Shop")
	EShopProductType GetProductType() const { return UiData.ProductType; }

	UFUNCTION(BlueprintPure, Category = "!Shop")
	int32 GetGoldPrice() const { return UiData.GoldPrice; }

	UFUNCTION(BlueprintPure, Category = "!Shop")
	bool CanSell() const { return UiData.bCanSell; }

	void BroadcastClicked();

public:
	FOnShopEntryClicked OnClicked;

private:
	UPROPERTY(Transient)
	TObjectPtr<UObject> ProductObject = nullptr;

	UPROPERTY(Transient)
	FShopEntryUiData UiData;
};
