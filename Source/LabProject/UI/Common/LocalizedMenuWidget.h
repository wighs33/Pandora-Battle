#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LocalizedMenuWidget.generated.h"

class UMenuLocalizationExtension;
class UMenuLocalizationSubsystem;

/**
 * 메뉴 문구를 쓰는 패널(항목·슬롯·HUD 조각)의 기반. 위젯 애셋이 정한 연결 표를 다국어 부품에 넘길 뿐, 다국어 처리는 부품이 한다.
 * 화면 층에 올리는 화면은 UScreenWidget을 쓰고, 같은 부품을 붙인다.
 */
UCLASS(Abstract)
class LABPROJECT_API ULocalizedMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeOnInitialized() override;

protected:
	/** 언어가 바뀌어 연결한 문구를 다시 채운 뒤 부른다. 코드에서 조합한 문구는 여기서 다시 만든다. */
	virtual void OnMenuLanguageChanged() {}

	// Internal Helpers ------------------------------------------------------------------------------------------------
	UMenuLocalizationSubsystem* GetLocalization() const;
	FText MenuText(FName Key) const;
	FText MenuTextOrFallback(FName Key, const FText& Fallback) const;
	void SetHoverTooltip(UWidget* Widget, const FText& Text);

protected:
	UPROPERTY(EditDefaultsOnly, Category="!UI|Localization")
	TMap<FName, FName> MenuTextBindings;

	UPROPERTY(EditDefaultsOnly, Category="!UI|Localization")
	TMap<FName, FName> MenuTooltipBindings;

	UPROPERTY(EditDefaultsOnly, Category="!UI|Localization")
	TMap<FName, FName> MenuHintBindings;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMenuLocalizationExtension> MenuLocalization;
};
