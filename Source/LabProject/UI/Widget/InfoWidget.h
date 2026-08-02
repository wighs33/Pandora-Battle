#pragma once

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "UI/InfoUiTypes.h"
#include "UI/Widget/LeftEquipmentWidget.h"
#include "UI/Widget/LeftPandoraWidget.h"
#include "UI/Widget/LeftProfileWidget.h"
#include "UI/Widget/LeftSkinWidget.h"
#include "UI/Widget/RightPandoraWidget.h"
#include "UI/Widget/RightInventoryWidget.h"
#include "UI/Widget/RightSkinWidget.h"
#include "UI/Widget/RightStatusWidget.h"
#include "InfoWidget.generated.h"

class UButton;
class UDragDropOperation;
class UItemDetailWidget;
class UItemInstance;
class UMapWidget;
class UOverlay;
class UPandoraDescriptionWidget;
class UPandoraInstance;
class USizeBox;
class USkinDefinition;
class USkinInstance;
class UWidget;
class UWidgetAnimation;
class UWidgetSwitcher;
class AActor;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPdOnClickedInfoCenterButton, FGameplayTag, LeftUiTag, FGameplayTag, RightUiTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdOnClickedMapButton);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdOnClickedSettingButton);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnDroppedItemToCharacterPanel, UItemInstance*, ItemInstance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnDroppedSkinToCharacterPanel, USkinInstance*, SkinInstance);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UInfoWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Tab Selection
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

	void FocusSection(EPdInfoUiSection Section, bool bAnimateTransition);
	EPdInfoUiSection GetFocusedSection() const { return FocusedSection; }

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

	UFUNCTION(BlueprintPure, Category = "!UI|Info")
	URightInventoryWidget* GetRightInventoryWidget() const;

	UFUNCTION(BlueprintPure, Category = "!UI|Info")
	URightStatusWidget* GetRightStatusWidget() const;

	UFUNCTION(BlueprintPure, Category = "!UI|Info")
	ULeftEquipmentWidget* GetLeftEquipmentWidget() const;

	UFUNCTION(BlueprintPure, Category = "!UI|Info")
	ULeftSkinWidget* GetLeftSkinWidget() const;

	UFUNCTION(BlueprintPure, Category = "!UI|Info")
	ULeftPandoraWidget* GetLeftPandoraWidget() const;

	UFUNCTION(BlueprintPure, Category = "!UI|Info")
	URightSkinWidget* GetRightSkinWidget() const;

	UFUNCTION(BlueprintPure, Category = "!UI|Info")
	URightPandoraWidget* GetRightPandoraWidget() const;

	UFUNCTION(BlueprintCallable, Category = "!UI|Info|Detail")
	void ShowItemDetailAtWidget(UItemInstance* ItemInstance, UWidget* AnchorWidget, bool bPlaceLeftOfWidget);

	UFUNCTION(BlueprintCallable, Category = "!UI|Info|Detail")
	void ShowSkinDetailAtWidget(USkinInstance* SkinInstance, UWidget* AnchorWidget, bool bPlaceLeftOfWidget);

	UFUNCTION(BlueprintCallable, Category = "!UI|Info|Detail")
	void ShowPandoraDescriptionDetailAtWidget(UPandoraInstance* PandoraInstance, UWidget* AnchorWidget, bool bPlaceLeftOfWidget);

	void ShowPandoraDescriptionDetailImmediatelyAtWidget(
		UPandoraInstance* PandoraInstance,
		UWidget* AnchorWidget,
		bool bPlaceLeftOfWidget);

	void ShowSkinDefinitionDetailAtWidget(const USkinDefinition* SkinDefinition, UWidget* AnchorWidget, bool bPlaceLeftOfWidget);

	UFUNCTION(BlueprintCallable, Category = "!UI|Info|Detail")
	void HideDetailWidgets();

	void HidePandoraDescriptionDetailAtWidget(const UWidget* AnchorWidget);

	UPROPERTY(BlueprintAssignable, Category = "!UI|Info")
	FPdOnClickedInfoCenterButton OnClickedInfoCenterButton;

	UPROPERTY(BlueprintAssignable, Category = "!UI|Info")
	FPdOnClickedMapButton OnClickedMapButton;

	UPROPERTY(BlueprintAssignable, Category = "!UI|Info")
	FPdOnClickedSettingButton OnClickedSettingButton;

	UPROPERTY(BlueprintAssignable, Category = "!UI|Info")
	FPdOnDroppedItemToCharacterPanel OnDroppedItemToCharacterPanel;

	UPROPERTY(BlueprintAssignable, Category = "!UI|Info")
	FPdOnDroppedSkinToCharacterPanel OnDroppedSkinToCharacterPanel;

