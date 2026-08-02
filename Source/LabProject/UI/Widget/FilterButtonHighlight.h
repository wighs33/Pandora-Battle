#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"

class UButton;

/**
 * Keeps filter button selection visuals in sync.
 * The selected background and button-style outline borrow hue/saturation from the accent
 * color while preserving their original HSV value and alpha.
 */
class FFilterButtonHighlightState
{
public:
	void Initialize(
		const TArray<TObjectPtr<UButton>>& InButtons,
		UButton* InitialSelectedButton,
		const FLinearColor& AccentColor);

	void Select(UButton* SelectedButton, const FLinearColor& AccentColor);
	void Reset();

private:
	static void ApplyAccentToButtonStyle(FButtonStyle& ButtonStyle, const FLinearColor& AccentColor);
	static void ApplyAccentToBrushOutline(FSlateBrush& Brush, const FLinearColor& AccentColor);
	static FLinearColor MakeAccentColorPreservingValue(
		const FLinearColor& SourceColor,
		const FLinearColor& AccentColor);

	TArray<TWeakObjectPtr<UButton>> Buttons;
	TArray<FLinearColor> DefaultButtonColors;
	TArray<FButtonStyle> DefaultButtonStyles;
};
