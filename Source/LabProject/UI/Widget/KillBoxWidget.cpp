#include "UI/Widget/KillBoxWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(KillBoxWidget)

namespace
{
	void ApplyBorderTint(UBorder* Border, const FLinearColor& TintColor)
	{
		if (!Border)
		{
			return;
		}

		FSlateBrush Brush = Border->Background;
		Brush.TintColor = FSlateColor(TintColor);
		Brush.OutlineSettings.Color = FSlateColor(TintColor);
		Border->SetBrush(Brush);
		Border->SetBrushColor(FLinearColor::White);
	}

	FLinearColor MakeLightTeamTextColor(const FLinearColor& TeamColor)
	{
		FLinearColor TextColor;
		TextColor.R = FMath::Lerp(TeamColor.R, 1.0f, 0.5f);
		TextColor.G = FMath::Lerp(TeamColor.G, 1.0f, 0.5f);
		TextColor.B = FMath::Lerp(TeamColor.B, 1.0f, 0.5f);
		TextColor.A = 1.0f;
		return TextColor;
	}
}

void UKillBoxWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshUI();
}

void UKillBoxWidget::SetTeamInfo(
	const int32 InTeamColorIndex,
	const FText& InTeamName,
	const FLinearColor& InTeamColor)
{
	TeamColorIndex = InTeamColorIndex;
	TeamName = InTeamName;
	TeamColor = InTeamColor;
	RefreshUI();
}

void UKillBoxWidget::SetKillCount(const int32 InKillCount)
{
	KillCount = FMath::Max(InKillCount, 0);
	RefreshUI();
}

void UKillBoxWidget::RefreshUI()
{
	if (Txt_Team)
	{
		Txt_Team->SetText(TeamName);
		Txt_Team->SetColorAndOpacity(FSlateColor(MakeLightTeamTextColor(TeamColor)));
	}

	if (Txt_KillCount)
	{
		Txt_KillCount->SetText(FText::AsNumber(KillCount));
	}

	FLinearColor TintColor = TeamColor;
	TintColor.A = TeamColorAlpha;

	ApplyBorderTint(Border_Name, TintColor);
	ApplyBorderTint(Border_OutLine, TintColor);
}
