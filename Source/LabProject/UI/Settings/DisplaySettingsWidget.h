#pragma once

#include "CoreMinimal.h"
#include "Components/ComboBoxString.h"
#include "UI/Common/LocalizedMenuWidget.h"
#include "DisplaySettingsWidget.generated.h"

class UDisplaySettingsSubsystem;

/** 게임 설정의 화면 탭. 화면 모드·해상도·그래픽 품질·프레임 제한을 고르면 바로 적용·저장된다(UDisplaySettingsSubsystem). */
UCLASS()
class LABPROJECT_API UDisplaySettingsWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

protected:
	virtual void OnMenuLanguageChanged() override;

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION() void HandleWindowModeSelected(FString Option, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleResolutionSelected(FString Option, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleQualitySelected(FString Option, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleFrameRateLimitSelected(FString Option, ESelectInfo::Type SelectionType);
	UFUNCTION() UWidget* GenerateOption(FString Option);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	/** 지금 설정으로 네 콤보의 목록과 고른 줄을 다시 채운다. 언어가 바뀌어도 부른다(줄 이름이 번역문이다). */
	void RefreshOptions();
	void RefreshResolutionOptions();
	bool IsUserSelection(ESelectInfo::Type SelectionType) const;
	UDisplaySettingsSubsystem* GetDisplaySettings() const;

protected:
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> CB_WindowMode;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> CB_Resolution;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> CB_GraphicsQuality;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> CB_FrameRateLimit;

private:
	/** CB_Resolution의 줄 순서대로 둔 해상도. */
	TArray<FIntPoint> ResolutionOptions;
	bool bSynchronizing = false;
};
