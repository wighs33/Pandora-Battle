#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"

class UButton;

/**
 * 필터 버튼의 선택 표시를 동기화한다.
 * 선택 배경과 버튼 테두리는 강조 색상의 색조·채도를 사용하며,
 * 기존 HSV 명도와 알파는 유지한다.
 */
class FFilterButtonHighlightState
{

public:
	// Public API ------------------------------------------------------------------------------------------------------
	void Initialize(
		const TArray<TObjectPtr<UButton>>& InButtons,
		UButton* InitialSelectedButton,
		const FLinearColor& AccentColor);

	void Select(UButton* SelectedButton, const FLinearColor& AccentColor);
	void Reset();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	static void ApplyAccentToButtonStyle(FButtonStyle& ButtonStyle, const FLinearColor& AccentColor);
	static void ApplyAccentToBrushOutline(FSlateBrush& Brush, const FLinearColor& AccentColor);
	static FLinearColor MakeAccentColorPreservingValue(
		const FLinearColor& SourceColor,
		const FLinearColor& AccentColor);

private:
	TArray<TWeakObjectPtr<UButton>> Buttons;
	TArray<FLinearColor> DefaultButtonColors;
	TArray<FButtonStyle> DefaultButtonStyles;
};
