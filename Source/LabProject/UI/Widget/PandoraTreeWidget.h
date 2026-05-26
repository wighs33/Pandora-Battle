#pragma once

#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"

#include "PandoraTreeWidget.generated.h"

class AActor;
class UInputAction;
class UButton;
class UPanelWidget;
class UPandoraTreeComponent;
class UPandoraDefinition;
class UPandoraTreeViewModel;
class UWidget;
class UWidgetAnimation;

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

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void HidePandoraTree();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void ResetPandora();

	UPanelWidget* GetPandoraDescriptionPopupPanel() const;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, DisplayName = "Reset Pandora Button"), Category = "!UI|Pandora|Widgets")
	TObjectPtr<UButton> ResetPandoraButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, DisplayName = "Pandora Description Popup"), Category = "!UI|Pandora|Widgets")
	TObjectPtr<UPanelWidget> PandoraDescriptionPopup;

	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional), Category = "!UI|Pandora|Animation")
	TObjectPtr<UWidgetAnimation> SlideInLeft;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora")
	FText PandoraPointsFormat = NSLOCTEXT("PandoraTreeWidget", "PandoraPointsFormat", "Pandora Points Available: {0}");

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

	void ResolvePandoraTreeComponent();
	void ResolveControlWidgets();
	UPandoraTreeViewModel* GetOrCreatePandoraTreeViewModel();
	void ApplyPandoraTreeViewModelToMvvmView();
	void BindPandoraTreeEvents();
	void UnbindPandoraTreeEvents();
	void BindButtonEvents();
	void UnbindButtonEvents();
	void ApplyPandoraDefinitionToComponent();
	void RefreshPandoraWidget(UWidget* Widget) const;
	void ResolveTogglePandoraTreeAction();
	bool IsTogglePandoraTreeKey(const FKey& Key) const;
	void ResolveCharacterPreviewClass();
	void SpawnCharacterPreview();
	void ReturnCameraToPawn(float BlendTime) const;
	void DestroyCharacterPreview();
	void FinishHidePandoraTree();
	void ClearHideTimer();

	FTimerHandle HideTimerHandle;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "!UI|Pandora|ViewModel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPandoraTreeViewModel> PandoraTreeViewModel;
};
