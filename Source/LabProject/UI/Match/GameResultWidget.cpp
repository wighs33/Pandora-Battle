#include "UI/Match/GameResultWidget.h"
#include "Algo/AllOf.h"
#include "Algo/Compare.h"
#include "UI/Core/UiSubsystem.h"
#include "UI/Core/UiScreen.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Engine/LocalPlayer.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Component/Match/MatchResultReport.h"
#include "Definition/Level/LevelDefinition.h"
#include "UI/Match/GameResultPlayerStatEntryWidget.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "UI/Common/TeamColorUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameResultWidget)

namespace
{
	bool HasSamePlayerStats(const TArray<FGameResultPlayerStat>& Left, const TArray<FGameResultPlayerStat>& Right)
	{
		return Algo::Compare(Left, Right, [](const FGameResultPlayerStat& A, const FGameResultPlayerStat& B)
		{
			return A.PlayerStateId == B.PlayerStateId && A.KillCount == B.KillCount && A.DeathCount == B.DeathCount
				&& A.GoldReward == B.GoldReward && A.TeamColorIndex == B.TeamColorIndex
				&& A.bVictoryRewardEligible == B.bVictoryRewardEligible && A.PlayerName.EqualTo(B.PlayerName)
				&& A.TeamName.EqualTo(B.TeamName);
		});
	}
}

UGameResultWidget::UGameResultWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UGameResultWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	RefreshUI();
}

void UGameResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	if (Btn_Exit)
	{
		Btn_Exit->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleExitClicked);
	}

	RefreshUI();
}

void UGameResultWidget::NativeDestruct()
{
	if (Btn_Exit)
	{
		Btn_Exit->OnClicked.RemoveDynamic(this, &ThisClass::HandleExitClicked);
	}

	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr)
	{
		if (EndSessionCompleteHandle.IsValid())
		{
			OnlineSessionsSubsystem->OnEndSessionComplete.Remove(EndSessionCompleteHandle);
			EndSessionCompleteHandle.Reset();
		}
	}

	Super::NativeDestruct();
}

void UGameResultWidget::SetInfo(const FText& InWinnerTitle, const int32 InWinnerTeamColorIndex,
	const FText& InMaxKillerName, const int32 InMaxKillCount, const TArray<FGameResultPlayerStat>& InPlayerStats)
{
	bInGameScoreboardMode = false;
	WinnerTitle = InWinnerTitle;
	WinnerTeamColorIndex = InWinnerTeamColorIndex;
	MaxKillerName = InMaxKillerName;
	MaxKillCount = InMaxKillCount;
	PlayerStats = InPlayerStats;
	MatchResultReport::SortPlayerStats(PlayerStats);
	RefreshUI();
}

void UGameResultWidget::SetInGameScoreboardInfo(const TArray<FGameResultPlayerStat>& InPlayerStats)
{
	TArray<FGameResultPlayerStat> SortedPlayerStats = InPlayerStats;
	MatchResultReport::SortPlayerStats(SortedPlayerStats);
	// 점수판은 열려 있는 동안 주기적으로 불리므로, 보이는 값이 그대로면 다시 그리지 않는다.
	if (bInGameScoreboardMode && HasSamePlayerStats(PlayerStats, SortedPlayerStats))
	{
		return;
	}

	bInGameScoreboardMode = true;
	WinnerTitle = FText::GetEmpty();
	WinnerTeamColorIndex = INDEX_NONE;
	PlayerStats = MoveTemp(SortedPlayerStats);

	if (PlayerStats.IsEmpty())
	{
		MaxKillerName = UnknownPlayerText;
		MaxKillCount = 0;
	}
	else
	{
		MaxKillerName = PlayerStats[0].PlayerName.IsEmpty() ? UnknownPlayerText : PlayerStats[0].PlayerName;
		MaxKillCount = PlayerStats[0].KillCount;
	}

	RefreshUI();
}

void UGameResultWidget::SetExitToLobbyEnabled(const bool bInExitToLobbyEnabled)
{
	bExitToLobbyEnabled = bInExitToLobbyEnabled;
	RefreshUI();
}

