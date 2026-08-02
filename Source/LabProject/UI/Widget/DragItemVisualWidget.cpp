#include "UI/Widget/DragItemVisualWidget.h"

#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/WidgetLookup.h"

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

void UDragItemVisualWidget::SetQuantity(const int32 InQuantity)
{
	Quantity = FMath::Max(0, InQuantity);
	ApplyVisual();
}

void UDragItemVisualWidget::CacheOptionalWidgets()
{
	if (!RootSizeBox)
	{
		RootSizeBox = PdWidgetLookup::FindWidgetByNames<USizeBox>(this, {
			TEXT("RootSizeBox"),
			TEXT("DragSizeBox"),
			TEXT("SizeBox")
		});
	}

	if (!IconImage)
	{
		IconImage = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("IconImage"),
			TEXT("DragIconImage"),
			TEXT("ItemIconImage"),
			TEXT("Icon")
		});
	}

	if (!QuantityTextBlock)
	{
		QuantityTextBlock = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("QuantityTextBlock"),
			TEXT("Txt_Quantity"),
			TEXT("Text_Quantity"),
			TEXT("QuantityText"),
			TEXT("ItemCountText")
		});
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

	if (QuantityTextBlock)
	{
		const bool bShowQuantity = Quantity > 0 && IconTexture != nullptr;
		QuantityTextBlock->SetText(FText::AsNumber(Quantity));
		QuantityTextBlock->SetVisibility(bShowQuantity ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}
