#pragma once

#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"

#include "PandoraTreeWidget.generated.h"

class AActor;
class UInputAction;
class UButton;
class UPandoraDescriptionWidget;
class UPandoraTreeComponent;
class UPandoraDefinition;
class UPandoraTreeWidget;
class UPandoraTreeViewModel;
class UPandoraWidget;
class UWidget;
class UWidgetAnimation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPandoraTreeClosedSignature, UPandoraTreeWidget*, PandoraTreeWidget);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UPandoraTreeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPandoraTreeWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetPandoraDefinition(UPandoraDefinition* InPandoraDefinition);

	UFUNCTION(BlueprintPure, Category = "!UI|Pandora")
	UPandoraDefinition* GetPandoraDefinition() const { return PandoraDefinition.Get(); }

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetPandoraPointsText();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void RefreshPandoraWidgets();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void ShowPandoraTree();
	void SetInputModeManagedExternally(bool bManagedExternally);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void HidePandoraTree();

	void HidePandoraTreeImmediately();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora|Character Preview")
	void SetReturnCameraOnHide(bool bInReturnCameraOnHide) { bReturnCameraOnHide = bInReturnCameraOnHide; }

	UFUNCTION(BlueprintPure, Category = "!UI|Pandora|Character Preview")
	float GetPreviewCameraShowBlendTime() const { return PreviewCameraShowBlendTime; }

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void ResetPandora();

	UPROPERTY(BlueprintAssignable, Category = "!UI|Pandora")
	FPandoraTreeClosedSignature OnPandoraTreeClosed;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, DisplayName = "Reset Pandora Button"), Category = "!UI|Pandora|Widgets")
	TObjectPtr<UButton> ResetPandoraButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Widgets")
	TObjectPtr<UButton> Btn_Loadout;

	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional), Category = "!UI|Pandora|Animation")
	TObjectPtr<UWidgetAnimation> SlideInLeft;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora")
	FText PandoraPointsFormat = NSLOCTEXT("PandoraTreeWidget", "PandoraPointsFormat", "Soul Dust: {0}");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora")
	TObjectPtr<UPandoraDefinition> PandoraDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Input")
	TObjectPtr<UInputAction> TogglePandoraTreeAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Input")
	bool bCloseOnToggleKey = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora")
	bool bSetInputModeOnShowHide = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Animation", meta = (ClampMin = "0.0"))
	float HideAnimationDelay = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Character Preview")
	bool bUseCharacterPreviewCamera = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Character Preview")
	TSubclassOf<AActor> CharacterPreviewClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Character Preview", meta = (ClampMin = "0.0"))
	float PreviewCameraShowBlendTime = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Character Preview", meta = (ClampMin = "0.0"))
	float PreviewCameraHideBlendTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Character Preview")
	bool bReturnCameraOnHide = true;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Pandora")
	TObjectPtr<UPandoraTreeComponent> PandoraTreeComponent;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Pandora|Character Preview")
	TObjectPtr<AActor> SpawnedCharacterPreview;

private:
	UFUNCTION()
	void HandlePandoraStateChanged();

	UFUNCTION()
	void HandlePandoraPointsChanged(int32 NewPointsAvailable);

	UFUNCTION()
	void HandleResetPandoraClicked();

	UFUNCTION()
	void HandleLoadoutClicked();

	UFUNCTION()
	void HandlePandoraDescriptionRequested(UPandoraWidget* PandoraWidget);

	UFUNCTION()
	void HandlePandoraDescriptionDismissed(UPandoraWidget* PandoraWidget);

	UFUNCTION()
	void HandlePandoraTreeFocusRequested(UPandoraWidget* PandoraWidget);

	void ResolvePandoraTreeComponent();
	void ApplyWidgetDefinitionSettings();
	void ResolveControlWidgets();
	UPandoraTreeViewModel* GetOrCreatePandoraTreeViewModel();
	void ApplyPandoraTreeViewModelToMvvmView();
	void BindPandoraTreeEvents();
	void UnbindPandoraTreeEvents();
	void BindButtonEvents();
	void UnbindButtonEvents();
	void ApplyPandoraDefinitionToComponent();
	void RefreshPandoraWidget(UWidget* Widget);
	void BindPandoraWidgetEvents(UPandoraWidget* PandoraWidget);
	void UnbindPandoraWidgetEvents();
	void ShowPandoraDescriptionAtWidget(
		UPandoraDefinition* InPandoraDefinition,
		UPandoraTreeComponent* InPandoraTreeComponent,
		const UWidget* AnchorWidget);
	void HidePandoraDescription(const UWidget* RequestingAnchorWidget = nullptr);
	void RefreshActivePandoraDescription();
	void PrunePandoraDescriptionRequests();
	void ShowTopRequestedPandoraDescription();
	void ResolveTogglePandoraTreeAction();
	bool IsTogglePandoraTreeKey(const FKey& Key) const;
	void ResolveCharacterPreviewClass();
	void SpawnCharacterPreview();
	void ReturnCameraToPawn(float BlendTime) const;
	void DestroyCharacterPreview();
	UPandoraDescriptionWidget* GetOrCreatePandoraDescriptionWidget();
	void PositionPandoraDescriptionWidget(const UWidget* AnchorWidget) const;
	bool ShouldManageInputModeInternally() const;
	bool ApplyRoutedPandoraInput();
	bool ReleaseRoutedPandoraInput();
	void PrepareToHidePandoraTree();
	void FinishHidePandoraTree();
	void ClearHideTimer();

	FTimerHandle HideTimerHandle;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "!UI|Pandora|ViewModel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPandoraTreeViewModel> PandoraTreeViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UPandoraDescriptionWidget> PandoraDescriptionWidget;

	TWeakObjectPtr<UWidget> ActivePandoraDescriptionAnchor;
	TWeakObjectPtr<UPandoraDefinition> ActivePandoraDescriptionDefinition;
	TArray<TWeakObjectPtr<UPandoraWidget>> PandoraDescriptionRequestStack;
	FGuid PandoraModalInputToken;
	bool bInputModeManagedExternally = false;
};
