#include "UI/HUD/Ability/SlotCooldownDisplay.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

float SlotCooldownDisplay::CalculateRemainingFraction(const float TimeRemaining, const double CooldownDuration)
{
	if (CooldownDuration <= UE_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return FMath::Clamp(static_cast<float>(TimeRemaining / CooldownDuration), 0.0f, 1.0f);
}

void SlotCooldownDisplay::ShowReady(UWidget* TimerContainer, UProgressBar* Shade, UTextBlock* TimerText)
{
	if (TimerContainer)
	{
		TimerContainer->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (Shade)
	{
		Shade->SetPercent(0.0f);
	}

	if (TimerText)
	{
		TimerText->SetText(FText::GetEmpty());
	}
}

void SlotCooldownDisplay::ShowRemaining(UProgressBar* Shade, UTextBlock* TimerText, const float TimeRemaining,
	const double CooldownDuration, const bool bShowTimeRemaining)
{
	if (Shade)
	{
		Shade->SetPercent(CalculateRemainingFraction(TimeRemaining, CooldownDuration));
	}

	if (TimerText)
	{
		TimerText->SetText(bShowTimeRemaining ? FText::AsNumber(FMath::CeilToInt(TimeRemaining)) : FText::GetEmpty());
	}
}
