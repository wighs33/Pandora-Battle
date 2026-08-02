#include "Lobby/UI/GameResultPlayerStatEntryWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/TeamColorUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameResultPlayerStatEntryWidget)

void UGameResultPlayerStatEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshUI();
}

void UGameResultPlayerStatEntryWidget::SetInfo(const FGameResultPlayerStat& InPlayerStat, const int32 InWinnerTeamColorIndex)
{
	PlayerStat = InPlayerStat;
	WinnerTeamColorIndex = InWinnerTeamColorIndex;
	bHasPlayerStat = true;
	RefreshUI();
}

void UGameResultPlayerStatEntryWidget::SetShowReward(const bool bInShowReward)
{
	bShowReward = bInShowReward;
	RefreshUI();
}

void UGameResultPlayerStatEntryWidget::RefreshUI()
{
	if (!bHasPlayerStat)
	{
		return;
	}

	if (Txt_PlayerName)
	{
		Txt_PlayerName->SetText(PlayerStat.PlayerName.IsEmpty() ? UnknownPlayerText : PlayerStat.PlayerName);
	}

	if (Txt_TeamName)
	{
		Txt_TeamName->SetText(PlayerStat.TeamName.IsEmpty() ? UnknownTeamText : PlayerStat.TeamName);
	}

	if (Txt_Kills)
	{
		Txt_Kills->SetText(FText::AsNumber(PlayerStat.KillCount));
	}

	if (Txt_Deaths)
	{
		Txt_Deaths->SetText(FText::AsNumber(PlayerStat.DeathCount));
	}

	if (Txt_Reward)
	{
		if (bShowReward)
		{
			Txt_Reward->SetText(FText::Format(RewardTextFormat, FText::AsNumber(PlayerStat.GoldReward)));
			Txt_Reward->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Txt_Reward->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (Img_Gold)
	{
		Img_Gold->SetVisibility(bShowReward ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (ColorBorder)
	{
		ColorBorder->SetBrushColor(LabTeamColorUtils::GetTeamColorTint(PlayerStat.TeamColorIndex));
	}
}
