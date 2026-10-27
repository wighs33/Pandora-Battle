#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "UI/Common/LocalizedMenuWidget.h"
#include "CoreMinimal.h"
#include "ShopEntryWidget.generated.h"

class UButton;
class UImage;
class UShopEntryViewData;
class UTextBlock;
class UWidget;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UShopEntryWidget : public ULocalizedMenuWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Interface Implementations ---------------------------------------------------------------------------------------
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bInIsSelected) override;

	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!Shop")
	void SetEntryData(UShopEntryViewData* InEntryData);

	UFUNCTION(BlueprintCallable, Category = "!Shop")
	void SetSelected(bool bInSelected);

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnMenuLanguageChanged() override { RefreshUI(); }

	UFUNCTION()
	void HandleSelectClicked();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void RefreshUI();
	void ApplySelectionVisual();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UTextBlock> Txt_Name = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UTextBlock> Txt_Price = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UTextBlock> Txt_State = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UImage> IconImage = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UImage> SelectionBorderImage = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UButton> Btn_Select = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Style")
	FLinearColor SelectionBorderDefaultColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Style")
	FLinearColor SelectionBorderSelectedColor = FLinearColor(0.0f, 0.45f, 1.0f, 1.0f);

private:
	UPROPERTY(Transient)
	TObjectPtr<UShopEntryViewData> EntryData = nullptr;

	bool bIsSelected = false;
};
