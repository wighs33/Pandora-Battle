#include "UI/HUD/Ability/SlotReadyPulse.h"

#include "Components/Widget.h"

namespace
{
	// 고리는 칸 크기에서 시작해 이만큼 커지며 사라진다.
	constexpr float RingDurationSeconds = 0.45f;
	constexpr float RingEndScale = 1.35f;
	constexpr float RingStartOpacity = 0.9f;

	// 그림은 처음 속도만 받고 스프링으로 1배에 돌아온다. 감쇠비 약 0.45라 한 번 넘쳤다 자리 잡는다.
	constexpr float IconKickVelocity = 3.2f;
	const UiMotion::FSpringSettings IconSpring{420.f, 18.f};
}

void FSlotReadyPulse::Play(UWidget* Ring, UWidget* Icon)
{
	RingProgress.Start();
	IconScale.Value = 1.f;
	IconScale.Velocity = IconKickVelocity;
	bIconSettling = true;
	if (Ring)
	{
		Ring->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	Tick(0.f, Ring, Icon);
}

void FSlotReadyPulse::Tick(const float DeltaSeconds, UWidget* Ring, UWidget* Icon)
{
	if (RingProgress.IsPlaying())
	{
		const float Alpha = RingProgress.Advance(DeltaSeconds, RingDurationSeconds);
		if (Ring)
		{
			const float Spread = UiMotion::EaseOutExpo(Alpha);
			Ring->SetRenderScale(FVector2D(FMath::Lerp(1.f, RingEndScale, Spread)));
			Ring->SetRenderOpacity(RingStartOpacity * (1.f - UiMotion::EaseOutCubic(Alpha)));
			if (Alpha >= 1.f)
			{
				Ring->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}

	if (bIconSettling)
	{
		IconScale.Step(1.f, DeltaSeconds, IconSpring);
		if (IconScale.IsSettled(1.f))
		{
			IconScale.Snap(1.f);
			bIconSettling = false;
		}
		if (Icon)
		{
			Icon->SetRenderScale(FVector2D(IconScale.Value));
		}
	}
}
