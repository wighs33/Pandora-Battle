#pragma once

#include "Blueprint/UserWidget.h"
#include "TimerManager.h"

#include "PandoraWidget.generated.h"

class UButton;
class UProgressBar;
class UPanelWidget;
class UUserWidget;
class UPandoraTreeComponent;
class UPandoraDefinition;
class UPandoraTreeWidget;
class UPandoraWidgetViewModel;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UPandoraWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetPandoraDefinition(UPandoraDefinition* InPandoraDefinition);

	UFUNCTION(BlueprintPure, Category = "!UI|Pandora")
	UPandoraDefinition* GetPandoraDefinition() const { return PandoraDefinition.Get(); }

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetPandoraTreeComponent(UPandoraTreeComponent* InPandoraTreeComponent);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetPandoraInfo();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora|Button Hold")
	void ConfirmSpendPointOnPandora();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora|Button Hold")
	void IncrementButtonTimer();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora|Button Hold")
	void ResetButtonPress();

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Widgets")
	TObjectPtr<UButton> Button;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Widgets")
	TObjectPtr<UProgressBar> ButtonProgressBar;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true"), Category = "!UI|Pandora")
	TObjectPtr<UPandoraDefinition> PandoraDefinition;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "!UI|Pandora|Internal")
	TObjectPtr<UPandoraTreeComponent> PandoraTreeComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style")
	FLinearColor AvailableOverlayColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style")
	FLinearColor UnavailableOverlayColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.55f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style")
	FLinearColor LockedOverlayColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.65f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style")
	FLinearColor NotEnoughPointsOverlayColor = FLinearColor(0.75f, 0.0f, 0.0f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AvailableContentOpacity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UnavailableContentOpacity = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style")
	bool bShowMaxText = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Button Hold", meta = (ClampMin = "0.0"))
	double ButtonHoldDuration = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Button Hold", meta = (ClampMin = "0.001"))
	float ButtonHoldUpdateInterval = 0.033333f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Description")
	TSubclassOf<UUserWidget> PandoraDescriptionPopupWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Description", meta = (ClampMin = "0.01"))
	float FocusCheckInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Description")
	FVector2D DescriptionPopupLocalOffset = FVector2D(120.0f, 0.0f);

private:
	UFUNCTION()
	void HandleButtonPressed();

	UFUNCTION()
	void HandleButtonReleased();

	UFUNCTION()
	void HandlePandoraStateChanged();

	UFUNCTION()
	void HandlePandoraPointsChanged(int32 NewPointsAvailable);

	void ResolvePandoraTreeComponent();
	void BindPandoraTreeEvents();
	void UnbindPandoraTreeEvents();
	void BindButtonEvents();
	void UnbindButtonEvents();
	void ResolveControlWidgets();
	UPandoraWidgetViewModel* GetOrCreatePandoraWidgetViewModel();
	void ApplyPandoraWidgetViewModelToMvvmView();
	void ApplyDesignerDefaults();
	void ClearButtonPressTimer();
	void SetupPandoraDescriptionPopupWidget();
	void ResolvePandoraTreeWidget();
	void ResolvePandoraDescriptionPopupWidgetClass();
	void StartFocusCheckTimer();
	void ClearFocusCheckTimer();
	void UpdatePandoraDescriptionDetails();
	UPanelWidget* GetPandoraDescriptionPopupPanel() const;

	UFUNCTION()
	void CheckFocusState();

	void ShowPandoraDescriptionPopup();
	void RemovePandoraDescriptionPopup();

	double ButtonHoldElapsedTime = 0.0;
	FTimerHandle ButtonHoldTimerHandle;
	FTimerHandle FocusCheckTimerHandle;

	UPROPERTY(Transient)
	TObjectPtr<UPandoraTreeWidget> ResolvedPandoraTreeWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CreatedPandoraDescriptionPopup;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "!UI|Pandora|ViewModel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPandoraWidgetViewModel> PandoraWidgetViewModel;
};
