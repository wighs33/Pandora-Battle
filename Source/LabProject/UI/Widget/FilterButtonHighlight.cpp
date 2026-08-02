#include "UI/Widget/FilterButtonHighlight.h"

#include "Components/Button.h"

void FFilterButtonHighlightState::Initialize(
	const TArray<TObjectPtr<UButton>>& InButtons,
	UButton* InitialSelectedButton,
	const FLinearColor& AccentColor)
{
	Reset();

	Buttons.Reserve(InButtons.Num());
	DefaultButtonColors.Reserve(InButtons.Num());
	DefaultButtonStyles.Reserve(InButtons.Num());

	for (UButton* Button : InButtons)
	{
		Buttons.Add(Button);
		DefaultButtonColors.Add(Button ? Button->GetBackgroundColor() : FLinearColor::White);
		DefaultButtonStyles.Add(Button ? Button->GetStyle() : FButtonStyle());
	}

	Select(InitialSelectedButton, AccentColor);
}

void FFilterButtonHighlightState::Select(UButton* SelectedButton, const FLinearColor& AccentColor)
{
	for (int32 ButtonIndex = 0; ButtonIndex < Buttons.Num(); ++ButtonIndex)
	{
		UButton* Button = Buttons[ButtonIndex].Get();
		if (!Button || !DefaultButtonColors.IsValidIndex(ButtonIndex))
		{
			continue;
		}

		const bool bIsSelected = Button == SelectedButton;
		const FLinearColor& DefaultButtonColor = DefaultButtonColors[ButtonIndex];
		Button->SetBackgroundColor(
			bIsSelected
				? MakeAccentColorPreservingValue(DefaultButtonColor, AccentColor)
				: DefaultButtonColor);

		if (!DefaultButtonStyles.IsValidIndex(ButtonIndex))
		{
			continue;
		}

		FButtonStyle ButtonStyle = DefaultButtonStyles[ButtonIndex];
		if (bIsSelected)
		{
			ApplyAccentToButtonStyle(ButtonStyle, AccentColor);
		}
		Button->SetStyle(ButtonStyle);
	}
}

void FFilterButtonHighlightState::Reset()
{
	for (int32 ButtonIndex = 0; ButtonIndex < Buttons.Num(); ++ButtonIndex)
	{
		if (UButton* Button = Buttons[ButtonIndex].Get();
			Button && DefaultButtonColors.IsValidIndex(ButtonIndex))
		{
			Button->SetBackgroundColor(DefaultButtonColors[ButtonIndex]);
			if (DefaultButtonStyles.IsValidIndex(ButtonIndex))
			{
				Button->SetStyle(DefaultButtonStyles[ButtonIndex]);
			}
		}
	}

	Buttons.Reset();
	DefaultButtonColors.Reset();
	DefaultButtonStyles.Reset();
}

void FFilterButtonHighlightState::ApplyAccentToButtonStyle(
	FButtonStyle& ButtonStyle,
	const FLinearColor& AccentColor)
{
	ApplyAccentToBrushOutline(ButtonStyle.Normal, AccentColor);
	ApplyAccentToBrushOutline(ButtonStyle.Hovered, AccentColor);
	ApplyAccentToBrushOutline(ButtonStyle.Pressed, AccentColor);
	ApplyAccentToBrushOutline(ButtonStyle.Disabled, AccentColor);
}

void FFilterButtonHighlightState::ApplyAccentToBrushOutline(
	FSlateBrush& Brush,
	const FLinearColor& AccentColor)
{
	const FLinearColor DefaultOutlineColor = Brush.OutlineSettings.Color.GetSpecifiedColor();
	Brush.OutlineSettings.Color = FSlateColor(
		MakeAccentColorPreservingValue(DefaultOutlineColor, AccentColor));
}

FLinearColor FFilterButtonHighlightState::MakeAccentColorPreservingValue(
	const FLinearColor& SourceColor,
	const FLinearColor& AccentColor)
{
	FLinearColor SourceHsv = SourceColor.LinearRGBToHSV();
	const FLinearColor AccentHsv = AccentColor.LinearRGBToHSV();

	SourceHsv.R = AccentHsv.R;
	SourceHsv.G = AccentHsv.G;

	FLinearColor Result = SourceHsv.HSVToLinearRGB();
	Result.A = SourceColor.A;
	return Result;
}
