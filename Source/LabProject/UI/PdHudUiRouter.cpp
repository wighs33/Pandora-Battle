#include "UI/PdHudUiRouter.h"

#include "Blueprint/UserWidget.h"
#include "Character/PdPlayer.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/UI/GameResultWidget.h"
#include "Mode/PdHUD.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "TimerManager.h"
#include "UI/InfoUiPresenter.h"
#include "UI/UiSubsystem.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/MenuPopupWidget.h"
#include "UI/Widget/PandoraTreeWidget.h"
#include "UI/Widget/PlayerHudWidget.h"
#include "UI/Widget/RightNotificationsWidget.h"
#include "UI/Widget/RightStatusWidget.h"
#include "UI/Widget/SelectPandoraWidget.h"
#include "UI/Widget/TrainingRoomMenuPopupWidget.h"
#include "Definition/UI/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdHudUiRouter)

DEFINE_LOG_CATEGORY_STATIC(LogPdHudUiRouter, Log, All);

namespace
{
	constexpr float InfoUiTrainingRoomPauseDelaySeconds = 0.03f;
	constexpr float ScoreboardRefreshIntervalSeconds = 0.20f;

	FText ResolveScoreboardTeamName(const int32 TeamColorIndex)
	{
		switch (TeamColorIndex)
		{
		case 0:
			return NSLOCTEXT("GameResult", "TeamNameRed", "Red");
		case 1:
			return NSLOCTEXT("GameResult", "TeamNameBlue", "Blue");
		case 2:
			return NSLOCTEXT("GameResult", "TeamNameYellow", "Yellow");
		case 3:
			return NSLOCTEXT("GameResult", "TeamNamePurple", "Purple");
		case 4:
			return NSLOCTEXT("GameResult", "TeamNameGreen", "Green");
		case 5:
			return NSLOCTEXT("GameResult", "TeamNameOrange", "Orange");
		default:
			return NSLOCTEXT("GameResult", "TeamNameNone", "No Team");
		}
	}

	FText ResolveScoreboardPlayerName(const APlayerState* PlayerState)
	{
		if (!PlayerState)
		{
			return NSLOCTEXT("GameResult", "UnknownPlayerName", "Unknown");
		}

		if (const APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState))
		{
			if (!PdPlayerState->GetPlayerMatchComponent()->GetMatchDisplayName().IsEmpty())
			{
				return PdPlayerState->GetPlayerMatchComponent()->GetMatchDisplayName();
			}
		}

		const FString PlayerName = PlayerState->GetPlayerName();
		return FText::FromString(PlayerName.IsEmpty() ? GetNameSafe(PlayerState) : PlayerName);
	}
}

void UPdHudScreenLayer::Initialize(APdHUD* InOwnerHud, UPdHudUiRouter* InRouter)
{
	OwnerHud = InOwnerHud;
	Router = InRouter;
}

void UPdHudScreenLayer::Shutdown()
{
	APdHUD* Hud = OwnerHud.Get();
	ClearInfoCloseTimer();
	ClearTrainingRoomPauseTimer();
	SetTrainingRoomPaused(false);
	RestoreInfoInputLock();

	if (Hud && Hud->CachedPandoraTreeUI)
	{
		Hud->CachedPandoraTreeUI->OnPandoraTreeClosed.RemoveDynamic(
			this,
			&ThisClass::HandlePandoraTreeClosed);
	}

	bInfoClosing = false;
	bPandoraTreeClosing = false;
}

void UPdHudScreenLayer::OpenInfo()
{
	APdHUD* Hud = OwnerHud.Get();
	UPdHudUiRouter* UiRouter = Router.Get();
	APdPlayerController* Controller = Hud ? Hud->GetPdController() : nullptr;
	if (!Hud || !UiRouter || !Controller)
	{
		return;
	}

	ClearInfoCloseTimer();
	UiRouter->EnsureCoreLayers();
	if (!Hud->CachedInfoUI)
	{
		return;
	}

	if (UiRouter->IsSettingsMenuOpen())
	{
		UiRouter->CloseSettingsMenu();
	}
	if (IsPandoraTreeOpen())
	{
		ClosePandoraTree(true, true);
	}

	bInfoClosing = false;
	Hud->CachedInfoUI->SetReturnCameraOnHide(true);
	Hud->CachedInfoUI->OnClickedInfoCenterButton.Clear();
	Hud->CachedInfoUI->OnClickedSettingButton.RemoveDynamic(Hud, &APdHUD::OpenSettingsMenu);
	Hud->CachedInfoUI->OnClickedSettingButton.AddUniqueDynamic(Hud, &APdHUD::OpenSettingsMenu);

	UInfoUiPresenter* Presenter = Hud->GetInfoUiPresenter();
	if (Presenter)
	{
		Presenter->BindInfoUi(Hud->CachedInfoUI);
		Hud->CachedInfoUI->OnClickedInfoCenterButton.AddUniqueDynamic(
			Presenter,
			&UInfoUiPresenter::HandleClickedInfoCenterButton);
	}

	if (URightStatusWidget* RightStatusWidget = Hud->CachedInfoUI->GetRightStatusWidget())
	{
		Hud->ApplyStatusViewModelToWidget(RightStatusWidget);
	}
	Hud->ApplyStatusViewModelToWidgetTree(Hud->CachedInfoUI);
	Hud->ApplyInventoryWidgetSettings();

	if (!Hud->CachedInfoUI->IsInViewport())
	{
		Hud->CachedInfoUI->AddToViewport();
	}

	Hud->ToggleUiMode(true);
	ApplyInfoInputLock();
	Hud->CachedInfoUI->ShowInfoUi();
	ScheduleTrainingRoomPause(InfoUiTrainingRoomPauseDelaySeconds);

	if (Presenter)
	{
		Presenter->HandleOpenedInfoUi();
	}
	Hud->CachedInfoUI->SelectProfileTab();
}

