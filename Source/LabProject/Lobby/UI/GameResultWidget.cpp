#include "Lobby/UI/GameResultWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Definition/Level/LevelDefinition.h"
#include "Lobby/UI/GameResultPlayerStatEntryWidget.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "UI/TeamColorUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameResultWidget)

UGameResultWidget::UGameResultWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UGameResultWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ResolveExitButton();
	RefreshUI();
}

void UGameResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	ResolveExitButton();

	if (PlayerStatsContainerCandidateNames.IsEmpty())
	{
		PlayerStatsContainerCandidateNames =
		{
			TEXT("PlayerStatsContainer"),
			TEXT("VerticalBox_PlayerStats"),
			TEXT("VB_PlayerStats"),
			TEXT("ScrollBox_PlayerStats")
		};
	}

	if (ScoreboardHiddenWidgetCandidateNames.IsEmpty())
	{
		ScoreboardHiddenWidgetCandidateNames =
		{
			TEXT("Txt_WinnerInfo"),
			TEXT("Txt_Reward"),
			TEXT("Reward"),
			TEXT("Rewards"),
			TEXT("RewardContainer"),
			TEXT("RewardPanel"),
			TEXT("HorizontalBox_Reward"),
			TEXT("HB_Reward"),
			TEXT("VerticalBox_Reward"),
			TEXT("VB_Reward")
		};
	}

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
	PlayerStats.Sort([](const FGameResultPlayerStat& Left, const FGameResultPlayerStat& Right)
	{
		if (Left.KillCount != Right.KillCount)
		{
			return Left.KillCount > Right.KillCount;
		}

		if (Left.DeathCount != Right.DeathCount)
		{
			return Left.DeathCount < Right.DeathCount;
		}

		return Left.PlayerName.ToString() < Right.PlayerName.ToString();
	});
	RefreshUI();
}

void UGameResultWidget::SetInGameScoreboardInfo(const TArray<FGameResultPlayerStat>& InPlayerStats)
{
	bInGameScoreboardMode = true;
	WinnerTitle = FText::GetEmpty();
	WinnerTeamColorIndex = INDEX_NONE;
	PlayerStats = InPlayerStats;
	PlayerStats.Sort([](const FGameResultPlayerStat& Left, const FGameResultPlayerStat& Right)
	{
		if (Left.KillCount != Right.KillCount)
		{
			return Left.KillCount > Right.KillCount;
		}

		if (Left.DeathCount != Right.DeathCount)
		{
			return Left.DeathCount < Right.DeathCount;
		}

		return Left.PlayerName.ToString() < Right.PlayerName.ToString();
	});

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
	APlayerController* PlayerController = GetOwningPlayer();
	if (PlayerController)
	{
		PlayerController->bShowMouseCursor = false;
	}

	if (bCloseOnlyOnExit)
	{
		RemoveFromParent();
		if (PlayerController)
		{
			UWidgetBlueprintLibrary::SetInputMode_GameOnly(PlayerController);
			PlayerController->bEnableClickEvents = false;
			PlayerController->bEnableMouseOverEvents = false;
		}
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

	RemoveFromParent();
	if (PlayerController)
	{
		if (bExitToLobbyEnabled)
		{
			UWidgetBlueprintLibrary::SetInputMode_GameOnly(PlayerController);
			return;
		}

		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(PlayerController, nullptr, EMouseLockMode::DoNotLock, false);
		PlayerController->bShowMouseCursor = true;
		PlayerController->bEnableClickEvents = true;
		PlayerController->bEnableMouseOverEvents = true;
	}
}

FString UGameResultWidget::GetResolvedLobbyTravelMapName() const
{
	const ULevelDefinition* Definition =
		ULevelDefinition::ResolveDefaultDefinition();
	return Definition ? Definition->GetLobbyTravelMapName() : FString();
}

void UGameResultWidget::ResolveExitButton()
{
	if (Btn_Exit || !WidgetTree)
	{
		return;
	}

	static const FName ExitButtonNames[] =
	{
		TEXT("Btn_Exit"),
		TEXT("ExitButton"),
		TEXT("Button_Exit")
	};

	for (const FName ExitButtonName : ExitButtonNames)
	{
		if (UButton* ExitButton = Cast<UButton>(WidgetTree->FindWidget(ExitButtonName)))
		{
			Btn_Exit = ExitButton;
			return;
		}
	}
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
		if (APlayerController* PlayerController = GetOwningPlayer())
		{
			UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(
				PlayerController,
				this,
				EMouseLockMode::DoNotLock,
				false);
			PlayerController->bShowMouseCursor = true;
			PlayerController->bEnableClickEvents = true;
			PlayerController->bEnableMouseOverEvents = true;
		}
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

UPanelWidget* UGameResultWidget::FindPlayerStatsContainer()
{
	if (PlayerStatsContainer)
	{
		return PlayerStatsContainer;
	}

	if (!WidgetTree)
	{
		return nullptr;
	}

	for (const FName& CandidateName : PlayerStatsContainerCandidateNames)
	{
		if (CandidateName.IsNone())
		{
			continue;
		}

		if (UPanelWidget* FoundContainer = Cast<UPanelWidget>(WidgetTree->FindWidget(CandidateName)))
		{
			PlayerStatsContainer = FoundContainer;
			return FoundContainer;
		}
	}

	return nullptr;
}

void UGameResultWidget::RefreshPlayerStatsList()
{
	UPanelWidget* Container = FindPlayerStatsContainer();
	if (!Container)
	{
		return;
	}

	Container->ClearChildren();
	if (!PlayerStatEntryWidgetClass)
	{
		return;
	}

	TArray<FGameResultPlayerStat> SortedPlayerStats = PlayerStats;
	SortedPlayerStats.Sort([](const FGameResultPlayerStat& Left, const FGameResultPlayerStat& Right)
	{
		if (Left.KillCount != Right.KillCount)
		{
			return Left.KillCount > Right.KillCount;
		}

		if (Left.DeathCount != Right.DeathCount)
		{
			return Left.DeathCount < Right.DeathCount;
		}

		return Left.PlayerName.ToString() < Right.PlayerName.ToString();
	});

	for (const FGameResultPlayerStat& PlayerStat : SortedPlayerStats)
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
		Container->AddChild(EntryWidget);
	}
}

void UGameResultWidget::ApplyDisplayModeVisibility()
{
	ResolveExitButton();

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

	for (const FName& CandidateName : ScoreboardHiddenWidgetCandidateNames)
	{
		if (CandidateName.IsNone())
		{
			continue;
		}

		UWidget* CandidateWidget = WidgetTree->FindWidget(CandidateName);
		if (CandidateWidget == ButtonOverlay)
		{
			continue;
		}

		const bool bCandidateIsWinnerInfo = CandidateWidget && CandidateWidget == Txt_WinnerInfo;
		SetWidgetVisibleForDisplayMode(
			CandidateWidget,
			bCandidateIsWinnerInfo ? bShowResultOnlyWidgets : bShowRewardWidgets);
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
