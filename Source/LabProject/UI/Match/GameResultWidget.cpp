#include "UI/Match/GameResultWidget.h"
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

void UGameResultWidget::SetInfo(
	const FText& InWinnerTitle,
	const int32 InWinnerTeamColorIndex,
	const FText& InMaxKillerName,
	const int32 InMaxKillCount,
	const TArray<FGameResultPlayerStat>& InPlayerStats)
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
	bInGameScoreboardMode = true;
	WinnerTitle = FText::GetEmpty();
	WinnerTeamColorIndex = INDEX_NONE;
	PlayerStats = InPlayerStats;
	MatchResultReport::SortPlayerStats(PlayerStats);

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
		Txt_WinnerInfo->SetText(FText::Format(
			WinnerInfoFormat,
			DisplayWinnerName));
		Txt_WinnerInfo->SetColorAndOpacity(LabTeamColorUtils::GetTeamColor(WinnerTeamColorIndex));
	}

	if (Txt_MostKill)
	{
		const FText DisplayMaxKillerName = MaxKillerName.IsEmpty() ? UnknownPlayerText : MaxKillerName;
		Txt_MostKill->SetText(FText::Format(
			MostKillFormat,
			DisplayMaxKillerName,
			MaxKillCount));
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
		EndSessionCompleteHandle = OnlineSessionsSubsystem->OnEndSessionComplete.AddUObject(
			this,
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
	const ULevelDefinition* Definition =
		ULevelDefinition::ResolveDefaultDefinition();
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

	PlayerStatsContainer->ClearChildren();
	if (!PlayerStatEntryWidgetClass)
	{
		return;
	}

	// PlayerStats는 받을 때 이미 정렬해 두었다.
	for (const FGameResultPlayerStat& PlayerStat : PlayerStats)
	{
		UGameResultPlayerStatEntryWidget* EntryWidget = CreateWidget<UGameResultPlayerStatEntryWidget>(
			GetOwningPlayer(),
			PlayerStatEntryWidgetClass);
		if (!EntryWidget)
		{
			continue;
		}

		EntryWidget->SetInfo(PlayerStat, WinnerTeamColorIndex);
		EntryWidget->SetShowReward(bShowRewards && !bInGameScoreboardMode);
		PlayerStatsContainer->AddChild(EntryWidget);
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

	// ButtonOverlay contains the result-screen buttons, so keep it interactive for
	// the final result popup and hide it only while Tab is showing the scoreboard.
	UWidget* ButtonOverlay = WidgetTree->FindWidget(TEXT("ButtonOverlay"));
	if (ButtonOverlay)
	{
		ButtonOverlay->SetVisibility(
			bShowResultOnlyWidgets ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