void UPdHudScreenLayer::CloseInfo(
	const bool bSuppressCameraReturn,
	const bool bImmediate)
{
	APdHUD* Hud = OwnerHud.Get();
	if (!Hud)
	{
		return;
	}

	if (APdPlayerController* Controller = Hud->GetPdController())
	{
		if (APdPlayer* PlayerCharacter = Cast<APdPlayer>(Controller->GetPawn()))
		{
			PlayerCharacter->HidePaintCanvas();
		}
	}

	if (!Hud->CachedInfoUI)
	{
		Hud->RefreshPlayerHudVisibility();
		RefreshTrainingRoomPause();
		RestoreInfoInputLock();
		Hud->ToggleUiMode(false);
		return;
	}

	ClearInfoCloseTimer();
	bInfoClosing = true;
	Hud->CachedInfoUI->SetReturnCameraOnHide(!bSuppressCameraReturn);
	Hud->CachedInfoUI->HideInfoUi();
	RefreshTrainingRoomPause(Hud->CachedInfoUI);

	if (bImmediate)
	{
		FinishCloseInfo();
		return;
	}

	const float HideAnimationDelay = Hud->CachedInfoUI->GetHideAnimationDelay();
	if (HideAnimationDelay > 0.0f)
	{
		Hud->GetWorldTimerManager().SetTimer(
			InfoCloseTimerHandle,
			this,
			&ThisClass::FinishCloseInfo,
			HideAnimationDelay,
			false);
		return;
	}

	FinishCloseInfo();
}

void UPdHudScreenLayer::ToggleInfo()
{
	if (IsInfoOpen())
	{
		if (!bInfoClosing)
		{
			CloseInfo();
		}
		return;
	}
	OpenInfo();
}

void UPdHudScreenLayer::OpenPandoraTree()
{
	APdHUD* Hud = OwnerHud.Get();
	UPdHudUiRouter* UiRouter = Router.Get();
	APdPlayerController* Controller = Hud ? Hud->GetPdController() : nullptr;
	if (!Hud || !UiRouter || !Controller)
	{
		return;
	}

	UiRouter->EnsureCoreLayers();
	if (!Hud->CachedPandoraTreeUI)
	{
		return;
	}

	if (UiRouter->IsSettingsMenuOpen())
	{
		UiRouter->CloseSettingsMenu();
	}
	if (IsInfoOpen())
	{
		CloseInfo(true, true);
	}

	bPandoraTreeClosing = false;
	Hud->CachedPandoraTreeUI->SetReturnCameraOnHide(true);
	Hud->CachedPandoraTreeUI->OnPandoraTreeClosed.RemoveDynamic(
		this,
		&ThisClass::HandlePandoraTreeClosed);
	Hud->CachedPandoraTreeUI->OnPandoraTreeClosed.AddUniqueDynamic(
		this,
		&ThisClass::HandlePandoraTreeClosed);

	if (!Hud->CachedPandoraTreeUI->IsInViewport())
	{
		Hud->CachedPandoraTreeUI->AddToViewport();
	}

	Hud->CachedPandoraTreeUI->ShowPandoraTree();
	Hud->ToggleUiMode(true);
	ScheduleTrainingRoomPause(Hud->CachedPandoraTreeUI->GetPreviewCameraShowBlendTime());
}

void UPdHudScreenLayer::ClosePandoraTree(
	const bool bSuppressCameraReturn,
	const bool bImmediate)
{
	APdHUD* Hud = OwnerHud.Get();
	if (!Hud || !Hud->CachedPandoraTreeUI)
	{
		return;
	}

	bPandoraTreeClosing = true;
	Hud->CachedPandoraTreeUI->SetReturnCameraOnHide(!bSuppressCameraReturn);
	if (bImmediate)
	{
		Hud->CachedPandoraTreeUI->HidePandoraTreeImmediately();
	}
	else
	{
		Hud->CachedPandoraTreeUI->HidePandoraTree();
	}
	RefreshTrainingRoomPause(Hud->CachedPandoraTreeUI);
}

void UPdHudScreenLayer::TogglePandoraTree()
{
	if (IsPandoraTreeOpen())
	{
		if (!bPandoraTreeClosing)
		{
			ClosePandoraTree();
		}
		return;
	}
	OpenPandoraTree();
}

bool UPdHudScreenLayer::IsInfoOpen() const
{
	const APdHUD* Hud = OwnerHud.Get();
	return Hud && Hud->CachedInfoUI && Hud->CachedInfoUI->IsInViewport();
}

bool UPdHudScreenLayer::IsPandoraTreeOpen() const
{
	const APdHUD* Hud = OwnerHud.Get();
	return Hud && Hud->CachedPandoraTreeUI && Hud->CachedPandoraTreeUI->IsInViewport();
}

