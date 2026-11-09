#include "UI/Info/FilterButtonHighlight.h"

#include "Components/Button.h"

void FFilterButtonHighlightState::Initialize(const TArray<TObjectPtr<UButton>>& InButtons,
	UButton* InitialSelectedButton, const FSlateBrush& InSelectedBrush)
{
	Reset();

	SelectedBrush = InSelectedBrush;
	Buttons.Reserve(InButtons.Num());
	DefaultButtonStyles.Reserve(InButtons.Num());

	for (UButton* Button : InButtons)
	{
		Buttons.Add(Button);
		DefaultButtonStyles.Add(Button ? Button->GetStyle() : FButtonStyle());
	}

	Select(InitialSelectedButton);
}

void FFilterButtonHighlightState::Select(UButton* SelectedButton)
{
	const bool bHasSelectedBrush = SelectedBrush.GetResourceObject() != nullptr;
	for (int32 ButtonIndex = 0; ButtonIndex < Buttons.Num(); ++ButtonIndex)
	{
		UButton* Button = Buttons[ButtonIndex].Get();
		if (!Button || !DefaultButtonStyles.IsValidIndex(ButtonIndex))
		{
			continue;
		}

		FButtonStyle ButtonStyle = DefaultButtonStyles[ButtonIndex];
		if (bHasSelectedBrush && Button == SelectedButton)
		{
			ButtonStyle.SetNormal(SelectedBrush);
			ButtonStyle.SetHovered(SelectedBrush);
			ButtonStyle.SetPressed(SelectedBrush);
		}
		Button->SetStyle(ButtonStyle);
	}
}

void FFilterButtonHighlightState::Reset()
{
	for (int32 ButtonIndex = 0; ButtonIndex < Buttons.Num(); ++ButtonIndex)
	{
		if (UButton* Button = Buttons[ButtonIndex].Get(); Button && DefaultButtonStyles.IsValidIndex(ButtonIndex))
		{
			Button->SetStyle(DefaultButtonStyles[ButtonIndex]);
		}
	}

	Buttons.Reset();
	DefaultButtonStyles.Reset();
}
