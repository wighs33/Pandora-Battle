#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"

class UButton;

/**
 * 필터 버튼의 선택 표시를 동기화한다.
 * 고른 버튼은 평소·마우스 올림·누름 바탕을 고른 버튼 바탕으로 바꾸고, 나머지는 처음 스타일로 되돌린다.
 */
class FFilterButtonHighlightState
{
public:
	// Public API ------------------------------------------------------------------------------------------------------
	/** 고른 버튼 바탕에 그림이 없으면 고른 버튼을 따로 그리지 않는다. */
	void Initialize(
		const TArray<TObjectPtr<UButton>>& InButtons,
		UButton* InitialSelectedButton,
		const FSlateBrush& InSelectedBrush);

	void Select(UButton* SelectedButton);
	void Reset();

private:
	TArray<TWeakObjectPtr<UButton>> Buttons;
	TArray<FButtonStyle> DefaultButtonStyles;
	FSlateBrush SelectedBrush;
};