bool UPdHudScreenLayer::IsBlockingGameplayInput() const
{
	return (!bInfoClosing && IsInfoOpen())
		|| (!bPandoraTreeClosing && IsPandoraTreeOpen());
}

void UPdHudScreenLayer::RefreshTrainingRoomPause(const UUserWidget* IgnoredWidget)
{
	ClearTrainingRoomPauseTimer();
	SetTrainingRoomPaused(IsTrainingRoomPauseUiOpen(IgnoredWidget));
}

void UPdHudScreenLayer::ScheduleTrainingRoomPause(const float DelaySeconds)
{
	APdHUD* Hud = OwnerHud.Get();
	ClearTrainingRoomPauseTimer();
	if (!Hud || !Hud->IsTrainingRoomMap())
	{
		return;
	}

	if (DelaySeconds <= 0.0f)
	{
		RefreshTrainingRoomPause();
		return;
	}

	Hud->GetWorldTimerManager().SetTimer(
		TrainingRoomPauseTimerHandle,
		this,
		&ThisClass::HandleDelayedTrainingRoomPause,
		DelaySeconds,
		false);
}

void UPdHudScreenLayer::HandlePandoraTreeClosed(UPandoraTreeWidget* ClosedWidget)
{
	APdHUD* Hud = OwnerHud.Get();
	if (!Hud || Hud->CachedPandoraTreeUI != ClosedWidget)
	{
		return;
	}

	bPandoraTreeClosing = false;
	Hud->CachedPandoraTreeUI->SetReturnCameraOnHide(true);
	RefreshTrainingRoomPause();
	Hud->RefreshPlayerHudVisibility();
	Hud->ToggleUiMode(false);
}

void UPdHudScreenLayer::FinishCloseInfo()
{
	APdHUD* Hud = OwnerHud.Get();
	ClearInfoCloseTimer();
	if (!Hud)
	{
		return;
	}

	if (Hud->CachedInfoUI)
	{
		Hud->CachedInfoUI->RemoveFromParent();
		Hud->CachedInfoUI->SetReturnCameraOnHide(true);
	}
	bInfoClosing = false;

	Hud->RefreshPlayerHudVisibility();
	RefreshTrainingRoomPause();
	RestoreInfoInputLock();
	Hud->ToggleUiMode(false);
}

void UPdHudScreenLayer::ClearInfoCloseTimer()
{
	if (APdHUD* Hud = OwnerHud.Get())
	{
		Hud->GetWorldTimerManager().ClearTimer(InfoCloseTimerHandle);
	}
	InfoCloseTimerHandle.Invalidate();
}

void UPdHudScreenLayer::ClearTrainingRoomPauseTimer()
{
	if (APdHUD* Hud = OwnerHud.Get())
	{
		Hud->GetWorldTimerManager().ClearTimer(TrainingRoomPauseTimerHandle);
	}
	TrainingRoomPauseTimerHandle.Invalidate();
}

void UPdHudScreenLayer::HandleDelayedTrainingRoomPause()
{
	TrainingRoomPauseTimerHandle.Invalidate();
	RefreshTrainingRoomPause();
}

bool UPdHudScreenLayer::IsTrainingRoomPauseUiOpen(const UUserWidget* IgnoredWidget) const
{
	const APdHUD* Hud = OwnerHud.Get();
	const UPdHudUiRouter* UiRouter = Router.Get();
	if (!Hud)
	{
		return false;
	}

	const auto IsPauseWidgetOpen = [IgnoredWidget](const UUserWidget* Widget)
	{
		return Widget
			&& Widget != IgnoredWidget
			&& Widget->IsInViewport()
			&& Widget->GetVisibility() != ESlateVisibility::Collapsed
			&& Widget->GetVisibility() != ESlateVisibility::Hidden;
	};

	return (!bInfoClosing && IsPauseWidgetOpen(Hud->CachedInfoUI))
		|| IsPauseWidgetOpen(UiRouter ? UiRouter->GetSettingsMenuWidget() : nullptr)
		|| (!bPandoraTreeClosing && IsPauseWidgetOpen(Hud->CachedPandoraTreeUI));
}

void UPdHudScreenLayer::SetTrainingRoomPaused(const bool bPaused)
{
	APdHUD* Hud = OwnerHud.Get();
	UWorld* World = Hud ? Hud->GetWorld() : nullptr;
	if (!Hud || !World)
	{
		bAppliedTrainingRoomPause = false;
		return;
	}

	if (bPaused)
	{
		if (bAppliedTrainingRoomPause
			|| !Hud->IsTrainingRoomMap()
			|| World->GetNetMode() != NM_Standalone)
		{
			return;
		}

		UGameplayStatics::SetGamePaused(Hud, true);
		bAppliedTrainingRoomPause = true;
		return;
	}

	if (bAppliedTrainingRoomPause)
	{
		UGameplayStatics::SetGamePaused(Hud, false);
		bAppliedTrainingRoomPause = false;
	}
}

void UPdHudScreenLayer::ApplyInfoInputLock()
{
	APdHUD* Hud = OwnerHud.Get();
	APdPlayerController* Controller = Hud ? Hud->GetPdController() : nullptr;
	if (!Controller || bInfoInputLockApplied)
	{
		return;
	}

	bPreviousLookInputIgnored = Controller->IsLookInputIgnored();
	bPreviousMoveInputIgnored = Controller->IsMoveInputIgnored();
	bInfoInputLockApplied = true;
	Controller->SetIgnoreLookInput(true);
	Controller->SetIgnoreMoveInput(true);
}

