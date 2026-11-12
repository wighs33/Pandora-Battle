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

	/** 고른 상품에만 보이는 표시(메달 뒤 후광과 이름 밑줄). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UImage> SelectionBorderImage = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UButton> Btn_Select = nullptr;

private:
	UPROPERTY(Transient)
	TObjectPtr<UShopEntryViewData> EntryData = nullptr;

	bool bIsSelected = false;
};
