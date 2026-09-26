#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/LocalizedMenuWidget.h"
#include "Components/ComboBoxString.h"
#include "GameSettingsWidget.generated.h"

class USlider;
class UTextBlock;

/** Personal settings only. Lobby match configuration remains host-owned. */
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

protected:
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> CB_SettingsLanguage;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_Done;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<USlider> Slider_MasterVolume;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_VolumeValue;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_SettingsHint;

private:
	FDelegateHandle VolumeChangedHandle;
	bool bSynchronizing = false;
	bool bLanguageSaveFailed = false;
};
