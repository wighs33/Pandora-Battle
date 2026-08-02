#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "UI/Shop/ShopTypes.h"
#include "ShopWidget.generated.h"

class UButton;
struct FStreamableHandle;
class UShopCatalogDefinition;
class UShopEntryViewData;
class UShopPreviewPanelWidget;
class UTextBlock;
class UTileView;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "!Shop")
	void RefreshUI();

	UFUNCTION(BlueprintCallable, Category = "!Shop")
	void SelectEntry(UShopEntryViewData* EntryData);

	UFUNCTION(BlueprintCallable, Category = "!Shop")
	bool TryPurchaseSelectedEntry();

protected:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandlePandoraCategoryClicked();

	UFUNCTION()
	void HandleSkinCategoryClicked();

	UFUNCTION()
	void HandleBuyRequested(UShopEntryViewData* EntryData);

	UFUNCTION()
	void HandleResetShopSaveClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UTileView> TileView = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UShopPreviewPanelWidget> WBP_ShopPreviewPanel = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UTextBlock> Txt_Gold = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UButton> Btn_Close = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UButton> PandoraCategoryButton = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UButton> SkinCategoryButton = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UButton> Btn_ResetShopSave = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Catalog")
	TObjectPtr<UShopCatalogDefinition> ShopCatalogDefinition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Catalog")
	TArray<FShopCatalogProductReference> ShopProductList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Catalog")
	bool bAutoIncludeAllPandoras = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Catalog")
	bool bAutoIncludeAllSkins = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Catalog", meta = (ClampMin = "1", UIMin = "1"))
	int32 PurchasedPandoraStartingLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Pandora")
	TArray<FName> PurchasablePandoraKeys = {
		TEXT("Fire"),
		TEXT("Light"),
		TEXT("Machine"),
		TEXT("Freeze"),
		TEXT("Lightning"),
		TEXT("Darkness")
	};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Text")
	FText GoldTextFormat = NSLOCTEXT("ShopWidget", "GoldTextFormat", "{0}");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Text")
	FText SelectItemText = NSLOCTEXT("ShopWidget", "SelectItemText", "Select an item.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Text")
	FText AlreadyOwnedText = NSLOCTEXT("ShopWidget", "AlreadyOwnedText", "Already owned.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Text")
	FText NotEnoughGoldText = NSLOCTEXT("ShopWidget", "NotEnoughGoldText", "Not enough gold.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Text")
	FText NotForSaleText = NSLOCTEXT("ShopWidget", "NotForSaleText", "Not for sale.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Text")
	FText PandoraComingSoonText = NSLOCTEXT("ShopWidget", "PandoraComingSoonText", "Update scheduled.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Text")
	FText UnsupportedProductTypeText = NSLOCTEXT("ShopWidget", "UnsupportedProductTypeText", "This product type cannot be purchased yet.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Text")
	FText PurchaseSucceededTextFormat = NSLOCTEXT("ShopWidget", "PurchaseSucceededTextFormat", "Purchased {0}.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Text")
	FText ShopSaveResetText = NSLOCTEXT("ShopWidget", "ShopSaveResetText", "Shop data reset.");

private:
	void ResolveWidgets();
	void BindWidgets();
	void UnbindWidgets();
	void BeginContentPreload();
	void BeginCatalogPresentationPreload(int32 PreloadGeneration);
	void ReleaseContentPreloads();
	void SetActiveCategory(EShopProductType NewCategory);
	void RefreshCategoryButtonStates() const;
	void RebuildEntryData();
	TArray<FShopCatalogEntry> BuildEffectiveCatalog() const;
	FShopCatalogEntry MakeCatalogEntry(const FShopCatalogProductReference& ProductReference) const;
	void AppendProductReference(TArray<FShopCatalogEntry>& OutCatalog, TSet<FString>& SeenProductKeys, const FShopCatalogProductReference& ProductReference) const;
	void AppendAllPandoras(TArray<FShopCatalogEntry>& OutCatalog, TSet<FString>& SeenProductKeys) const;
	void AppendAllSkins(TArray<FShopCatalogEntry>& OutCatalog, TSet<FString>& SeenProductKeys) const;
	FString MakeProductKey(const FShopCatalogProductReference& ProductReference) const;
	void SortCatalogEntries(TArray<FShopCatalogEntry>& CatalogEntries) const;
	FString GetCatalogEntrySortName(const FShopCatalogEntry& CatalogEntry) const;
	UObject* ResolveProductObject(const FShopCatalogEntry& CatalogEntry) const;
	UObject* ResolveProductObject(const FShopCatalogProductReference& ProductReference) const;
	FShopProductDefinitionData ResolveShopData(UObject* ProductObject, EShopProductType ProductType) const;
	FString GetResolvedPlayerId() const;
	int32 GetCurrentGold() const;
	bool IsProductOwned(UShopEntryViewData* EntryData) const;
	bool IsProductOwned(UObject* ProductObject, EShopProductType ProductType) const;
	bool IsPandoraAllowedForShopPurchase(const UObject* ProductObject) const;
	bool IsPandoraComingSoonProduct(const UObject* ProductObject, EShopProductType ProductType) const;
	bool IsPandoraComingSoonEntry(const UShopEntryViewData* EntryData) const;
	bool CanPurchaseProductType(EShopProductType ProductType) const;
	void EnsurePlayerSaveLoaded() const;
	void SetMessage(const FText& Message) const;
	UShopEntryViewData* FindEntryDataByProduct(UObject* ProductObject, EShopProductType ProductType) const;
	void HandleTileViewItemClicked(UObject* ItemObject);
	void HandleEntryDataClicked(UShopEntryViewData* EntryData);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UShopEntryViewData>> EntryDataList;

	UPROPERTY(Transient)
	TObjectPtr<UShopEntryViewData> SelectedEntryData = nullptr;

	EShopProductType ActiveCategory = EShopProductType::Pandora;

	bool bWidgetsBound = false;

	int32 ContentPreloadGeneration = 0;
	TSharedPtr<FStreamableHandle> PandoraContentPreloadHandle;
	TSharedPtr<FStreamableHandle> SkinContentPreloadHandle;
	TSharedPtr<FStreamableHandle> CatalogProductPreloadHandle;
	TSharedPtr<FStreamableHandle> CatalogPresentationPreloadHandle;
};
