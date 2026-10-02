#include "UI/HUD/Notification/NotificationEntryWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NotificationEntryWidget)

void UNotificationEntryWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		if (HasNotificationContent(PreviewNotificationData))
		{
			ApplyNotificationData(PreviewNotificationData, false);
		}
		return;
	}

	if (bHasRuntimeNotificationData)
	{
		ApplyNotificationData(CachedNotificationData, false);
	}
}

void UNotificationEntryWidget::SetNotificationData(const FPdNotificationData& InNotificationData)
{
	CachedNotificationData = InNotificationData;
	bHasRuntimeNotificationData = true;

	ApplyNotificationData(InNotificationData, true);
}

void UNotificationEntryWidget::ApplyNotificationData(const FPdNotificationData& InNotificationData, bool bNotifyBlueprint)
{
	if (IconImage)
	{
		if (UTexture2D* IconTexture = Cast<UTexture2D>(InNotificationData.IconResource))
		{
			IconImage->SetBrushFromTexture(IconTexture, false);
		}
		else
		{
			IconImage->SetBrushResourceObject(InNotificationData.IconResource);
		}
		IconImage->SetVisibility(InNotificationData.IconResource ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (NotificationText)
	{
		NotificationText->SetText(InNotificationData.Text);
	}

	if (bNotifyBlueprint)
	{
		BP_OnNotificationDataSet(InNotificationData);
	}
}

void UNotificationEntryWidget::PlayNotificationIn()
{
	if (FadeIn)
	{
		PlayAnimation(FadeIn, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
	}
}

float UNotificationEntryWidget::PlayNotificationOut()
{
	if (!FadeIn)
	{
		return 0.0f;
	}

	PlayAnimation(FadeIn, 0.0f, 1, EUMGSequencePlayMode::Reverse, 1.0f, false);
	return FadeIn->GetEndTime();
}

bool UNotificationEntryWidget::HasNotificationContent(const FPdNotificationData& InNotificationData)
{
	return !InNotificationData.Text.IsEmpty() || InNotificationData.IconResource != nullptr;
}
