#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LocalizedMenuWidget.generated.h"

class UButton;
class UMenuHoverTooltip;
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
	UFUNCTION()
	void RefreshLocalizedText();

	UFUNCTION()
	void OpenGameSettings();

protected:
	virtual void OnMenuLanguageChanged() {}

	// Internal Helpers ------------------------------------------------------------------------------------------------
	UMenuLocalizationSubsystem* GetLocalization() const;
	FText MenuText(FName Key) const;
	FText MenuTextOrFallback(FName Key, const FText& Fallback) const;

	/**
	 * Widget에 마우스를 올리면 바깥쪽 설명 패널(UMenuHoverTooltip)에 Text를 보여 준다. 엔진 툴팁은 끈다. 비활성 위젯에도 보인다.
	 * MenuTooltipBindings에 연결한 위젯과 에셋에 툴팁을 직접 적은 위젯은 기반 클래스가 부른다. 코드에서 바꾸는 설명은 직접 부른다.
	 */
	void SetHoverTooltip(UWidget* Widget, const FText& Text);

private:
	void ApplyLocalizedBindings();
	void UpdateHoverTooltip();
	UWidget* FindHoveredTooltipWidget() const;

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
	/** 설명 패널은 처음 쓸 때 모든 메뉴 화면보다 위층에 하나 만들고, 이 화면이 닫히면 함께 치운다. */
	UPROPERTY(Transient)
	TObjectPtr<UMenuHoverTooltip> HoverTooltip;

	TMap<TWeakObjectPtr<UWidget>, FText> HoverTooltipTexts;

	FDelegateHandle HoverTooltipUpdateHandle;
};
