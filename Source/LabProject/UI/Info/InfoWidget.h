#pragma once

#include "UI/Common/LocalizedMenuWidget.h"
#include "GameplayTagContainer.h"
#include "Common/InfoUiTypes.h"
#include "UI/Info/Item/LeftEquipmentWidget.h"
#include "UI/Info/Pandora/LeftPandoraWidget.h"
#include "UI/Info/Status/LeftProfileWidget.h"
#include "UI/Info/Skin/LeftSkinWidget.h"
#include "UI/Info/Pandora/RightPandoraWidget.h"
#include "UI/Info/Item/RightInventoryWidget.h"
#include "UI/Info/Skin/RightSkinWidget.h"
#include "UI/Info/Status/RightStatusWidget.h"
#include "InfoWidget.generated.h"

class UButton;
class UCanvasPanel;
struct FPdButtonClickBinding;
class UPandoraTreeWidget;
class UDragDropOperation;
class UInfoCharacterPreview;
class UInfoDetailPopup;
class UInfoMapPanel;
class UInfoPaintCanvas;
class UItemDetailWidget;
class UItemInstance;
class UMapWidget;
class UOverlay;
class UPandoraDescriptionWidget;
class UPandoraDefinition;
class UPaintCanvasWidget;
class USkinDefinition;
class UTextBlock;
class UWidget;
class UWidgetAnimation;
class UWidgetSwitcher;
class AActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPdOnClickedInfoCenterButton, FGameplayTag, LeftUiTag, FGameplayTag, RightUiTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdOnClickedMapButton);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnDroppedItemToCharacterPanel, UItemInstance*, ItemInstance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnDroppedSkinToCharacterPanel, const USkinDefinition*, SkinDefinition);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UInfoWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UInfoWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "!UI|Info")
	void SelectProfileTab();

	UFUNCTION(BlueprintCallable, Category = "!UI|Info")
	void SelectItemTab();

	UFUNCTION(BlueprintCallable, Category = "!UI|Info")
	void SelectSkinTab();

	UFUNCTION(BlueprintCallable, Category = "!UI|Info")
	void SelectPandoraTab();

	UFUNCTION(BlueprintCallable, Category = "!UI|Info")
	void SelectMapTab();

	/** Mount the growth drawer in the right-hand space without replacing this screen. */
	bool AttachPandoraTree(UPandoraTreeWidget* Tree);
	UFUNCTION(BlueprintPure, Category="!UI|Info|Pandora")
	bool IsPandoraDrawerExpanded() const { return bPandoraDrawerExpanded; }

	void FocusSection(EInfoUiSection Section, bool bAnimateTransition);
	EInfoUiSection GetFocusedSection() const { return FocusedSection; }

	UFUNCTION(BlueprintCallable, Category = "!UI|Info|Animation")
	void ShowInfoUi();

	UFUNCTION(BlueprintCallable, Category = "!UI|Info|Animation")
	void HideInfoUi();

	UFUNCTION(BlueprintCallable, Category = "!UI|Info|Character Preview")
	void SetReturnCameraOnHide(bool bInReturnCameraOnHide) { bReturnCameraOnHide = bInReturnCameraOnHide; }

	UFUNCTION(BlueprintPure, Category = "!UI|Info|Animation")
	float GetHideAnimationDelay() const;

	UFUNCTION(BlueprintPure, Category = "!UI|Info|Character Preview")
	float GetPreviewCameraShowBlendTime() const { return 0.0f; }

	UFUNCTION(BlueprintPure, Category = "!UI|Info|Character Preview")
	float GetPreviewCameraHideBlendTime() const { return 0.0f; }

	URightInventoryWidget* GetRightInventoryWidget() const;

	URightStatusWidget* GetRightStatusWidget() const;

	ULeftEquipmentWidget* GetLeftEquipmentWidget() const;

	ULeftSkinWidget* GetLeftSkinWidget() const;

	ULeftPandoraWidget* GetLeftPandoraWidget() const;

	URightSkinWidget* GetRightSkinWidget() const;

	URightPandoraWidget* GetRightPandoraWidget() const;

	void ShowItemDetailAtWidget(UItemInstance* ItemInstance, UWidget* AnchorWidget, bool bPlaceLeftOfWidget);

	void ShowPandoraDescriptionDetailAtWidget(const UPandoraDefinition* PandoraDefinition, UWidget* AnchorWidget, bool bPlaceLeftOfWidget);

	void ShowPandoraDescriptionDetailImmediatelyAtWidget(
		const UPandoraDefinition* PandoraDefinition,
		UWidget* AnchorWidget,
		bool bPlaceLeftOfWidget);

	void ShowSkinDefinitionDetailAtWidget(const USkinDefinition* SkinDefinition, UWidget* AnchorWidget, bool bPlaceLeftOfWidget);

	void HideDetailWidgets();

	void HidePandoraDescriptionDetailAtWidget(const UWidget* AnchorWidget);

	// Event Handlers --------------------------------------------------------------------------------------------------
	void OnPandoraDrawerClosed();

protected:
	UFUNCTION()
	void OnClosePaintClicked();
	virtual void OnMenuLanguageChanged() override;