void UGameResultWidget::SetCloseOnlyOnExit(const bool bInCloseOnlyOnExit)
{
	bCloseOnlyOnExit = bInCloseOnlyOnExit;
}

void UGameResultWidget::SetShowRewards(const bool bInShowRewards)
{
	bShowRewards = bInShowRewards;
	RefreshUI();
}

void UGameResultWidget::RefreshUI()
{
	ApplyDisplayModeVisibility();

	if (Txt_WinnerInfo)
	{
		const FText DisplayWinnerName = WinnerTitle.IsEmpty() ? UnknownPlayerText : WinnerTitle;
		Txt_WinnerInfo->SetText(FText::Format(WinnerInfoFormat, DisplayWinnerName));
		Txt_WinnerInfo->SetColorAndOpacity(LabTeamColorUtils::GetTeamColor(WinnerTeamColorIndex));
	}

	if (Txt_MostKill)
	{
		const FText DisplayMaxKillerName = MaxKillerName.IsEmpty() ? UnknownPlayerText : MaxKillerName;
		Txt_MostKill->SetText(FText::Format(MostKillFormat, DisplayMaxKillerName, MaxKillCount));
	}

	RefreshPlayerStatsList();
}

void UGameResultWidget::HandleExitClicked()
{
	if (bCloseOnlyOnExit)
	{
        if (UCommonActivatableWidget* Screen = UCommonUIActionRouterBase::FindOwningActivatable(GetCachedWidget(), GetOwningLocalPlayer()))
            Screen->DeactivateWidget();
        RemoveFromParent();
		return;
	}

	UWorld* World = GetWorld();
	const FString LobbyMapName = GetResolvedLobbyTravelMapName();
	if (bExitToLobbyEnabled && World && World->GetAuthGameMode() && !LobbyMapName.IsEmpty())
	{
		UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
			: nullptr;
		if (!OnlineSessionsSubsystem)
		{
			TravelToLobbyMap();
			return;
		}

		if (EndSessionCompleteHandle.IsValid())
		{
			OnlineSessionsSubsystem->OnEndSessionComplete.Remove(EndSessionCompleteHandle);
			EndSessionCompleteHandle.Reset();
		}

		bPendingLobbyTravelAfterEndSession = true;
		if (Btn_Exit)
		{
			Btn_Exit->SetIsEnabled(false);
		}
		EndSessionCompleteHandle = OnlineSessionsSubsystem->OnEndSessionComplete.AddUObject(this,
			&ThisClass::HandleEndSessionForExit);
		OnlineSessionsSubsystem->EndSession();
		return;
	}

    if (UCommonActivatableWidget* Screen = UCommonUIActionRouterBase::FindOwningActivatable(GetCachedWidget(), GetOwningLocalPlayer()))
        Screen->DeactivateWidget();
    RemoveFromParent();
}

FString UGameResultWidget::GetResolvedLobbyTravelMapName() const
{
	const ULevelDefinition* Definition = ULevelDefinition::ResolveDefaultDefinition();
	return Definition ? Definition->GetLobbyTravelMapName() : FString();
}

void UGameResultWidget::HandleEndSessionForExit(const bool bWasSuccessful)
{
	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr)
	{
		if (EndSessionCompleteHandle.IsValid())
		{
			OnlineSessionsSubsystem->OnEndSessionComplete.Remove(EndSessionCompleteHandle);
			EndSessionCompleteHandle.Reset();
		}
	}

	if (!bPendingLobbyTravelAfterEndSession)
	{
		return;
	}

	bPendingLobbyTravelAfterEndSession = false;
	if (!bWasSuccessful)
	{
		if (Btn_Exit)
		{
			Btn_Exit->SetIsEnabled(true);
		}
		if (UCommonActivatableWidget* Screen = UCommonUIActionRouterBase::FindOwningActivatable(GetCachedWidget(), GetOwningLocalPlayer()))
			Screen->RequestRefreshFocus();
		return;
	}

	TravelToLobbyMap();
}

