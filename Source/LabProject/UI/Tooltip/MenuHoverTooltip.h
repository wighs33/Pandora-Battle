#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MenuHoverTooltip.generated.h"

class UFont;
class UTextBlock;

/**
 * 메뉴 위젯에 마우스를 올리면 그 위젯 바깥쪽에 붙어 설명을 보여 주는 패널.
 * 엔진 툴팁은 마우스 위치에 떠서 큰 소프트웨어 커서가 글자를 가리므로, 커서가 아니라 가리킨 위젯을 기준으로 놓는다.
 */
UCLASS()
class LABPROJECT_API UMenuHoverTooltip : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Target이 화면 왼쪽 절반에 있으면 오른쪽에, 아니면 왼쪽에 붙여 Text를 보여 준다. 같은 설명을 이미 보이고 있으면 그대로 둔다. */
	void ShowFor(const UWidget& Target, const FText& Text, UFont* Font);

	void Hide();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DescriptionText;

	TWeakObjectPtr<const UWidget> CurrentTarget;
};
