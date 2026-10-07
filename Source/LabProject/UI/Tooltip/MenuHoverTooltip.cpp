#include "UI/Tooltip/MenuHoverTooltip.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MenuHoverTooltip)

namespace
{
	/** 가리킨 위젯과 패널 사이 간격(뷰포트 단위) */
	constexpr float TargetGap = 12.f;
}

// 타이틀의 Luna 말풍선과 같은 색으로 만든다: 크림색 바탕, 갈색 테두리, 짙은 갈색 글자.
TSharedRef<SWidget> UMenuHoverTooltip::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor(0.955f, 0.896f, 0.776f));
		Brush.OutlineSettings.Color = FSlateColor(FLinearColor(0.701f, 0.474f, 0.195f));
		Brush.OutlineSettings.Width = 2.f;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(10.f, 10.f, 10.f, 10.f);

		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuHoverTooltipPanel"));
		Panel->SetBrush(Brush);
		Panel->SetPadding(FMargin(16.f, 10.f));

		DescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_MenuHoverTooltip"));
		FSlateFontInfo Font = DescriptionText->GetFont();
		Font.Size = 16;
		DescriptionText->SetFont(Font);
		DescriptionText->SetColorAndOpacity(FSlateColor(FLinearColor(0.130f, 0.056f, 0.021f)));
		DescriptionText->SetWrapTextAt(360.f);
		Panel->SetContent(DescriptionText);
		WidgetTree->RootWidget = Panel;
	}
	return Super::RebuildWidget();
}

void UMenuHoverTooltip::ShowFor(const UWidget& Target, const FText& Text, UFont* Font)
{
	if (!DescriptionText || (CurrentTarget.Get() == &Target && DescriptionText->GetText().EqualTo(Text)))
	{
		return;
	}
	CurrentTarget = &Target;
	DescriptionText->SetText(Text);
	if (Font)
	{
		FSlateFontInfo FontInfo = DescriptionText->GetFont();
		FontInfo.FontObject = Font;
		FontInfo.TypefaceFontName = NAME_None;
		DescriptionText->SetFont(FontInfo);
	}

	// 가리킨 위젯의 왼쪽·오른쪽 가운데를 뷰포트 좌표로 바꾸고, 화면 안쪽을 향한 면에 붙여 패널이 화면 밖으로 나가지 않게 한다.
	const FGeometry& Geometry = Target.GetCachedGeometry();
	const FVector2D AbsoluteTopLeft = Geometry.GetAbsolutePosition();
	const FVector2D AbsoluteSize = Geometry.GetAbsoluteSize();
	FVector2D LeftPixel, LeftViewport, RightPixel, RightViewport;
	USlateBlueprintLibrary::AbsoluteToViewport(this, AbsoluteTopLeft + FVector2D(0.0, AbsoluteSize.Y * 0.5), LeftPixel, LeftViewport);
	USlateBlueprintLibrary::AbsoluteToViewport(this, AbsoluteTopLeft + FVector2D(AbsoluteSize.X, AbsoluteSize.Y * 0.5), RightPixel, RightViewport);
	const bool bPlaceRight = (LeftPixel.X + RightPixel.X) * 0.5 < UWidgetLayoutLibrary::GetViewportSize(this).X * 0.5;
	SetAlignmentInViewport(FVector2D(bPlaceRight ? 0.0 : 1.0, 0.5));
	SetPositionInViewport(bPlaceRight ? RightViewport + FVector2D(TargetGap, 0.0) : LeftViewport - FVector2D(TargetGap, 0.0), false);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMenuHoverTooltip::Hide()
{
	CurrentTarget.Reset();
	SetVisibility(ESlateVisibility::Collapsed);
}