void UPdHudScreenLayer::RestoreInfoInputLock()
{
	APdHUD* Hud = OwnerHud.Get();
	APdPlayerController* Controller = Hud ? Hud->GetPdController() : nullptr;
	if (!Controller || !bInfoInputLockApplied)
	{
		return;
	}

	Controller->SetIgnoreLookInput(bPreviousLookInputIgnored);
	Controller->SetIgnoreMoveInput(bPreviousMoveInputIgnored);
	bInfoInputLockApplied = false;
	bPreviousLookInputIgnored = false;
	bPreviousMoveInputIgnored = false;
}

void UPdHudMenuLayer::Initialize(APdHUD* InOwnerHud, UPdHudUiRouter* InRouter)
{
	OwnerHud = InOwnerHud;
	Router = InRouter;
}

bool UPdHudMenuLayer::Open()
{
	APdHUD* Hud = OwnerHud.Get();
	UPdHudUiRouter* UiRouter = Router.Get();
	APdPlayerController* Controller = Hud ? Hud->GetPdController() : nullptr;
	const UWidgetClassDefinition* Definition = UiRouter ? UiRouter->GetActiveDefinition() : nullptr;
	if (!Hud || !Controller || !Controller->IsLocalController() || !Definition
		|| Hud->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	if (IsOpen())
	{
		Hud->ToggleUiMode(true);
		return true;
	}

	if (Hud->CachedInfoUI && Hud->CachedInfoUI->IsInViewport())
	{
		Hud->CloseInfoUiInternal(false, true);
	}
	if (Hud->CachedPandoraTreeUI && Hud->CachedPandoraTreeUI->IsInViewport())
	{
		Hud->ClosePandoraTreeUiInternal(false, true);
	}

	TSubclassOf<UMenuPopupWidget> MenuPopupClass;
	if (Hud->IsTrainingRoomMap())
	{
		const TSubclassOf<class UTrainingRoomMenuPopupWidget> TrainingMenuClass =
			Definition->GetTrainingRoomMenuPopupWidgetClass();
		if (TrainingMenuClass)
		{
			MenuPopupClass = TSubclassOf<UMenuPopupWidget>(TrainingMenuClass.Get());
		}
	}
	if (!MenuPopupClass)
	{
		MenuPopupClass = Definition->GetMenuPopupWidgetClass();
	}
	if (!MenuPopupClass)
	{
		UE_LOG(LogPdHudUiRouter, Error, TEXT("Settings menu layer has no configured widget class."));
		return false;
	}

	ActiveWidget = CreateWidget<UMenuPopupWidget>(Controller, MenuPopupClass);
	if (!ActiveWidget)
	{
		UE_LOG(LogPdHudUiRouter, Error, TEXT("Failed to create settings menu widget."));
		return false;
	}

	ActiveWidget->SetInputModeManagedExternally(true);
	ActiveWidget->SetRestoreGameInputOnClose(false);
	ActiveWidget->OnMenuClosed.AddUniqueDynamic(this, &ThisClass::HandleMenuClosed);
	ActiveWidget->AddToViewport(100);
	Hud->ToggleUiMode(true);
	Hud->RefreshTrainingRoomUiPause();
	return true;
}

bool UPdHudMenuLayer::Toggle()
{
	return IsOpen() ? Close() : Open();
}

bool UPdHudMenuLayer::Close()
{
	if (!ActiveWidget)
	{
		return false;
	}

	UMenuPopupWidget* MenuWidget = ActiveWidget.Get();
	if (!MenuWidget->IsInViewport())
	{
		ActiveWidget = nullptr;
		if (APdHUD* Hud = OwnerHud.Get())
		{
			Hud->RefreshTrainingRoomUiPause();
		}
		return false;
	}

	MenuWidget->SetRestoreGameInputOnClose(false);
	MenuWidget->CloseMenu();
	return true;
}

bool UPdHudMenuLayer::IsOpen() const
{
	return IsValid(ActiveWidget) && ActiveWidget->IsInViewport();
}

void UPdHudMenuLayer::Shutdown()
{
	if (IsValid(ActiveWidget))
	{
		ActiveWidget->OnMenuClosed.RemoveDynamic(this, &ThisClass::HandleMenuClosed);
		ActiveWidget->RemoveFromParent();
	}
	ActiveWidget = nullptr;
}

void UPdHudMenuLayer::HandleMenuClosed(UMenuPopupWidget* ClosedWidget)
{
	if (ActiveWidget != ClosedWidget)
	{
		return;
	}

	if (ActiveWidget)
	{
		ActiveWidget->OnMenuClosed.RemoveDynamic(this, &ThisClass::HandleMenuClosed);
	}
	ActiveWidget = nullptr;

	if (APdHUD* Hud = OwnerHud.Get())
	{
		Hud->HandleSettingsMenuLayerClosed();
	}
}

void UPdHudScoreboardLayer::Initialize(APdHUD* InOwnerHud, UPdHudUiRouter* InRouter)
{
	OwnerHud = InOwnerHud;
	Router = InRouter;
}

void UPdHudScoreboardLayer::Show()
{
	APdHUD* Hud = OwnerHud.Get();
	const UPdHudUiRouter* UiRouter = Router.Get();
	APdPlayerController* Controller = Hud ? Hud->GetPdController() : nullptr;
	const UWidgetClassDefinition* Definition = UiRouter ? UiRouter->GetActiveDefinition() : nullptr;
	if (!Hud || !Controller || !Controller->IsLocalController() || !Definition
		|| Hud->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!ScoreboardWidget)
	{
		if (const TSubclassOf<UGameResultWidget> WidgetClass = Definition->GetGameResultWidgetClass())
		{
			ScoreboardWidget = CreateWidget<UGameResultWidget>(Controller, WidgetClass);
		}
	}
	if (!ScoreboardWidget)
	{
		return;
	}

	Refresh();
	if (!ScoreboardWidget->IsInViewport())
	{
		ScoreboardWidget->AddToViewport(80);
	}

	if (UWorld* World = Hud->GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
		World->GetTimerManager().SetTimer(
			RefreshTimerHandle,
			this,
			&ThisClass::Refresh,
			ScoreboardRefreshIntervalSeconds,
			true);
	}

	Hud->RefreshPlayerHudVisibility();
}

void UPdHudScoreboardLayer::Hide()
{
	if (APdHUD* Hud = OwnerHud.Get())
	{
		Hud->GetWorldTimerManager().ClearTimer(RefreshTimerHandle);
	}
	RefreshTimerHandle.Invalidate();

	if (ScoreboardWidget)
	{
		ScoreboardWidget->RemoveFromParent();
	}

	if (APdHUD* Hud = OwnerHud.Get())
	{
		Hud->RefreshPlayerHudVisibility();
	}
}

bool UPdHudScoreboardLayer::IsOpen() const
{
	return ScoreboardWidget && ScoreboardWidget->IsInViewport();
}

void UPdHudScoreboardLayer::Refresh()
{
	if (!ScoreboardWidget)
	{
		return;
	}

	TArray<FGameResultPlayerStat> PlayerStats;
	BuildPlayerStats(PlayerStats);
	ScoreboardWidget->SetInGameScoreboardInfo(PlayerStats);
}

void UPdHudScoreboardLayer::Shutdown()
{
	Hide();
	ScoreboardWidget = nullptr;
}

void UPdHudScoreboardLayer::BuildPlayerStats(TArray<FGameResultPlayerStat>& OutPlayerStats) const
{
	OutPlayerStats.Reset();

	const APdHUD* Hud = OwnerHud.Get();
	const UWorld* World = Hud ? Hud->GetWorld() : nullptr;
	const AGameStateBase* CurrentGameState = World ? World->GetGameState() : nullptr;
	if (!CurrentGameState)
	{
		return;
	}

	for (APlayerState* PlayerState : CurrentGameState->PlayerArray)
	{
		const APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState);
		if (!PdPlayerState)
		{
			continue;
		}

		FGameResultPlayerStat PlayerStat;
		PlayerStat.PlayerName = ResolveScoreboardPlayerName(PdPlayerState);
		PlayerStat.TeamColorIndex = PdPlayerState->GetPlayerMatchComponent()->GetMatchTeamColorIndex();
		PlayerStat.TeamName = ResolveScoreboardTeamName(PlayerStat.TeamColorIndex);
		PlayerStat.PlayerStateId = PdPlayerState->GetPlayerId();
		PlayerStat.KillCount = PdPlayerState->GetPlayerMatchComponent()->GetKillCount();
		PlayerStat.DeathCount = PdPlayerState->GetPlayerMatchComponent()->GetDeathCount();
		PlayerStat.GoldReward = 0;
		PlayerStat.bVictoryRewardEligible = false;
		OutPlayerStats.Add(PlayerStat);
	}

	OutPlayerStats.Sort([](const FGameResultPlayerStat& Left, const FGameResultPlayerStat& Right)
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
}

void UPdHudUiRouter::Initialize(APdHUD* InOwnerHud)
{
	if (!IsValid(InOwnerHud) || OwnerHud.Get() == InOwnerHud)
	{
		return;
	}

	Shutdown();
	OwnerHud = InOwnerHud;

	MenuLayer = NewObject<UPdHudMenuLayer>(this);
	MenuLayer->Initialize(InOwnerHud, this);

	ScreenLayer = NewObject<UPdHudScreenLayer>(this);
	ScreenLayer->Initialize(InOwnerHud, this);

	ScoreboardLayer = NewObject<UPdHudScoreboardLayer>(this);
	ScoreboardLayer->Initialize(InOwnerHud, this);
}

void UPdHudUiRouter::Shutdown()
{
	ResetLayers();

	UWidgetClassDefinition* PreviousDefinition = ActiveDefinition;
	DefinitionRequests.Reset();
	ActiveDefinition = nullptr;
	if (APdHUD* Hud = OwnerHud.Get())
	{
		Hud->WidgetClassDefinition = nullptr;
	}
	if (UUiSubsystem* UiSubsystem = ResolveUiSubsystem())
	{
		UiSubsystem->ClearWidgetClassDefinition(PreviousDefinition);
	}

	MenuLayer = nullptr;
	ScreenLayer = nullptr;
	ScoreboardLayer = nullptr;
	OwnerHud.Reset();
}

bool UPdHudUiRouter::AddDefinitionRequest(UWidgetClassDefinition* Definition)
{
	if (!IsValid(Definition) || !OwnerHud.IsValid())
	{
		return false;
	}

	UWidgetClassDefinition* PreviousDefinition = ActiveDefinition;
	DefinitionRequests.Add(Definition);
	ApplyActiveDefinition(Definition);
	return PreviousDefinition != ActiveDefinition;
}

bool UPdHudUiRouter::RemoveDefinitionRequest(const UWidgetClassDefinition* Definition)
{
	if (!Definition)
	{
		return false;
	}

	const int32 RequestIndex = DefinitionRequests.FindLastByPredicate(
		[Definition](const TObjectPtr<UWidgetClassDefinition>& Request)
		{
			return Request == Definition;
		});
	if (RequestIndex == INDEX_NONE)
	{
		return false;
	}

	UWidgetClassDefinition* PreviousDefinition = ActiveDefinition;
	DefinitionRequests.RemoveAt(RequestIndex);
	UWidgetClassDefinition* NewDefinition =
		DefinitionRequests.IsEmpty() ? nullptr : DefinitionRequests.Last().Get();
	ApplyActiveDefinition(NewDefinition);
	return PreviousDefinition != ActiveDefinition;
}

void UPdHudUiRouter::EnsureCoreLayers()
{
	APdHUD* Hud = OwnerHud.Get();
	APdPlayerController* Controller = ResolvePlayerController();
	UWidgetClassDefinition* Definition = ActiveDefinition;
	if (!Hud || !Controller || !Controller->IsLocalController() || !Definition)
	{
		return;
	}

	Hud->RefreshUiBindings();
	if (UUiSubsystem* UiSubsystem = ResolveUiSubsystem())
	{
		if (UiSubsystem->IsTravelLoadingScreenActive())
		{
			UiSubsystem->ShowTravelLoadingScreen();
		}
		else
		{
			UiSubsystem->HideConnectingPopup();
		}
	}

	if (!Hud->CachedPlayerHUD)
	{
		if (const TSubclassOf<UUserWidget> WidgetClass = Definition->GetPlayerHudWidgetClass())
		{
			Hud->CachedPlayerHUD = CreateWidget<UUserWidget>(Controller, WidgetClass);
			if (UPlayerHudWidget* PlayerHudWidget = Cast<UPlayerHudWidget>(Hud->CachedPlayerHUD))
			{
				PlayerHudWidget->InitializePlayerHud(Definition);
			}
		}
	}
	if (Hud->CachedPlayerHUD && !Hud->CachedPlayerHUD->IsInViewport())
	{
		Hud->CachedPlayerHUD->AddToViewport();
	}
	// Core layers can be supplied after a derived HUD has already opened a
	// full-screen UI (for example, the lobby widget). Always apply the current
	// policy after creation instead of assuming a newly added HUD is visible.
	Hud->RefreshPlayerHudVisibility();
	Hud->ApplyStatusViewModelToPlayerHud();
	Hud->ApplyHudTimerVisibility();

	if (!Hud->CachedInfoUI)
	{
		if (const TSubclassOf<UInfoWidget> WidgetClass = Definition->GetInfoWidgetClass())
		{
			Hud->CachedInfoUI = CreateWidget<UInfoWidget>(Controller, WidgetClass);
		}
	}
	if (!Hud->CachedSelectPandoraUI)
	{
		if (const TSubclassOf<USelectPandoraWidget> WidgetClass = Definition->GetSelectPandoraWidgetClass())
		{
			Hud->CachedSelectPandoraUI = CreateWidget<USelectPandoraWidget>(Controller, WidgetClass);
		}
	}
	if (!Hud->AimCrosshairWidget)
	{
		if (const TSubclassOf<UUserWidget> WidgetClass = Definition->GetAimCrosshairWidgetClass())
		{
			Hud->AimCrosshairWidget = CreateWidget<UUserWidget>(Controller, WidgetClass);
		}
	}
	if (!Hud->CachedPandoraTreeUI)
	{
		if (const TSubclassOf<UPandoraTreeWidget> WidgetClass = Definition->GetPandoraTreeWidgetClass())
		{
			Hud->CachedPandoraTreeUI = CreateWidget<UPandoraTreeWidget>(Controller, WidgetClass);
			if (Hud->CachedPandoraTreeUI)
			{
				Hud->CachedPandoraTreeUI->SetInputModeManagedExternally(true);
			}
		}
	}
	if (!Hud->CachedRightNotificationsUI)
	{
		if (const TSubclassOf<URightNotificationsWidget> WidgetClass =
			Definition->GetRightNotificationsWidgetClass())
		{
			Hud->CachedRightNotificationsUI =
				CreateWidget<URightNotificationsWidget>(Controller, WidgetClass);
		}
	}
	if (Hud->CachedRightNotificationsUI && !Hud->CachedRightNotificationsUI->IsInViewport())
	{
		Hud->CachedRightNotificationsUI->AddToViewport(20);
	}

	if (Hud->CachedInfoUI)
	{
		Hud->CachedInfoUI->OnClickedInfoCenterButton.Clear();
		Hud->CachedInfoUI->OnClickedSettingButton.RemoveDynamic(Hud, &APdHUD::OpenSettingsMenu);
		Hud->CachedInfoUI->OnClickedSettingButton.AddUniqueDynamic(Hud, &APdHUD::OpenSettingsMenu);
		if (UInfoUiPresenter* Presenter = Hud->GetInfoUiPresenter())
		{
			Presenter->BindInfoUi(Hud->CachedInfoUI);
			Hud->CachedInfoUI->OnClickedInfoCenterButton.AddUniqueDynamic(
				Presenter,
				&UInfoUiPresenter::HandleClickedInfoCenterButton);
		}

		if (URightStatusWidget* RightStatusWidget = Hud->CachedInfoUI->GetRightStatusWidget())
		{
			Hud->ApplyStatusViewModelToWidget(RightStatusWidget);
		}
		Hud->ApplyStatusViewModelToWidgetTree(Hud->CachedInfoUI);
		Hud->ApplyInventoryWidgetSettings();
	}

	if (Hud->CachedSelectPandoraUI)
	{
		Hud->CachedSelectPandoraUI->OnSelected.Clear();
		if (UInfoUiPresenter* Presenter = Hud->GetInfoUiPresenter())
		{
			Hud->CachedSelectPandoraUI->OnSelected.AddUniqueDynamic(
				Presenter,
				&UInfoUiPresenter::HandleSelectedPandoraDirection);
		}
	}
}

void UPdHudUiRouter::ResetLayers()
{
	ReleaseInput();
	if (ScreenLayer)
	{
		ScreenLayer->Shutdown();
	}
	if (MenuLayer)
	{
		MenuLayer->Shutdown();
	}
	if (ScoreboardLayer)
	{
		ScoreboardLayer->Shutdown();
	}

	APdHUD* Hud = OwnerHud.Get();
	if (!Hud)
	{
		return;
	}

	HideAimCrosshair();
	Hud->AimCrosshairWidget = nullptr;

	if (Hud->CachedPlayerHUD)
	{
		Hud->CachedPlayerHUD->RemoveFromParent();
		Hud->CachedPlayerHUD = nullptr;
	}
	if (Hud->CachedInfoUI)
	{
		Hud->CachedInfoUI->SetReturnCameraOnHide(true);
		Hud->CachedInfoUI->RemoveFromParent();
		Hud->CachedInfoUI = nullptr;
	}
	if (Hud->CachedSelectPandoraUI)
	{
		Hud->CachedSelectPandoraUI->RemoveFromParent();
		Hud->CachedSelectPandoraUI = nullptr;
	}
	if (Hud->CachedPandoraTreeUI)
	{
		Hud->CachedPandoraTreeUI->SetReturnCameraOnHide(true);
		Hud->CachedPandoraTreeUI->RemoveFromParent();
		Hud->CachedPandoraTreeUI = nullptr;
	}
	if (Hud->CachedRightNotificationsUI)
	{
		Hud->CachedRightNotificationsUI->RemoveFromParent();
		Hud->CachedRightNotificationsUI = nullptr;
	}
}

void UPdHudUiRouter::RouteInput(
	UWidget* FocusWidget,
	const bool bPreserveGameplayInputMode,
	const bool bCenterCursor)
{
	UUiSubsystem* UiSubsystem = ResolveUiSubsystem();
	APdPlayerController* Controller = ResolvePlayerController();
	if (!UiSubsystem || !Controller)
	{
		return;
	}

	FPdUiModalInputConfig InputConfig;
	InputConfig.RestorePolicy = EPdUiInputRestorePolicy::Gameplay;
	if (bPreserveGameplayInputMode)
	{
		InputConfig.InputMode = EPdUiInputMode::GameOnly;
		InputConfig.bApplyInputMode = false;
	}

	if (!UiSubsystem->UpdateModalInput(this, ModalInputToken, FocusWidget, InputConfig))
	{
		ModalInputToken.Invalidate();
		ModalInputToken = UiSubsystem->AcquireModalInput(this, FocusWidget, InputConfig);
	}
	if (!ModalInputToken.IsValid())
	{
		UE_LOG(LogPdHudUiRouter, Error, TEXT("Failed to acquire the HUD modal input route."));
		return;
	}

	if (bCenterCursor)
	{
		int32 ViewportSizeX = 0;
		int32 ViewportSizeY = 0;
		Controller->GetViewportSize(ViewportSizeX, ViewportSizeY);
		Controller->SetMouseLocation(ViewportSizeX / 2, ViewportSizeY / 2);
	}
}

void UPdHudUiRouter::ReleaseInput()
{
	if (ModalInputToken.IsValid())
	{
		if (UUiSubsystem* UiSubsystem = ResolveUiSubsystem())
		{
			UiSubsystem->ReleaseModalInput(this, ModalInputToken);
		}
	}
	ModalInputToken.Invalidate();
}

bool UPdHudUiRouter::OpenSettingsMenu()
{
	return MenuLayer && MenuLayer->Open();
}

bool UPdHudUiRouter::ToggleSettingsMenu()
{
	return MenuLayer && MenuLayer->Toggle();
}

bool UPdHudUiRouter::CloseSettingsMenu()
{
	return MenuLayer && MenuLayer->Close();
}

bool UPdHudUiRouter::IsSettingsMenuOpen() const
{
	return MenuLayer && MenuLayer->IsOpen();
}

UMenuPopupWidget* UPdHudUiRouter::GetSettingsMenuWidget() const
{
	return MenuLayer ? MenuLayer->GetWidget() : nullptr;
}

void UPdHudUiRouter::ShowScoreboard()
{
	if (ScoreboardLayer)
	{
		ScoreboardLayer->Show();
	}
}

void UPdHudUiRouter::HideScoreboard()
{
	if (ScoreboardLayer)
	{
		ScoreboardLayer->Hide();
	}
}

void UPdHudUiRouter::RefreshScoreboard()
{
	if (ScoreboardLayer)
	{
		ScoreboardLayer->Refresh();
	}
}

bool UPdHudUiRouter::IsScoreboardOpen() const
{
	return ScoreboardLayer && ScoreboardLayer->IsOpen();
}

void UPdHudUiRouter::ShowAimCrosshair(const FGameplayTag DesiredCrosshairWidgetTag)
{
	APdHUD* Hud = OwnerHud.Get();
	APdPlayerController* Controller = ResolvePlayerController();
	if (!Hud || !Controller || !ActiveDefinition)
	{
		return;
	}

	TSubclassOf<UUserWidget> DesiredWidgetClass =
		ActiveDefinition->FindWidgetClassByTag(DesiredCrosshairWidgetTag);
	if (!DesiredWidgetClass)
	{
		DesiredWidgetClass = ActiveDefinition->GetAimCrosshairWidgetClass();
	}
	if (!DesiredWidgetClass)
	{
		return;
	}

	if (!Hud->AimCrosshairWidget || Hud->AimCrosshairWidget->GetClass() != DesiredWidgetClass)
	{
		HideAimCrosshair();
		Hud->AimCrosshairWidget = CreateWidget<UUserWidget>(Controller, DesiredWidgetClass);
	}
	if (Hud->AimCrosshairWidget && !Hud->AimCrosshairWidget->IsInViewport())
	{
		Hud->AimCrosshairWidget->AddToViewport();
	}
}

void UPdHudUiRouter::HideAimCrosshair()
{
	if (APdHUD* Hud = OwnerHud.Get())
	{
		if (Hud->AimCrosshairWidget)
		{
			Hud->AimCrosshairWidget->RemoveFromParent();
		}
	}
}

void UPdHudUiRouter::OpenInfo()
{
	if (ScreenLayer)
	{
		ScreenLayer->OpenInfo();
	}
}

void UPdHudUiRouter::CloseInfo(
	const bool bSuppressCameraReturn,
	const bool bImmediate)
{
	if (ScreenLayer)
	{
		ScreenLayer->CloseInfo(bSuppressCameraReturn, bImmediate);
	}
}

void UPdHudUiRouter::ToggleInfo()
{
	if (ScreenLayer)
	{
		ScreenLayer->ToggleInfo();
	}
}

void UPdHudUiRouter::OpenPandoraTree()
{
	if (ScreenLayer)
	{
		ScreenLayer->OpenPandoraTree();
	}
}

void UPdHudUiRouter::ClosePandoraTree(
	const bool bSuppressCameraReturn,
	const bool bImmediate)
{
	if (ScreenLayer)
	{
		ScreenLayer->ClosePandoraTree(bSuppressCameraReturn, bImmediate);
	}
}

void UPdHudUiRouter::TogglePandoraTree()
{
	if (ScreenLayer)
	{
		ScreenLayer->TogglePandoraTree();
	}
}

bool UPdHudUiRouter::IsInfoClosing() const
{
	return ScreenLayer && ScreenLayer->IsInfoClosing();
}

bool UPdHudUiRouter::IsPandoraTreeClosing() const
{
	return ScreenLayer && ScreenLayer->IsPandoraTreeClosing();
}

bool UPdHudUiRouter::IsScreenLayerBlockingGameplayInput() const
{
	return ScreenLayer && ScreenLayer->IsBlockingGameplayInput();
}

void UPdHudUiRouter::RefreshTrainingRoomPause(const UUserWidget* IgnoredWidget)
{
	if (ScreenLayer)
	{
		ScreenLayer->RefreshTrainingRoomPause(IgnoredWidget);
	}
}

void UPdHudUiRouter::ScheduleTrainingRoomPause(const float DelaySeconds)
{
	if (ScreenLayer)
	{
		ScreenLayer->ScheduleTrainingRoomPause(DelaySeconds);
	}
}

void UPdHudUiRouter::ApplyActiveDefinition(UWidgetClassDefinition* NewDefinition)
{
	UWidgetClassDefinition* PreviousDefinition = ActiveDefinition;
	ActiveDefinition = NewDefinition;

	if (APdHUD* Hud = OwnerHud.Get())
	{
		Hud->WidgetClassDefinition = ActiveDefinition;
	}

	if (UUiSubsystem* UiSubsystem = ResolveUiSubsystem())
	{
		if (ActiveDefinition)
		{
			UiSubsystem->SetWidgetClassDefinition(ActiveDefinition);
		}
		else
		{
			UiSubsystem->ClearWidgetClassDefinition(PreviousDefinition);
		}
	}
}

UUiSubsystem* UPdHudUiRouter::ResolveUiSubsystem() const
{
	const APdPlayerController* Controller = ResolvePlayerController();
	const ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
}

APdPlayerController* UPdHudUiRouter::ResolvePlayerController() const
{
	const APdHUD* Hud = OwnerHud.Get();
	return Hud ? Cast<APdPlayerController>(Hud->GetOwningPlayerController()) : nullptr;
}