private:
	UFUNCTION()
	void OnProfileTabButtonClicked();

	UFUNCTION()
	void OnItemTabButtonClicked();

	UFUNCTION()
	void OnSkinButtonClicked();

	UFUNCTION()
	void OnPandoraButtonClicked();

	UFUNCTION()
	void OnMapButtonClicked();

	UFUNCTION()
	void OnCloseButtonClicked();

	UFUNCTION()
	void OnPandoraUpgradeButtonClicked();

	UFUNCTION()
	void OnCanvasExportButtonClicked();

	UFUNCTION()
	void OnFaceDecalButtonClicked();

	UFUNCTION()
	void OnDebugButtonClicked();

	UFUNCTION()
	void HandlePaintCanvasGroupVisibilityChanged(bool bVisible);

protected:

private:
	void EnsureInfoViews();
	void ConfigureInfoViews();
	void ApplyWidgetDefinitionSettings();
	void PlaySidePanelsSlideInAnimation();
	void PlaySidePanelsSlideOutAnimation();
	TArray<FPdButtonClickBinding, TInlineAllocator<12>> GetButtonBindings() const;
	// 왼쪽·오른쪽 패널을 쓰는 탭으로 바꾼다. 지도 패널은 밀어 닫고, 판도라 탭에서만 강화 버튼을 보인다.
	void SelectSidePanelSection(EInfoUiSection Section, UWidget* LeftWidget, UWidget* RightWidget, const FGameplayTag& LeftUiTag, const FGameplayTag& RightUiTag);
	void SelectInfoCenterPage(UWidget* LeftWidget, UWidget* RightWidget, const FGameplayTag& LeftUiTag, const FGameplayTag& RightUiTag);
	void HideSkinPaintCanvasGroup();
	bool SetPaintCanvasWidgetVisible(bool bVisible);
	void SetCanvasExportButtonVisible(bool bVisible) const;
	void SetPandoraUpgradeButtonVisible(bool bVisible);
	void RefreshPandoraDrawerLabel();
	void ClosePandoraDrawerForNavigation();
	void BindLeftSkinPaintCanvasEvents();
	void UnbindLeftSkinPaintCanvasEvents();
	bool IsScreenPositionInsideCharacterDropPanel(const FVector2D& ScreenSpacePosition) const;

public:
	UPROPERTY(BlueprintAssignable, Category = "!UI|Info")
	FPdOnClickedInfoCenterButton OnClickedInfoCenterButton;

	UPROPERTY(BlueprintAssignable, Category = "!UI|Info")
	FPdOnClickedMapButton OnClickedMapButton;

	UPROPERTY(BlueprintAssignable, Category = "!UI|Info")
	FPdOnDroppedItemToCharacterPanel OnDroppedItemToCharacterPanel;

	UPROPERTY(BlueprintAssignable, Category = "!UI|Info")
	FPdOnDroppedSkinToCharacterPanel OnDroppedSkinToCharacterPanel;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Paint")
	TObjectPtr<UButton> Btn_ClosePaint;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info|Layout", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CenterPreviewPanel;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info|Layout", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CharacterPanel;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Info|Animation", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> SlideInLeft;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Info|Animation", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> SlideInRight;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Info|Animation", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> SlideInBottom;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Info|Animation", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> SlideMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Animation", meta = (ClampMin = "0.0"))
	float HideAnimationDelay = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Animation")
	FVector2D MapSlideStartOffset = FVector2D(0.0f, -96.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Animation", meta = (ClampMin = "0.01"))
	float MapSlideDuration = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Character Preview")
	bool bUseCharacterPreviewCamera = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Character Preview")
	bool bReturnCameraOnHide = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Character Preview")
	TSubclassOf<AActor> CharacterPreviewClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Detail")
	TSubclassOf<UItemDetailWidget> ItemDetailWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Detail")
	TSubclassOf<UPandoraDescriptionWidget> PandoraDescriptionWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Detail")
	FVector2D DetailPopupOffset = FVector2D(18.0f, 0.0f);

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> LeftWidgetSwitcher;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> RightWidgetSwitcher;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<UButton> ProfileTabButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<UButton> ItemTabButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<UButton> SkinButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<UButton> PandoraButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<UButton> MapButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Close;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_PandoraUpgrade;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UCanvasPanel> PandoraTreeHost;
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PandoraDrawerAction;
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PandoraDrawerArrow;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_CanvasExport;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_FaceDecal;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Debug;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info|Paint", meta = (BindWidget))
	TObjectPtr<UPaintCanvasWidget> Canvas;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info|Paint", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Canvas;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info|Map", meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> MapOverlay;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info|Map", meta = (BindWidgetOptional))
	TObjectPtr<UMapWidget> TotalMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Map")
	TSubclassOf<UMapWidget> TotalMapWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<ULeftProfileWidget> WB_LeftProfile;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<ULeftEquipmentWidget> WB_LeftEquipment;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<ULeftSkinWidget> WB_LeftSkin;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<ULeftPandoraWidget> WB_LeftPandora;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<URightStatusWidget> WB_RightStatus;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<URightInventoryWidget> WB_RightInventory;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<URightSkinWidget> WB_RightSkin;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<URightPandoraWidget> WB_RightPandora;

private:
	friend class UInfoMapPanel;

	UPROPERTY(Transient)
	TObjectPtr<UInfoDetailPopup> DetailPopup;

	UPROPERTY(Transient)
	TObjectPtr<UInfoCharacterPreview> CharacterPreview;

	UPROPERTY(Transient)
	TObjectPtr<UInfoMapPanel> MapPanel;

	UPROPERTY(Transient)
	TObjectPtr<UInfoPaintCanvas> PaintCanvas;

	TWeakObjectPtr<UPandoraTreeWidget> AttachedPandoraTree;
	bool bPandoraDrawerExpanded = false;
	EInfoUiSection FocusedSection = EInfoUiSection::Profile;
};
