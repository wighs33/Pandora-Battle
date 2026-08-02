#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "ShopEntryWidget.generated.h"

class UButton;
class UImage;
class UShopEntryViewData;
class UTextBlock;
class UWidget;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UShopEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bInIsSelected) override;

	UFUNCTION(BlueprintCallable, Category = "!Shop")
	void SetEntryData(UShopEntryViewData* InEntryData);

	UFUNCTION(BlueprintCallable, Category = "!Shop")
	void SetSelected(bool bInSelected);

protected:
	UFUNCTION()
	void HandleSelectClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UTextBlock> Txt_Name = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UTextBlock> Txt_Price = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UTextBlock> Txt_State = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UImage> Img_Icon = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UImage> SelectionBorderImage = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UButton> Btn_Select = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Text")
	FText PriceTextFormat = NSLOCTEXT("ShopEntryWidget", "PriceTextFormat", "{0} Gold");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Text")
	FText OwnedText = NSLOCTEXT("ShopEntryWidget", "OwnedText", "Owned");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Text")
	FText AvailableText = NSLOCTEXT("ShopEntryWidget", "AvailableText", "Available");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Text")
	FText NotEnoughGoldText = NSLOCTEXT("ShopEntryWidget", "NotEnoughGoldText", "Need Gold");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Text")
	FText NotForSaleText = NSLOCTEXT("ShopEntryWidget", "NotForSaleText", "Not for sale");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Style")
	FLinearColor SelectionBorderDefaultColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Style")
	FLinearColor SelectionBorderSelectedColor = FLinearColor(0.0f, 0.45f, 1.0f, 1.0f);

private:
	void ResolveWidgets();
	void RefreshUI();
	void ApplySelectionVisual();

	UPROPERTY(Transient)
	TObjectPtr<UShopEntryViewData> EntryData = nullptr;

	bool bIsSelected = false;
};