void UGameResultWidget::TravelToLobbyMap()
{
	const FString LobbyMapName = GetResolvedLobbyTravelMapName();
	UWorld* World = GetWorld();
	if (World && World->GetAuthGameMode() && !LobbyMapName.IsEmpty())
	{
		World->ServerTravel(LobbyMapName);
	}
}

void UGameResultWidget::RefreshPlayerStatsList()
{
	if (!PlayerStatsContainer)
	{
		return;
	}

	// 항목 위젯은 다시 쓰고, 인원이 바뀐 만큼만 만들거나 지운다. 항목이 아닌 자식이 섞여 있으면 처음부터 채운다.
	const bool bOnlyEntryWidgets = PlayerStatEntryWidgetClass && Algo::AllOf(PlayerStatsContainer->GetAllChildren(),
		[this](const UWidget* Child) { return Child && Child->GetClass() == PlayerStatEntryWidgetClass; });
	if (!bOnlyEntryWidgets)
	{
		PlayerStatsContainer->ClearChildren();
	}
	if (!PlayerStatEntryWidgetClass)
	{
		return;
	}
	while (PlayerStatsContainer->GetChildrenCount() > PlayerStats.Num())
	{
		PlayerStatsContainer->RemoveChildAt(PlayerStatsContainer->GetChildrenCount() - 1);
	}

	// PlayerStats는 받을 때 이미 정렬해 두었다.
	for (int32 StatIndex = 0; StatIndex < PlayerStats.Num(); ++StatIndex)
	{
		UGameResultPlayerStatEntryWidget* EntryWidget =
			Cast<UGameResultPlayerStatEntryWidget>(PlayerStatsContainer->GetChildAt(StatIndex));
		if (!EntryWidget)
		{
			EntryWidget = CreateWidget<UGameResultPlayerStatEntryWidget>(GetOwningPlayer(), PlayerStatEntryWidgetClass);
			if (!EntryWidget)
			{
				return;
			}
			PlayerStatsContainer->AddChild(EntryWidget);
		}

		EntryWidget->SetShowReward(bShowRewards && !bInGameScoreboardMode);
		EntryWidget->SetInfo(PlayerStats[StatIndex], WinnerTeamColorIndex);
	}
}

void UGameResultWidget::ApplyDisplayModeVisibility()
{
	const bool bShowResultOnlyWidgets = !bInGameScoreboardMode;
	const bool bShowRewardWidgets = bShowResultOnlyWidgets && bShowRewards;

	SetWidgetVisibleForDisplayMode(Txt_WinnerInfo, bShowResultOnlyWidgets);
	SetWidgetVisibleForDisplayMode(Txt_Reward, bShowRewardWidgets);
	if (Btn_Exit)
	{
		Btn_Exit->SetVisibility(bShowResultOnlyWidgets ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (!WidgetTree)
	{
		return;
	}

	// ButtonOverlay에 결과 화면 버튼들이 있으므로, 최종 결과 팝업에서는 입력을 받게 두고
	// Tab으로 스코어보드를 보는 동안에만 숨긴다.
	UWidget* ButtonOverlay = WidgetTree->FindWidget(TEXT("ButtonOverlay"));
	if (ButtonOverlay)
	{
		ButtonOverlay->SetVisibility(bShowResultOnlyWidgets ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UGameResultWidget::SetWidgetVisibleForDisplayMode(UWidget* Widget, const bool bVisible) const
{
	if (!Widget)
	{
		return;
	}

	Widget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UGameResultWidget::ShowResultScreen()
{
    APlayerController* Controller = GetOwningPlayer();
    if (!Controller || !Controller->GetLocalPlayer()) return;
    // 퇴장과 서버 Travel은 기존 종료 버튼에서만 실행한다.
    UUiScreen* Screen = UUiScreen::CreateBlocking(Controller, this, Btn_Exit, FSimpleDelegate::CreateLambda([]() {}));
    Controller->GetLocalPlayer()->GetSubsystem<UUiSubsystem>()->PushScreen(Screen, EUiScreenLayer::Modal);
}
