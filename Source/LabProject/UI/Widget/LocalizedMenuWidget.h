#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LocalizedMenuWidget.generated.h"

class UButton;
class UGameSettingsWidget;
class UMenuLocalizationSubsystem;

/** Screen assets bind widget names to text keys; the base handles lifetime and live updates. */
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
