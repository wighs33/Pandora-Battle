#pragma once

#include "CoreMinimal.h"
#include "Extensions/UserWidgetExtension.h"
#include "MenuLocalizationExtension.generated.h"

class UMenuHoverTooltip;
class UMenuLocalizationSubsystem;
class UWidget;

/** 위젯 이름을 문자열 테이블 MenuText의 키에 잇는 표. 위젯 애셋이 클래스 기본값으로 정한다. */
struct FMenuTextBindings
{
	TMap<FName, FName> Texts;
	TMap<FName, FName> Tooltips;
	TMap<FName, FName> Hints;
};

/**
 * 메뉴 위젯에 붙여 쓰는 다국어 부품. 패널이든 화면이든 기반 클래스와 관계없이 붙일 수 있다.
 * 연결한 글상자·설명·입력 힌트를 메뉴 문구로 채우고 언어별 글꼴을 입히며, 언어가 바뀌면 다시 채운 뒤 주인에게 알린다.
 * 마우스를 올린 위젯의 설명은 모든 메뉴 화면보다 위층의 설명 패널(UMenuHoverTooltip)에 보여 준다.
 */
UCLASS()
class LABPROJECT_API UMenuLocalizationExtension : public UUserWidgetExtension
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Construct() override;
	virtual void Destruct() override;

	// Public API ------------------------------------------------------------------------------------------------------
	/** 위젯에 부품을 붙인다. 주인이 NativeOnInitialized에서 부르고, OnLanguageChanged는 문구를 다시 채운 뒤 부른다. */
	static UMenuLocalizationExtension* Attach(UUserWidget& Widget, FMenuTextBindings InBindings, FSimpleDelegate InOnLanguageChanged);

	/**
	 * Widget에 마우스를 올리면 설명 패널에 Text를 보여 준다. 엔진 툴팁은 끈다. 비활성 위젯에도 보인다.
	 * 연결 표의 설명과 에셋에 툴팁을 직접 적은 위젯은 부품이 부른다. 코드에서 바꾸는 설명은 직접 부른다.
	 */
	void SetHoverTooltip(UWidget* Widget, const FText& Text);

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandleLanguageChanged();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	UMenuLocalizationSubsystem* GetLocalization() const;
	void ApplyBindings();
	void UpdateHoverTooltip();
	UWidget* FindHoveredTooltipWidget() const;

private:
	FMenuTextBindings Bindings;
	FSimpleDelegate OnLanguageChanged;

	/** 설명 패널은 처음 쓸 때 모든 메뉴 화면보다 위층에 하나 만들고, 주인 위젯이 닫히면 함께 치운다. */
	UPROPERTY(Transient)
	TObjectPtr<UMenuHoverTooltip> HoverTooltip;

	TMap<TWeakObjectPtr<UWidget>, FText> HoverTooltipTexts;
	FDelegateHandle HoverTooltipUpdateHandle;
};
