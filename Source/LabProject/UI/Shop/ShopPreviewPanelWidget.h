#pragma once

#include "UI/Common/LocalizedMenuWidget.h"
#include "CoreMinimal.h"
#include "ShopPreviewPanelWidget.generated.h"

class UButton;
class UImage;
class UShopEntryViewData;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShopPreviewBuyRequested, UShopEntryViewData*, EntryData);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UShopPreviewPanelWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!Shop")
	void SetEntryData(UShopEntryViewData* InEntryData);

	UFUNCTION(BlueprintCallable, Category = "!Shop")
	void SetMessage(const FText& Message);

	UFUNCTION(BlueprintCallable, Category = "!Shop")
	void RefreshUI();

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnMenuLanguageChanged() override { RefreshUI(); }

	UFUNCTION()
	void HandleBuyClicked();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ResolveWidgets();
	void CaptureDefaultNameColor();
	void CaptureDefaultPriceColor();
	void ConfigureDescriptionTextBlock();
	void ApplyPriceColor(const struct FShopEntryUiData* UiData);
	void ApplyMessage();

public:
	UPROPERTY(BlueprintAssignable, Category = "!Shop")
	FOnShopPreviewBuyRequested OnBuyRequested;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UTextBlock> Txt_Name = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UTextBlock> Txt_Description = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UTextBlock> Txt_Price = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UTextBlock> Txt_State = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UTextBlock> Txt_Message = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UImage> Img_Icon = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Shop|Bind")
	TObjectPtr<UButton> Btn_Buy = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Style")
	FSlateColor NotEnoughGoldPriceColor = FSlateColor(FLinearColor::Red);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Style")
	bool bAutoWrapDescription = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop|Style", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DescriptionWrapTextAt = 0.0f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UShopEntryViewData> EntryData = nullptr;

	FText MessageText;

	FSlateColor DefaultNameColor = FSlateColor(FLinearColor::White);
	FSlateColor DefaultPriceColor = FSlateColor(FLinearColor::White);
	bool bDefaultNameColorCaptured = false;
	bool bDefaultPriceColorCaptured = false;
};
