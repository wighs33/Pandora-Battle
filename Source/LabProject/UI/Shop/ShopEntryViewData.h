#pragma once

#include "CoreMinimal.h"
#include "UI/Shop/ShopTypes.h"
#include "ShopEntryViewData.generated.h"

class UPandoraDefinition;
enum class EShopProductType : uint8;

DECLARE_MULTICAST_DELEGATE_OneParam(FShopEntryViewDataClickedDelegate, class UShopEntryViewData*);

UCLASS(BlueprintType)
class LABPROJECT_API UShopEntryViewData : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(const FShopCatalogEntry& InCatalogEntry, UObject* InProductObject, int32 InPlayerGold, bool bInOwned);

	const FShopCatalogEntry& GetCatalogEntry() const { return CatalogEntry; }
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
	UObject* GetProductObjectBP() const { return UiData.ProductObject; }

	UFUNCTION(BlueprintPure, Category = "!Shop")
	EShopProductType GetProductType() const { return UiData.ProductType; }

	UFUNCTION(BlueprintPure, Category = "!Shop")
	int32 GetGoldPrice() const { return UiData.GoldPrice; }

	UFUNCTION(BlueprintPure, Category = "!Shop")
	bool IsOwned() const { return UiData.bOwned; }

	UFUNCTION(BlueprintPure, Category = "!Shop")
	bool CanAfford() const { return UiData.bCanAfford; }

	UFUNCTION(BlueprintPure, Category = "!Shop")
	bool CanSell() const { return UiData.bCanSell; }

	UFUNCTION(BlueprintPure, Category = "!Shop")
	bool IsValidEntry() const { return UiData.bValid; }

	void BroadcastClicked();

	FShopEntryViewDataClickedDelegate OnClicked;

private:
	UPROPERTY(Transient)
	FShopCatalogEntry CatalogEntry;

	UPROPERTY(Transient)
	TObjectPtr<UObject> ProductObject = nullptr;

	UPROPERTY(Transient)
	FShopEntryUiData UiData;
};
