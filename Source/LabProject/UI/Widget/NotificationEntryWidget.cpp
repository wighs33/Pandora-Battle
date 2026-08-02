#include "UI/Widget/NotificationEntryWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/WidgetLookup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NotificationEntryWidget)

void UNotificationEntryWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	CacheOptionalWidgets();


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
	CacheOptionalWidgets();



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

void UNotificationEntryWidget::CacheOptionalWidgets()
{
	if (!IconImage)
	{
		IconImage = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("IconImage"),
			TEXT("NotificationIcon"),
			TEXT("ItemIcon")
		});
	}

	if (!NotificationText)
	{
		NotificationText = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("NotificationText"),
			TEXT("Text"),
			TEXT("TextBlock")
		});
	}
}

bool UNotificationEntryWidget::HasNotificationContent(const FPdNotificationData& InNotificationData)
{
	return !InNotificationData.Text.IsEmpty() || InNotificationData.IconResource != nullptr;
}
