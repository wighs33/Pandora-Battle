#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "PdScreenWidget.generated.h"

class UButton;
class UMenuLocalizationExtension;
class UMenuLocalizationSubsystem;

/**
 * 화면 층(UUiSubsystem::PushScreen)에 올리는 화면의 기반. CommonUI 화면으로서 입력 설정·포커스·뒤로 가기를 맡는다.
 * 화면이 떠 있는 동안 이동·시점·게임플레이 입력은 막힌다. 메뉴 문구는 패널과 같은 다국어 부품(UMenuLocalizationExtension)을 붙여 쓴다.
 * 화면을 띄우는 쪽이 ConfigureScreen으로 포커스와 뒤로 가기를 정한 뒤 올린다. 정하지 않으면 메뉴 입력을 쓰고 뒤로 가기에 스스로 닫힌다.
 */
UCLASS(Abstract)
class LABPROJECT_API UPdScreenWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UPdScreenWidget(const FObjectInitializer& ObjectInitializer);

	/** FocusTarget이 없으면 CommonUI가 뷰포트에 포커스를 유지하여 누르고 있는 입력을 지킨다. 마우스는 기본으로 잡지 않는다. */
	void ConfigureScreen(UWidget* FocusTarget, FSimpleDelegate BackAction, ECommonInputMode InputMode = ECommonInputMode::Menu,
		EMouseCaptureMode MouseCapture = EMouseCaptureMode::NoCapture);

	/** 이 화면에서 연 게임 설정 화면을 닫는다. 연 것이 없으면 false. */
	bool CloseGameSettings();

	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void OpenGameSettings();

protected:
	/** 언어가 바뀌어 연결한 문구를 다시 채운 뒤 부른다. 코드에서 조합한 문구는 여기서 다시 만든다. */
	virtual void OnMenuLanguageChanged() {}

	// Internal Helpers ------------------------------------------------------------------------------------------------
	UMenuLocalizationSubsystem* GetLocalization() const;
	FText MenuText(FName Key) const;
	FText MenuTextOrFallback(FName Key, const FText& Fallback) const;
	void SetHoverTooltip(UWidget* Widget, const FText& Text);

protected:
	/** 아래 연결 표는 ULocalizedMenuWidget과 이름이 같아, 패널에서 화면으로 바꾼 위젯 애셋이 값을 그대로 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="!UI|Localization")
	TMap<FName, FName> MenuTextBindings;

	UPROPERTY(EditDefaultsOnly, Category="!UI|Localization")
	TMap<FName, FName> MenuTooltipBindings;

	UPROPERTY(EditDefaultsOnly, Category="!UI|Localization")
	TMap<FName, FName> MenuHintBindings;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="!UI|Settings")
	TObjectPtr<UButton> Btn_GameSettings;

private:
	FUIInputConfig ScreenInputConfig;
	TWeakObjectPtr<UWidget> ScreenFocusTarget;
	FSimpleDelegate ScreenBackAction;

	UPROPERTY(Transient)
	TObjectPtr<UMenuLocalizationExtension> MenuLocalization;
};
