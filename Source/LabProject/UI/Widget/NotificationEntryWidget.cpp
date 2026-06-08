#include "UI/Widget/NotificationEntryWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NotificationEntryWidget)

DEFINE_LOG_CATEGORY_STATIC(LogNotificationEntryWidget, Log, All);

void UNotificationEntryWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	CacheOptionalWidgets();
	UE_LOG(LogNotificationEntryWidget, Verbose,
		TEXT("[NotificationEntry] PreConstruct. widget=%s designTime=%s iconImage=%s textBlock=%s cachedText=%s cachedIcon=%s previewText=%s previewIcon=%s hasRuntimeData=%s"),
		*GetNameSafe(this),
		IsDesignTime() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(IconImage),
		*GetNameSafe(NotificationText),
		*CachedNotificationData.Text.ToString(),
		*GetNameSafe(CachedNotificationData.IconResource),
		*PreviewNotificationData.Text.ToString(),
		*GetNameSafe(PreviewNotificationData.IconResource),
		bHasRuntimeNotificationData ? TEXT("true") : TEXT("false"));

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

	UE_LOG(LogNotificationEntryWidget, Verbose,
		TEXT("[NotificationEntry] SetNotificationData. widget=%s iconImage=%s textBlock=%s text=%s icon=%s"),
		*GetNameSafe(this),
		*GetNameSafe(IconImage),
		*GetNameSafe(NotificationText),
		*InNotificationData.Text.ToString(),
		*GetNameSafe(InNotificationData.IconResource));

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
	else if (InNotificationData.IconResource)
	{
		UE_LOG(LogNotificationEntryWidget, Warning,
			TEXT("[NotificationEntry] icon skipped: IconImage widget not found. widget=%s icon=%s expected one of: IconImage, NotificationIcon, ItemIcon"),
			*GetNameSafe(this),
			*GetNameSafe(InNotificationData.IconResource));
	}

	if (NotificationText)
	{
		NotificationText->SetText(InNotificationData.Text);
	}
	else if (!InNotificationData.Text.IsEmpty())
	{
		UE_LOG(LogNotificationEntryWidget, Warning,
			TEXT("[NotificationEntry] text skipped: NotificationText widget not found. widget=%s text=%s expected one of: NotificationText, Text, TextBlock"),
			*GetNameSafe(this),
			*InNotificationData.Text.ToString());
	}

	if (bNotifyBlueprint)
	{
		BP_OnNotificationDataSet(InNotificationData);
	}
}

void UNotificationEntryWidget::PlayNotificationIn()
{
	UE_LOG(LogNotificationEntryWidget, Verbose,
		TEXT("[NotificationEntry] PlayNotificationIn. widget=%s fadeIn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(FadeIn));
	if (FadeIn)
	{
		PlayAnimation(FadeIn, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
	}
}

float UNotificationEntryWidget::PlayNotificationOut()
{
	UE_LOG(LogNotificationEntryWidget, Verbose,
		TEXT("[NotificationEntry] PlayNotificationOut. widget=%s fadeIn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(FadeIn));
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
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("IconImage")));
	}
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("NotificationIcon")));
	}
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("ItemIcon")));
	}

	if (!NotificationText)
	{
		NotificationText = Cast<UTextBlock>(GetWidgetFromName(TEXT("NotificationText")));
	}
	if (!NotificationText)
	{
		NotificationText = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text")));
	}
	if (!NotificationText)
	{
		NotificationText = Cast<UTextBlock>(GetWidgetFromName(TEXT("TextBlock")));
	}
}

bool UNotificationEntryWidget::HasNotificationContent(const FPdNotificationData& InNotificationData)
{
	return !InNotificationData.Text.IsEmpty() || InNotificationData.IconResource != nullptr;
}
