#include "UI/Cursor/MouseCursorWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MouseCursorWidget)

void UMouseCursorWidget::ConfigureCursor(UTexture2D* InTexture, const FVector2D InSize, const FVector2D InHotSpot)
{
	CursorTexture = InTexture;
	CursorSize = FVector2D(FMath::Max(InSize.X, 1.0), FMath::Max(InSize.Y, 1.0));
	CursorHotSpot = FVector2D(
		FMath::Clamp(InHotSpot.X, 0.0, CursorSize.X),
		FMath::Clamp(InHotSpot.Y, 0.0, CursorSize.Y));

	ApplyCursorVisual();
}

TSharedRef<SWidget> UMouseCursorWidget::RebuildWidget()
{
	if (WidgetTree && !CursorRoot)
	{
		CursorRoot = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CursorRoot"));
		CursorCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CursorCanvas"));
		CursorImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CursorImage"));

		if (CursorRoot && CursorCanvas && CursorImage)
		{
			CursorRoot->SetContent(CursorCanvas);
			CursorCanvas->AddChildToCanvas(CursorImage);
			WidgetTree->RootWidget = CursorRoot;
		}
	}

	ApplyCursorVisual();
	return Super::RebuildWidget();
}

void UMouseCursorWidget::ApplyCursorVisual()
{
	if (!CursorImage)
	{
		return;
	}

	if (CursorRoot)
	{
		const FVector2D CursorRootSize = CursorSize * 2.0;
		CursorRoot->SetWidthOverride(CursorRootSize.X);
		CursorRoot->SetHeightOverride(CursorRootSize.Y);
		CursorRoot->SetMinDesiredWidth(CursorRootSize.X);
		CursorRoot->SetMinDesiredHeight(CursorRootSize.Y);
		CursorRoot->SetRenderTranslation(FVector2D::ZeroVector);
	}

	if (UCanvasPanelSlot* CursorImageSlot = Cast<UCanvasPanelSlot>(CursorImage->Slot))
	{
		CursorImageSlot->SetAutoSize(false);
		CursorImageSlot->SetPosition(CursorSize - CursorHotSpot);
		CursorImageSlot->SetSize(CursorSize);
		CursorImageSlot->SetAlignment(FVector2D::ZeroVector);
	}

	CursorImage->SetVisibility(CursorTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	CursorImage->SetDesiredSizeOverride(CursorSize);
	CursorImage->SetRenderTranslation(FVector2D::ZeroVector);

	if (CursorTexture)
	{
		FSlateBrush CursorBrush;
		CursorBrush.SetResourceObject(CursorTexture);
		CursorBrush.DrawAs = ESlateBrushDrawType::Image;
		CursorBrush.ImageSize = CursorSize;
		CursorImage->SetBrush(CursorBrush);
	}
}
