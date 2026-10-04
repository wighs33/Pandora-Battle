#pragma once

#include "CoreMinimal.h"
#include "UI/Common/LocalizedMenuWidget.h"
#include "Components/ComboBoxString.h"
#include "GameSettingsWidget.generated.h"

class UAudioVolumeControl;
class UGuideWidget;
class USlider;
class UTextBlock;
class UWidgetSwitcher;

/** 개인 설정만 담당한다. 로비 경기 설정은 호스트가 관리한다. 훈련장에서는 훈련 봇 설정 탭이 함께 보인다. */
UCLASS()
class LABPROJECT_API UGameSettingsWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UWidget* GetInitialFocusTarget() const { return CB_SettingsLanguage; }

	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category="UI|Settings") void CloseSettings();

protected:
	virtual void OnMenuLanguageChanged() override;

private:
	UFUNCTION() void HandleLanguageSelected(FString Option, ESelectInfo::Type SelectionType);
	UFUNCTION() UWidget* GenerateLanguageOption(FString Option);
	UFUNCTION() void HandleVolumeChanged(float Value);
	void RefreshVolume(int32 Percent);
	UFUNCTION() void HandleMouseSensitivityChanged(float Value);
	void RefreshMouseSensitivityText();
	UFUNCTION() void OpenGuide();
	UFUNCTION() void HandleGuideClosed(UGuideWidget* ClosedGuideWidget);
	void CloseGuide();
	UFUNCTION() void ShowGeneralPage();
	UFUNCTION() void ShowTrainingRoomPage();
	void ShowPage(int32 PageIndex);

protected:
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> CB_SettingsLanguage;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_Done;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<USlider> Slider_MasterVolume;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_VolumeValue;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_SettingsHint;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_Sound;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<USlider> Slider_MouseSensitivity;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_MouseSensitivityValue;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_Guide;

	/** 일반·훈련장 탭 버튼 묶음. 훈련장에서만 보인다. */
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> SettingsTabs;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_TabGeneral;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_TabTrainingRoom;

	/** 0번은 일반 설정, 1번은 훈련장 설정 페이지. */
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidgetSwitcher> Switcher_SettingsPages;

	UPROPERTY(EditDefaultsOnly, Category="UI|Settings") FLinearColor SelectedTabColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, Category="UI|Settings") FLinearColor UnselectedTabColor = FLinearColor(0.45f, 0.4f, 0.5f, 1.f);

private:
	UPROPERTY(Transient) TObjectPtr<UAudioVolumeControl> SoundButtonControl;
	UPROPERTY(Transient) TObjectPtr<UGuideWidget> ActiveGuideWidget;

	FDelegateHandle VolumeChangedHandle;
	bool bSynchronizing = false;
	bool bLanguageSaveFailed = false;
};
