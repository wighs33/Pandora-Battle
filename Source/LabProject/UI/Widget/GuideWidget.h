#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "Definition/UI/GuideDefinition.h"
#include "GuideWidget.generated.h"

class UButton;
class UGuideWidget;
struct FStreamableHandle;
class UTextBlock;
class UTexture2D;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGuideClosedSignature, UGuideWidget*, GuideWidget);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UGuideWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UGuideWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "!Guide")
	void RefreshGuide();

	UFUNCTION(BlueprintCallable, Category = "!Guide")
	void SelectGuidePage(int32 PageIndex);

	UFUNCTION(BlueprintCallable, Category = "!Guide")
	void CloseGuide();

	void SetOpenedFromGameplayMenu(bool bInOpenedFromGameplayMenu);

	UPROPERTY(BlueprintAssignable, Category = "!Guide")
	FGuideClosedSignature OnGuideClosed;

protected:
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Guide|Bind")
	TObjectPtr<UTextBlock> Txt_Content;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Guide|Bind")
	TObjectPtr<UWidget> ImageBorder;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Guide|Bind")
	TObjectPtr<UButton> Btn_Close;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Guide|Bind")
	TObjectPtr<UComboBoxString> CB_Language;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Guide|Bind")
	TObjectPtr<UWidget> Img_BackgroundPattern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Guide", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UGuideDefinition> GuideData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Guide|Bind")
	TArray<FName> GuideButtonWidgetNames;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleLanguageSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION() void HandleGuideButton0Clicked();
	UFUNCTION() void HandleGuideButton1Clicked();
	UFUNCTION() void HandleGuideButton2Clicked();
	UFUNCTION() void HandleGuideButton3Clicked();
	UFUNCTION() void HandleGuideButton4Clicked();
	UFUNCTION() void HandleGuideButton5Clicked();
	UFUNCTION() void HandleGuideButton6Clicked();
	UFUNCTION() void HandleGuideButton7Clicked();
	UFUNCTION() void HandleGuideButton8Clicked();
	UFUNCTION() void HandleGuideButton9Clicked();
	UFUNCTION() void HandleGuideButton10Clicked();
	UFUNCTION() void HandleGuideButton11Clicked();

	void ResolveWidgets();
	void ApplyBackgroundPatternVisibility();
	void BeginContentPreload();
	void BeginPageImagePreload(int32 PreloadGeneration);
	void ReleaseContentPreloads();
	void RebuildPages();
	void BindGuideButtons();
	void UnbindGuideButtons();
	void BindGuideButton(int32 PageIndex, UButton* Button);
	void UnbindGuideButton(int32 PageIndex, UButton* Button);
	UButton* FindButtonForPage(int32 PageIndex, const FGuidePageEntry& Page) const;
	void ApplyPage(const FGuidePageEntry& Page);
	void ApplyImage(UTexture2D* Texture);
	void SelectBoundGuideButton(int32 PageIndex);
	void ResolveSelectedLanguage();
	FText ResolvePageContent(const FGuidePageEntry& Page) const;

	UPROPERTY(Transient)
	TArray<FGuidePageEntry> CachedPages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> BoundGuideButtons;

	EGuideLanguage CurrentLanguage = EGuideLanguage::Korean;
	bool bHasSelectedLanguage = false;
	int32 CurrentPageIndex = INDEX_NONE;
	int32 ContentPreloadGeneration = 0;
	ESlateVisibility DefaultBackgroundPatternVisibility = ESlateVisibility::Visible;
	bool bCapturedBackgroundPatternVisibility = false;
	bool bOpenedFromGameplayMenu = false;
	bool bIsClosing = false;
	TSharedPtr<FStreamableHandle> GuideDefinitionPreloadHandle;
	TSharedPtr<FStreamableHandle> GuideImagePreloadHandle;
};
