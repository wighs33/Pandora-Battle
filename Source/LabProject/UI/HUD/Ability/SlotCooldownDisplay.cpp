#include "UI/HUD/Ability/SlotCooldownDisplay.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

float PdSlotCooldownDisplay::CalculatePercent(const float TimeRemaining, const double CooldownDuration)
{
	if (CooldownDuration <= UE_SMALL_NUMBER)
	{
		return 1.0f;
	}

	return FMath::Clamp(1.0f - static_cast<float>(TimeRemaining / CooldownDuration), 0.0f, 1.0f);
}

void PdSlotCooldownDisplay::ShowReady(UWidget* TimerContainer, UProgressBar* Progress, UTextBlock* TimerText)
{
	if (TimerContainer)
	{
		TimerContainer->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (Progress)
	{
		Progress->SetPercent(1.0f);
	}

	if (TimerText)
	{
		TimerText->SetText(FText::GetEmpty());
	}
}

void PdSlotCooldownDisplay::ShowRemaining(UProgressBar* Progress, UTextBlock* TimerText, const float TimeRemaining,
	const double CooldownDuration, const bool bShowTimeRemaining)
{
	if (Progress)
	{
		Progress->SetPercent(CalculatePercent(TimeRemaining, CooldownDuration));
	}

	if (TimerText)
	{
		TimerText->SetText(bShowTimeRemaining ? FText::AsNumber(FMath::CeilToInt(TimeRemaining)) : FText::GetEmpty());
	}
}