protected:
	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	//------------------------------------------------------------------------------------------------------------------
	//--- Layout
	UFUNCTION(BlueprintCallable, Category = "!UI|Info|Layout")
	void ApplyInfoLayout();

	UFUNCTION(BlueprintPure, Category = "!UI|Info|Layout")
	UWidget* GetCenterPreviewPanel() const { return CenterPreviewPanel; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Layout")
	bool bAutoApplyInfoLayout = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Layout")
	FVector2D DesignResolution = FVector2D(1920.0f, 1080.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Layout", meta = (ClampMin = "0.0"))
	float LeftPanelWidth = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Layout", meta = (ClampMin = "0.0"))
	float RightPanelWidth = 560.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Layout", meta = (ClampMin = "0.0"))
	float MinCenterPreviewWidth = 640.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Layout", meta = (ClampMin = "0.0"))
	float BottomNavigationReservedHeight = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Layout", meta = (ClampMin = "0.0"))
	float BottomTabBarWidth = 720.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Layout", meta = (ClampMin = "0.0"))
	float BottomTabBarHeight = 72.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Layout", meta = (ClampMin = "0.0"))
	float BottomTabBarBottomPadding = 24.0f;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info|Layout", meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> DesignRootSizeBox;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info|Layout", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LeftCurtainPanel;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info|Layout", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CenterPreviewPanel;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info|Layout", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CharacterPanel;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info|Layout", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RightCurtainPanel;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info|Layout", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BottomTabBar;

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

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Info|Character Preview")
	TObjectPtr<AActor> SpawnedCharacterPreview;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Detail")
	TSubclassOf<UItemDetailWidget> ItemDetailWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Detail")
	TSubclassOf<UPandoraDescriptionWidget> PandoraDescriptionWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Detail")
	FVector2D DetailPopupOffset = FVector2D(18.0f, 0.0f);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Info|Detail")
	TObjectPtr<UItemDetailWidget> ItemDetailWidget;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Info|Detail")
	TObjectPtr<UPandoraDescriptionWidget> PandoraDescriptionWidget;

	//------------------------------------------------------------------------------------------------------------------
	//--- Root Widgets
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> LeftWidgetSwitcher;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> RightWidgetSwitcher;

	//------------------------------------------------------------------------------------------------------------------
	//--- Tab Buttons
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
	TObjectPtr<UButton> Btn_Setting;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Close;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_PandoraUpgrade;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_CanvasExport;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_FaceDecal;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Debug;

	//------------------------------------------------------------------------------------------------------------------
	//--- Map
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info|Map", meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> MapOverlay;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info|Map", meta = (BindWidgetOptional))
	TObjectPtr<UMapWidget> TotalMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Map")
	TSubclassOf<UMapWidget> TotalMapWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|Map")
	TArray<FName> MapButtonDisabledMapNames = { TEXT("LV_TrainingRoom") };

	//------------------------------------------------------------------------------------------------------------------
	//--- Left Pages
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<ULeftProfileWidget> WB_LeftProfile;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<ULeftEquipmentWidget> WB_LeftEquipment;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<ULeftSkinWidget> WB_LeftSkin;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<ULeftPandoraWidget> WB_LeftPandora;

	//------------------------------------------------------------------------------------------------------------------
	//--- Right Pages
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<URightStatusWidget> WB_RightStatus;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<URightInventoryWidget> WB_RightInventory;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<URightSkinWidget> WB_RightSkin;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<URightPandoraWidget> WB_RightPandora;

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Button Callbacks
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
	void OnSettingButtonClicked();

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
	void HandleMapSlideAnimationFinished();

	UFUNCTION()
	void HandlePaintCanvasGroupVisibilityChanged(bool bVisible);

	bool IsMapButtonDisabledForCurrentMap() const;
	void BeginMapUiContentPreload();
	void BeginMapWidgetClassPreload(int32 PreloadGeneration);
	void ReleaseMapUiContentPreloads();
	void ApplyWidgetDefinitionSettings();
	void RefreshMapButtonEnabledState();
	bool EnsureMapOverlay();
	TSubclassOf<UMapWidget> ResolveTotalMapWidgetClassForCurrentMap() const;
	void EnsureTotalMapWidget();
	void HideMapOverlayImmediately();
	void PlayMapSlideInAnimation();
	void PlayMapSlideOutAnimation();
	void TickMapSlideAnimation();
	void FinishMapSlideOutAnimation();
	void PlaySidePanelsSlideInAnimation();
	void PlaySidePanelsSlideOutAnimation();
	void SelectInfoCenterPage(UWidget* LeftWidget, UWidget* RightWidget, const FGameplayTag& LeftUiTag, const FGameplayTag& RightUiTag);
	void HideSkinPaintCanvasGroup();
	void SetCanvasExportButtonVisible(bool bVisible) const;
	void SetPandoraUpgradeButtonVisible(bool bVisible) const;
	void BindLeftSkinPaintCanvasEvents();
	void UnbindLeftSkinPaintCanvasEvents();
	bool IsScreenPositionInsideCharacterDropPanel(const FVector2D& ScreenSpacePosition) const;
	void ResolveCharacterPreviewClass();
	void SpawnCharacterPreview();
	void ReturnCameraToPawn() const;
	void DestroyCharacterPreview();
	UItemDetailWidget* GetOrCreateItemDetailWidget();
	UPandoraDescriptionWidget* GetOrCreatePandoraDescriptionWidget();
	void ShowPandoraDescriptionDetailAtWidgetInternal(
		UPandoraInstance* PandoraInstance,
		UWidget* AnchorWidget,
		bool bPlaceLeftOfWidget,
		bool bPlayShowAnimation);
	void PositionDetailWidgetAdjacentToWidget(UUserWidget* DetailWidget, const UWidget* AnchorWidget, bool bPlaceLeftOfWidget) const;
	UItemInstance* ResolveEquippedItemForComparison(UItemInstance* HoveredItem) const;
	FGameplayTag GetProfileLeftUiTag() const;
	FGameplayTag GetProfileRightUiTag() const;
	FGameplayTag GetItemLeftUiTag() const;
	FGameplayTag GetItemRightUiTag() const;
	FGameplayTag GetSkinLeftUiTag() const;
	FGameplayTag GetSkinRightUiTag() const;
	FGameplayTag GetPandoraLeftUiTag() const;
	FGameplayTag GetPandoraRightUiTag() const;

	FTimerHandle MapSlideTimerHandle;
	double MapSlideStartTime = 0.0;
	bool bMapOverlayOpen = false;
	bool bMapSlideReverse = false;
	bool bMapUiContentReady = false;
	int32 MapUiContentPreloadGeneration = 0;
	TSharedPtr<FStreamableHandle> MapRulePreloadHandle;
	TSharedPtr<FStreamableHandle> MapWidgetClassPreloadHandle;
	EPdInfoUiSection FocusedSection = EPdInfoUiSection::Profile;

	TWeakObjectPtr<UWidget> ActivePandoraDescriptionAnchor;
	TWeakObjectPtr<UPandoraInstance> ActivePandoraDescriptionInstance;
};
