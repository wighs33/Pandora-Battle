#include "UI/Widget/GoldenKillAnnouncementWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GoldenKillAnnouncementWidget)

UGoldenKillAnnouncementWidget::UGoldenKillAnnouncementWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UGoldenKillAnnouncementWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyTextStyle(GoldenKillText);
	HideGoldenKillAnnouncement();
}

void UGoldenKillAnnouncementWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}

	Super::NativeDestruct();
}

void UGoldenKillAnnouncementWidget::PlayGoldenKillAnnouncement(const FText& OverrideText)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}

	const FText TextToShow = OverrideText.IsEmpty() ? GoldenKillText : OverrideText;
	ApplyTextStyle(TextToShow);

	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(1.0f);
	SetRenderTransformAngle(0.0f);

	float HideDelay = FallbackVisibleDuration;
	if (GoldenKillAnimation)
	{
		StopAnimation(GoldenKillAnimation);
		PlayAnimation(GoldenKillAnimation, 0.0f, 1, EUMGSequencePlayMode::Forward, AnimationPlaybackSpeed);
		HideDelay = FMath::Max(GoldenKillAnimation->GetEndTime() / FMath::Max(AnimationPlaybackSpeed, 0.01f), 0.01f);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			HideTimerHandle,
			this,
			&ThisClass::HideGoldenKillAnnouncement,
			HideDelay,
			false);
	}
}

void UGoldenKillAnnouncementWidget::HideGoldenKillAnnouncement()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}

	SetRenderOpacity(0.0f);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UGoldenKillAnnouncementWidget::ApplyTextStyle(const FText& TextToShow)
{
	if (!Txt_GoldenKill)
	{
		return;
	}

	Txt_GoldenKill->SetText(TextToShow);
	Txt_GoldenKill->SetColorAndOpacity(FSlateColor(GoldenKillTextColor));
	Txt_GoldenKill->SetVisibility(ESlateVisibility::HitTestInvisible);
}
