#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MenuPopupWidget.generated.h"

class UButton;
class UAudioVolumeControl;
class UAudioVolumeSlider;
class UGuideWidget;
class UOnlineSessionsSubsystem;
class USlider;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMenuPopupClosedSignature, UMenuPopupWidget*, MenuPopupWidget);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UMenuPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMenuPopupWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "!Menu")
	virtual void CloseMenu();

	UFUNCTION(BlueprintCallable, Category = "!Menu")
	virtual void ExitToTitleMap();

	UFUNCTION(BlueprintCallable, Category = "!Menu|Input")
	void SetRestoreGameInputOnClose(bool bInRestoreGameInputOnClose);

	void SetInputModeManagedExternally(bool bManagedExternally);
	bool CloseGuide();
	UWidget* GetActiveGuideWidget() const;

	UPROPERTY(BlueprintAssignable, Category = "!Menu")
	FMenuPopupClosedSignature OnMenuClosed;

protected:
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

	void ApplyMenuInputMode();
	void ReleaseMenuInputMode();
	void RestoreGameInputMode() const;
	void SetRequestedPause(bool bPaused);
	FString GetResolvedTitleTravelMapName() const;
	void TravelToTitleMap();
	void ClearDestroySessionDelegate();
	void HandleDestroySessionForExit(bool bWasSuccessful);

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Menu|Input")
	bool bRestoreGameInputOnClose = true;

private:
	void OpenGuide();
	void DiscardGuideWidget();
	void RestoreMenuAfterGuide();
	void InitializeMouseSensitivitySlider();
	void ShutdownMouseSensitivitySlider();

	UPROPERTY(Transient)
	TObjectPtr<UAudioVolumeControl> AudioVolumeControl;

	UPROPERTY(Transient)
	TObjectPtr<UGuideWidget> ActiveGuideWidget;

	UPROPERTY(Transient)
	TObjectPtr<USlider> StandardMouseSlider;

	UPROPERTY(Transient)
	TObjectPtr<UAudioVolumeSlider> AudioMouseSlider;

	FDelegateHandle DestroySessionCompleteHandle;
	FGuid MenuModalInputToken;
	bool bAppliedPause = false;
	bool bInputModeManagedExternally = false;
};
