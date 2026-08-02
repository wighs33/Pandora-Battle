#include "UI/Widget/RespawnDelayWidget.h"

#include "Components/TextBlock.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RespawnDelayWidget)

URespawnDelayWidget::URespawnDelayWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void URespawnDelayWidget::NativeConstruct()
{
	Super::NativeConstruct();
	HideRespawnDelay();
}

void URespawnDelayWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnDelayTickHandle);
	}

	Super::NativeDestruct();
}

void URespawnDelayWidget::StartRespawnDelay(const float InDelaySeconds)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnDelayTickHandle);
	}

	RemainingSeconds = FMath::Max(InDelaySeconds, 0.0f);
	bRespawnDelayActive = RemainingSeconds > 0.0f;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(1.0f);
	RefreshUI();
	BP_OnRespawnDelayStarted(RemainingSeconds);

	if (!bRespawnDelayActive)
	{
		FinishRespawnDelay();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		LastUpdateTimeSeconds = World->GetTimeSeconds();
		World->GetTimerManager().SetTimer(
			RespawnDelayTickHandle,
			this,
			&ThisClass::HandleRespawnDelayTick,
			FMath::Max(TickInterval, 0.01f),
			true);
	}
}

void URespawnDelayWidget::HideRespawnDelay()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnDelayTickHandle);
	}

	bRespawnDelayActive = false;
	RemainingSeconds = 0.0f;
	SetRenderOpacity(0.0f);
	SetVisibility(ESlateVisibility::Collapsed);
	RefreshUI();
}

void URespawnDelayWidget::RefreshUI()
{
	if (!Txt_RespawnDelay)
	{
		return;
	}

	Txt_RespawnDelay->SetText(FormatRespawnDelayText());
	Txt_RespawnDelay->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void URespawnDelayWidget::HandleRespawnDelayTick()
{
	UWorld* World = GetWorld();
	if (!World || !bRespawnDelayActive)
	{
		return;
	}

	const float NowSeconds = World->GetTimeSeconds();
	const float DeltaSeconds = FMath::Max(NowSeconds - LastUpdateTimeSeconds, 0.0f);
	LastUpdateTimeSeconds = NowSeconds;

	RemainingSeconds = FMath::Max(RemainingSeconds - DeltaSeconds, 0.0f);
	RefreshUI();

	if (RemainingSeconds <= 0.0f)
	{
		FinishRespawnDelay();
	}
}

void URespawnDelayWidget::FinishRespawnDelay()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnDelayTickHandle);
	}

	bRespawnDelayActive = false;
	RemainingSeconds = 0.0f;
	RefreshUI();
	BP_OnRespawnDelayFinished();

	if (bHideWhenFinished)
	{
		HideRespawnDelay();
	}
}

FText URespawnDelayWidget::FormatRespawnDelayText() const
{
	if (!bRespawnDelayActive && RemainingSeconds <= 0.0f)
	{
		return FinishedText;
	}

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Seconds"), FText::AsNumber(FMath::CeilToInt(FMath::Max(RemainingSeconds, 0.0f))));
	return FText::Format(RespawnDelayFormatText, Arguments);
}
