#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LocalizedMenuWidget.generated.h"

class UButton;
class UGameSettingsWidget;
class UMenuLocalizationSubsystem;

/** 화면 에셋은 위젯 이름을 텍스트 키에 연결하고, 기반 클래스는 수명과 실시간 갱신을 처리한다. */
UCLASS(Abstract)
class LABPROJECT_API ULocalizedMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Public API ------------------------------------------------------------------------------------------------------
	bool CloseGameSettings();

	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category="UI|Localization")
	void RefreshLocalizedText();

	UFUNCTION(BlueprintCallable, Category="UI|Settings")
	void OpenGameSettings();

protected:
	virtual void OnMenuLanguageChanged() {}

	// Internal Helpers ------------------------------------------------------------------------------------------------
	UMenuLocalizationSubsystem* GetLocalization() const;
	FText MenuText(FName Key) const;
	FText MenuTextOrFallback(FName Key, const FText& Fallback) const;

private:
	void ApplyLocalizedBindings();

protected:
	UPROPERTY(EditDefaultsOnly, Category="UI|Localization")
	TMap<FName, FName> MenuTextBindings;

	UPROPERTY(EditDefaultsOnly, Category="UI|Localization")
	TMap<FName, FName> MenuTooltipBindings;

	UPROPERTY(EditDefaultsOnly, Category="UI|Localization")
	TMap<FName, FName> MenuHintBindings;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="UI|Settings")
	TObjectPtr<UButton> Btn_GameSettings;

private:
	UPROPERTY(Transient) TObjectPtr<UGameSettingsWidget> ActiveGameSettings;
};
