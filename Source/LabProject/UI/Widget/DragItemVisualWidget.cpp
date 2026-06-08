#include "UI/Widget/DragItemVisualWidget.h"

#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DragItemVisualWidget)

void UDragItemVisualWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplyVisual();
}

void UDragItemVisualWidget::SetIconTexture(UTexture2D* InIconTexture)
{
	IconTexture = InIconTexture;
	ApplyVisual();
}

void UDragItemVisualWidget::SetIconSize(const FVector2D InIconSize)
{
	IconSize = InIconSize;
	ApplyVisual();
}

void UDragItemVisualWidget::CacheOptionalWidgets()
{
	if (!RootSizeBox)
	{
		RootSizeBox = Cast<USizeBox>(GetWidgetFromName(TEXT("RootSizeBox")));
	}
	if (!RootSizeBox)
	{
		RootSizeBox = Cast<USizeBox>(GetWidgetFromName(TEXT("DragSizeBox")));
	}
	if (!RootSizeBox)
	{
		RootSizeBox = Cast<USizeBox>(GetWidgetFromName(TEXT("SizeBox")));
	}

	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("IconImage")));
	}
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("DragIconImage")));
	}
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("ItemIconImage")));
	}
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("Icon")));
	}
}

void UDragItemVisualWidget::ApplyVisual()
{
	CacheOptionalWidgets();

	if (RootSizeBox && IconSize.X > 0.0f && IconSize.Y > 0.0f)
	{
		RootSizeBox->SetWidthOverride(IconSize.X);
		RootSizeBox->SetHeightOverride(IconSize.Y);
	}

	if (IconImage)
	{
		IconImage->SetBrushFromTexture(IconTexture, false);
		if (IconSize.X > 0.0f && IconSize.Y > 0.0f)
		{
			IconImage->SetDesiredSizeOverride(IconSize);
		}
		IconImage->SetVisibility(IconTexture ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}
