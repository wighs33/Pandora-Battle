#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MenuPopupWidget.generated.h"

class UButton;
class UAudioVolumeControl;
class UAudioVolumeSlider;
class UGuideWidget;
class UOnlineSessionsSubsystem;
class USlider;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMenuPopupClosedSignature, UMenuPopupWidget*, MenuPopupWidget);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UMenuPopupWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UMenuPopupWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "!Menu")
	virtual void CloseMenu();

	UFUNCTION(BlueprintCallable, Category = "!Menu")
	virtual void ExitToTitleMap();

	bool CloseGuide();
	UWidget* GetActiveGuideWidget() const;

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	virtual void HandleResumeClicked();

	UFUNCTION()
	virtual void HandleExitClicked();

	UFUNCTION()
	void HandleGuideClicked();

	UFUNCTION()
	void HandleGuideClosed(UGuideWidget* ClosedGuideWidget);

	UFUNCTION()
	void HandleMouseSensitivityChanged(float NormalizedValue);
	void HandleDestroySessionForExit(bool bWasSuccessful);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void SetRequestedPause(bool bPaused);
	FString GetResolvedTitleTravelMapName() const;
	void TravelToTitleMap();
	void ClearDestroySessionDelegate();

private:
	void OpenGuide();
	void DiscardGuideWidget();
	void RestoreMenuAfterGuide();
	void InitializeMouseSensitivitySlider();
	void ShutdownMouseSensitivitySlider();

public:
	UPROPERTY(BlueprintAssignable, Category = "!Menu")
	FMenuPopupClosedSignature OnMenuClosed;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Menu|Bind")
	TObjectPtr<UButton> Btn_Resume;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Menu|Bind")
	TObjectPtr<UButton> Btn_Exit;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Menu|Bind")
	TObjectPtr<UButton> Btn_Guide;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Menu|Bind")
	TObjectPtr<UAudioVolumeSlider> AudioVolumeSlider_;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Menu|Bind")
	TObjectPtr<UButton> Btn_Sound;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Menu|Bind")
	TObjectPtr<UWidget> MouseSlider;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Menu|Exit")
	bool bDestroySessionOnExit = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Menu|Pause")
	bool bPauseGameWhenOpened = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Menu|Pause", meta = (EditCondition = "bPauseGameWhenOpened"))
	bool bAllowNetworkPause = false;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAudioVolumeControl> AudioVolumeControl;

	UPROPERTY(Transient)
	TObjectPtr<UGuideWidget> ActiveGuideWidget;

	UPROPERTY(Transient)
	TObjectPtr<USlider> StandardMouseSlider;

	UPROPERTY(Transient)
	TObjectPtr<UAudioVolumeSlider> AudioMouseSlider;

	FDelegateHandle DestroySessionCompleteHandle;
	bool bAppliedPause = false;
};
