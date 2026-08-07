#include "Lobby/UI/LobbyWidget.h"

#include "AudioSlider.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/VerticalBox.h"
#include "Definition/Lobby/LobbyModeDefinition.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Lobby/Contents/LobbyHUD.h"
#include "Lobby/Contents/LobbyPlayerState.h"
#include "Lobby/UI/ConnectingPopupWidget.h"
#include "Lobby/UI/GameConfigWidget.h"
#include "Lobby/UI/LobbyUserWidget.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "TimerManager.h"
#include "UI/UiSubsystem.h"
#include "UI/Widget/AudioVolumeControl.h"
#include "Definition/UI/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyWidget)

DEFINE_LOG_CATEGORY_STATIC(LogLobbyWidget, Log, All);

void ULobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	SetFocus();

	ApplyWidgetDefinitionSettings();
	ApplyLobbyInputPassthroughVisibility();
	if (!bHasDefaultSelectedMapPlayerCountColor)
	{
		if (const UTextBlock* PlayerCountText = FindSelectedMapPlayerCountText())
		{
			DefaultSelectedMapPlayerCountColor = PlayerCountText->GetColorAndOpacity();
			bHasDefaultSelectedMapPlayerCountColor = true;
		}
	}

	if (!AudioVolumeSlider_)
	{
		AudioVolumeSlider_ = Cast<UAudioVolumeSlider>(GetWidgetFromName(TEXT("AudioVolumeSlider_")));
	}
	if (!Btn_Sound)
	{
		Btn_Sound = Cast<UButton>(GetWidgetFromName(TEXT("Btn_Sound")));
	}
	AudioVolumeControl = NewObject<UAudioVolumeControl>(this);
	AudioVolumeControl->Initialize(this, AudioVolumeSlider_, Btn_Sound);

	if (Btn_Close)
	{
		Btn_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}

	if (Btn_GameConfig)
	{
		Btn_GameConfig->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGameConfigClicked);
	}

	if (Btn_GameStart)
	{
		Btn_GameStart->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGameStartClicked);
	}

	if (UButton* EnterButton = FindEnterButton())
	{
		EnterButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleEnterClicked);
	}

	if (Btn_Invite)
	{
		Btn_Invite->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleInviteClicked);
	}

	if (UButton* PreviousMapButton = FindMapPreviousButton())
	{
		PreviousMapButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMapPreviousClicked);
	}

	if (UButton* NextMapButton = FindMapNextButton())
	{
		NextMapButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMapNextClicked);
	}

	if (UUiSubsystem* UiSubsystem = GetUiSubsystem();
		UiSubsystem && UiSubsystem->IsTravelLoadingScreenActive())
	{
		UiSubsystem->ShowTravelLoadingScreen();
	}
	else
	{
		HideConnectingPopup();
	}
	ApplyReplicatedGameStartState();
	SetInfo();
}

FReply ULobbyWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() != EKeys::Escape)
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}

	if (IsGameStartPending())
	{
		return FReply::Handled();
	}

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (APdHUD* Hud = PlayerController->GetHUD<APdHUD>())
		{
			Hud->HandleEscapeInput();
			return FReply::Handled();
		}
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool ULobbyWidget::CloseTopmostUiForEscape()
{
	if (IsGameStartPending())
	{
		return true;
	}

	if (!ActiveGameConfigWidget || !ActiveGameConfigWidget->IsInViewport())
	{
		return false;
	}

	ActiveGameConfigWidget->SaveConfig();
	ActiveGameConfigWidget->RemoveFromParent();

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (ALobbyHUD* LobbyHUD = PlayerController->GetHUD<ALobbyHUD>())
		{
			LobbyHUD->NotifyLobbyWidgetOpened();
		}
		SetUserFocus(PlayerController);
		SetFocus();
	}

	return true;
}

void ULobbyWidget::NativeDestruct()
{
	if (AudioVolumeControl)
	{
		AudioVolumeControl->Shutdown();
		AudioVolumeControl = nullptr;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CloseDestroyTimerHandle);
		World->GetTimerManager().ClearTimer(GameStartCountdownTickHandle);
	}

	if (Btn_Close)
	{
		Btn_Close->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
	}

	if (Btn_GameConfig)
	{
		Btn_GameConfig->OnClicked.RemoveDynamic(this, &ThisClass::HandleGameConfigClicked);
	}

	if (Btn_GameStart)
	{
		Btn_GameStart->OnClicked.RemoveDynamic(this, &ThisClass::HandleGameStartClicked);
	}

	if (UButton* EnterButton = FindEnterButton())
	{
		EnterButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleEnterClicked);
	}

	if (Btn_Invite)
	{
		Btn_Invite->OnClicked.RemoveDynamic(this, &ThisClass::HandleInviteClicked);
	}

	if (UButton* PreviousMapButton = FindMapPreviousButton())
	{
		PreviousMapButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleMapPreviousClicked);
	}

	if (UButton* NextMapButton = FindMapNextButton())
	{
		NextMapButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleMapNextClicked);
	}

	if (ActiveGameConfigWidget)
	{
		ActiveGameConfigWidget->RemoveFromParent();
		ActiveGameConfigWidget = nullptr;
	}

	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr)
	{
		if (DestroySessionCompleteHandle.IsValid())
		{
			OnlineSessionsSubsystem->OnDestroySessionComplete.Remove(DestroySessionCompleteHandle);
			DestroySessionCompleteHandle.Reset();
		}
	}

	Super::NativeDestruct();
}

void ULobbyWidget::ApplyWidgetDefinitionSettings()
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const FLobbyWidgetSettings& Settings = WidgetDefinition->GetLobbyWidgetSettings();
		if (const TSubclassOf<ULobbyUserWidget> ResolvedLobbyUserWidgetClass =
			WidgetDefinition->GetLobbyUserWidgetClass())
		{
			LobbyUserWidgetClass = ResolvedLobbyUserWidgetClass;
		}
		if (const TSubclassOf<UGameConfigWidget> ResolvedGameConfigWidgetClass =
			WidgetDefinition->GetGameConfigWidgetClass())
		{
			GameConfigWidgetClass = ResolvedGameConfigWidgetClass;
		}
		MaxLobbySlots = FMath::Max(Settings.MaxLobbySlots, 1);
		GameStartCountdownFormatText = Settings.GameStartCountdownFormatText;
		GameStartCountdownFinishedText = Settings.GameStartCountdownFinishedText;
		GameStartCountdownTickInterval = FMath::Max(Settings.GameStartCountdownTickInterval, 0.01f);
		if (!Settings.TeamBalanceWarningText.IsEmpty())
		{
			TeamBalanceWarningText = Settings.TeamBalanceWarningText;
		}
	}
}

void ULobbyWidget::ApplyLobbyInputPassthroughVisibility()
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (UWidget* RootWidget = GetRootWidget())
	{
		RootWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void ULobbyWidget::SetInfo()
{
	if (!UserList)
	{

		return;
	}

	UserList->ClearChildren();
	LobbyUsers.Reset();

	if (!LobbyUserWidgetClass)
	{
		if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
		{
			LobbyUserWidgetClass = WidgetDefinition->GetLobbyUserWidgetClass();
		}
	}

	for (int32 Index = 0; Index < GetMaxLobbySlotsForUI(); ++Index)
	{
		if (!LobbyUserWidgetClass)
		{
			break;
		}

		ULobbyUserWidget* ChildWidget = CreateWidget<ULobbyUserWidget>(GetOwningPlayer(), LobbyUserWidgetClass);
		if (!ChildWidget)
		{
			continue;
		}

		UserList->AddChildToVerticalBox(ChildWidget);
		LobbyUsers.Add(ChildWidget);
	}

	RefreshUI();
}

void ULobbyWidget::RefreshUI()
{
	if (UserList && LobbyUsers.Num() != GetMaxLobbySlotsForUI())
	{
		SetInfo();
		return;
	}

	const TArray<ALobbyPlayerState*> LobbyPlayerStates = GetLobbyPlayerStates();
	const bool bStartPending = IsGameStartPending();
	if (!bStartPending)
	{
		SetLobbyInteractionsLocked(false);
	}
	RefreshSelectedMapUI();

	for (int32 Index = 0; Index < LobbyUsers.Num(); ++Index)
	{
		ULobbyUserWidget* LobbyUserWidget = LobbyUsers[Index];
		if (!LobbyUserWidget)
		{
			continue;
		}

		if (LobbyPlayerStates.IsValidIndex(Index))
		{
			LobbyUserWidget->SetVisibility(ESlateVisibility::Visible);
			LobbyUserWidget->SetInfo(LobbyPlayerStates[Index]);
		}
		else
		{
			LobbyUserWidget->SetInfo(nullptr);
			LobbyUserWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	const bool bIsServer = GetWorld() && GetWorld()->GetAuthGameMode() != nullptr;
	const ALobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALobbyGameMode>() : nullptr;
	const bool bTeamsBalanced = AreLobbyTeamsBalancedForUI(LobbyPlayerStates);
	const IOnlineExternalUIPtr ExternalUI = GetWorld() ? Online::GetExternalUIInterface(GetWorld()) : nullptr;
	const bool bCanInvite = GetOwningPlayer() && GetOwningPlayer()->IsLocalController() && ExternalUI.IsValid();
	if (Btn_GameConfig)
	{
		Btn_GameConfig->SetVisibility(bIsServer ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Btn_GameStart)
	{
		const bool bShowGameStartButton = bIsServer && !bStartPending;
		Btn_GameStart->SetVisibility(bShowGameStartButton ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Btn_GameStart->SetIsEnabled(bShowGameStartButton && LobbyGameMode && LobbyGameMode->CanHostStartGame());
	}

	if (Btn_Invite)
	{
		Btn_Invite->SetVisibility(bCanInvite ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Btn_Invite->SetIsEnabled(bCanInvite);
	}

	if (UTextBlock* WarningText = FindTeamBalanceWarningText())
	{
		const bool bShowWarning = !bStartPending && LobbyPlayerStates.Num() > 0 && !bTeamsBalanced;
		if (!TeamBalanceWarningText.IsEmpty())
		{
			WarningText->SetText(TeamBalanceWarningText);
		}
		SetTeamBalanceWarningVisibility(bShowWarning ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	ApplyReplicatedGameStartState();
}

void ULobbyWidget::StartGameCountdown(const float DelaySeconds)
{
	static_cast<void>(DelaySeconds);

	if (!IsInViewport())
	{
		AddToViewport();
	}
	ApplyLobbyInputPassthroughVisibility();

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (ALobbyHUD* LobbyHUD = PlayerController->GetHUD<ALobbyHUD>())
		{
			LobbyHUD->NotifyLobbyWidgetOpened();
		}

		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(PlayerController, this, EMouseLockMode::DoNotLock, false);
		PlayerController->bShowMouseCursor = true;
		PlayerController->bEnableClickEvents = true;
		PlayerController->bEnableMouseOverEvents = true;
		SetUserFocus(PlayerController);
		SetFocus();
	}

	RefreshUI();
}

void ULobbyWidget::HideGameCountdown()
{
	RefreshUI();
}

TArray<ALobbyPlayerState*> ULobbyWidget::GetLobbyPlayerStates() const
{
	TArray<ALobbyPlayerState*> LobbyPlayerStates;

	const AGameStateBase* GameState = UGameplayStatics::GetGameState(this);
	if (!GameState)
	{
		return LobbyPlayerStates;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (ALobbyPlayerState* LobbyPlayerState = Cast<ALobbyPlayerState>(PlayerState))
		{
			if (!LobbyPlayerState->IsLeavingLobby())
			{
				LobbyPlayerStates.Add(LobbyPlayerState);
			}
		}
	}

	LobbyPlayerStates.Sort([](const ALobbyPlayerState& Left, const ALobbyPlayerState& Right)
	{
		return Left.GetPlayerId() < Right.GetPlayerId();
	});

	return LobbyPlayerStates;
}

void ULobbyWidget::HandleCloseClicked()
{
	const FString TitleMapName = GetResolvedTitleTravelMapName();
	if (TitleMapName.IsEmpty())
	{

		return;
	}
	ShowConnectingPopup(false);

	UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr;
	if (!OnlineSessionsSubsystem)
	{
		TravelToTitleMap();
		return;
	}

	if (DestroySessionCompleteHandle.IsValid())
	{
		OnlineSessionsSubsystem->OnDestroySessionComplete.Remove(DestroySessionCompleteHandle);
		DestroySessionCompleteHandle.Reset();
	}

	bPendingCloseAfterDestroy = true;
	DestroySessionCompleteHandle = OnlineSessionsSubsystem->OnDestroySessionComplete.AddUObject(
		this,
		&ThisClass::HandleDestroySessionForClose);

	if (UWorld* World = GetWorld(); World && World->GetAuthGameMode())
	{
		SendRemoteClientsToTitleMap(TitleMapName);
		World->GetTimerManager().SetTimer(
			CloseDestroyTimerHandle,
			this,
			&ThisClass::DestroySessionForClose,
			0.25f,
			false);
		return;
	}

	OnlineSessionsSubsystem->DestroySession();
}

void ULobbyWidget::HandleGameConfigClicked()
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode())
	{

		return;
	}

	if (!GameConfigWidgetClass)
	{
		if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
		{
			GameConfigWidgetClass = WidgetDefinition->GetGameConfigWidgetClass();
		}
	}

	if (!GameConfigWidgetClass)
	{
		UE_LOG(LogLobbyWidget, Error, TEXT("[Lobby] GameConfigWidgetClass is not configured in widget or DA_Widget."));
		return;
	}

	if (!ActiveGameConfigWidget)
	{
		ActiveGameConfigWidget = CreateWidget<UGameConfigWidget>(GetOwningPlayer(), GameConfigWidgetClass);
	}

	if (!ActiveGameConfigWidget)
	{
		UE_LOG(LogLobbyWidget, Error, TEXT("[Lobby] failed to create game config popup. class=%s"), *GetPathNameSafe(GameConfigWidgetClass));
		return;
	}

	ActiveGameConfigWidget->RemoveFromParent();
	ActiveGameConfigWidget->AddToViewport(100);
	ActiveGameConfigWidget->SetVisibility(ESlateVisibility::Visible);
	ActiveGameConfigWidget->SetIsEnabled(true);
	ActiveGameConfigWidget->SetRenderOpacity(1.0f);

	if (UWidget* RootWidget = ActiveGameConfigWidget->GetRootWidget())
	{
		RootWidget->SetVisibility(ESlateVisibility::Visible);
		RootWidget->SetIsEnabled(true);
		RootWidget->SetRenderOpacity(1.0f);
	}

	ActiveGameConfigWidget->ForceLayoutPrepass();

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(PlayerController, ActiveGameConfigWidget, EMouseLockMode::DoNotLock, false);
		PlayerController->bShowMouseCursor = true;
		PlayerController->bEnableClickEvents = true;
		PlayerController->bEnableMouseOverEvents = true;
	}

	const UWidget* RootWidget = ActiveGameConfigWidget->GetRootWidget();

}

void ULobbyWidget::HandleGameStartClicked()
{
	UWorld* World = GetWorld();
	ALobbyGameMode* LobbyGameMode = World ? World->GetAuthGameMode<ALobbyGameMode>() : nullptr;
	if (!LobbyGameMode)
	{

		return;
	}

	if (Btn_GameStart)
	{
		Btn_GameStart->SetIsEnabled(false);
	}

	LobbyGameMode->TryStartGame();
}

void ULobbyWidget::HandleEnterClicked()
{
	if (ActiveGameConfigWidget)
	{
		ActiveGameConfigWidget->RemoveFromParent();
		ActiveGameConfigWidget = nullptr;
	}

	RemoveFromParent();

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (ALobbyHUD* LobbyHUD = PlayerController->GetHUD<ALobbyHUD>())
		{
			LobbyHUD->NotifyLobbyWidgetClosed();
		}

		UWidgetBlueprintLibrary::SetInputMode_GameOnly(PlayerController, false);
		PlayerController->bShowMouseCursor = false;
		PlayerController->bEnableClickEvents = false;
		PlayerController->bEnableMouseOverEvents = false;
	}
}

void ULobbyWidget::HandleInviteClicked()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	IOnlineExternalUIPtr ExternalUI = Online::GetExternalUIInterface(World);
	if (!ExternalUI.IsValid())
	{

		return;
	}

	const bool bShown = ExternalUI->ShowInviteUI(0, NAME_GameSession);
	if (!bShown)
	{

		return;
	}

}

void ULobbyWidget::HandleMapPreviousClicked()
{
	ALobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALobbyGameMode>() : nullptr;
	if (!LobbyGameMode)
	{
		return;
	}

	LobbyGameMode->SelectLobbyMapByOffset(-1);
	RefreshUI();
}

void ULobbyWidget::HandleMapNextClicked()
{
	ALobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALobbyGameMode>() : nullptr;
	if (!LobbyGameMode)
	{
		return;
	}

	LobbyGameMode->SelectLobbyMapByOffset(1);
	RefreshUI();
}

FString ULobbyWidget::GetResolvedTitleTravelMapName() const
{
	const ULobbyModeDefinition* Definition =
		ULobbyModeDefinition::ResolveDefaultDefinition();
	return Definition ? Definition->GetTitleTravelMapName() : FString();
}

UUiSubsystem* ULobbyWidget::GetUiSubsystem() const
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
}

UWidget* ULobbyWidget::FindGameStartCountdownRoot() const
{
	if (GameStartCountdownRoot)
	{
		return GameStartCountdownRoot;
	}

	if (!WidgetTree)
	{
		return nullptr;
	}

	if (UWidget* NamedWidget = WidgetTree->FindWidget(TEXT("GameStartCountdownRoot")))
	{
		return NamedWidget;
	}

	if (UWidget* NamedWidget = WidgetTree->FindWidget(TEXT("GameStartCountdown")))
	{
		return NamedWidget;
	}

	if (UWidget* NamedWidget = WidgetTree->FindWidget(TEXT("StartGameCountdown")))
	{
		return NamedWidget;
	}

	return nullptr;
}

UTextBlock* ULobbyWidget::FindGameStartCountdownText() const
{
	if (Txt_GameStartCountdown)
	{
		return Txt_GameStartCountdown;
	}

	if (!WidgetTree)
	{
		return nullptr;
	}

	if (UTextBlock* NamedWidget = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_GameStartCountdown"))))
	{
		return NamedWidget;
	}

	if (UTextBlock* NamedWidget = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_StartGameCountdown"))))
	{
		return NamedWidget;
	}

	if (UTextBlock* NamedWidget = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_Countdown"))))
	{
		return NamedWidget;
	}

	return nullptr;
}

UWidget* ULobbyWidget::FindTeamBalanceWarningRoot() const
{
	if (TeamBalanceWarningRoot)
	{
		return TeamBalanceWarningRoot;
	}

	if (!WidgetTree)
	{
		return nullptr;
	}

	if (UWidget* NamedWidget = WidgetTree->FindWidget(TEXT("TeamBalanceWarningRoot")))
	{
		return NamedWidget;
	}

	if (UWidget* NamedWidget = WidgetTree->FindWidget(TEXT("WarningRoot")))
	{
		return NamedWidget;
	}

	if (UWidget* NamedWidget = WidgetTree->FindWidget(TEXT("Txt_Warning")))
	{
		return NamedWidget;
	}

	return nullptr;
}

UTextBlock* ULobbyWidget::FindTeamBalanceWarningText() const
{
	if (Txt_Warning)
	{
		return Txt_Warning;
	}

	if (!WidgetTree)
	{
		return nullptr;
	}

	if (UTextBlock* NamedWidget = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_Warning"))))
	{
		return NamedWidget;
	}

	if (UTextBlock* NamedWidget = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_TeamWarning"))))
	{
		return NamedWidget;
	}

	if (UTextBlock* NamedWidget = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_TeamBalanceWarning"))))
	{
		return NamedWidget;
	}

	return nullptr;
}

UButton* ULobbyWidget::FindEnterButton() const
{
	if (Btn_Enter)
	{
		return Btn_Enter;
	}

	if (!WidgetTree)
	{
		return nullptr;
	}

	return Cast<UButton>(WidgetTree->FindWidget(TEXT("Btn_Enter")));
}

UButton* ULobbyWidget::FindMapPreviousButton() const
{
	if (Btn_MapPrevious)
	{
		return Btn_MapPrevious;
	}

	if (!WidgetTree)
	{
		return nullptr;
	}

	if (UButton* NamedWidget = Cast<UButton>(WidgetTree->FindWidget(TEXT("Btn_MapPrevious"))))
	{
		return NamedWidget;
	}

	if (UButton* NamedWidget = Cast<UButton>(WidgetTree->FindWidget(TEXT("Btn_PreviousMap"))))
	{
		return NamedWidget;
	}

	if (UButton* NamedWidget = Cast<UButton>(WidgetTree->FindWidget(TEXT("Btn_MapPrev"))))
	{
		return NamedWidget;
	}

	return nullptr;
}

UButton* ULobbyWidget::FindMapNextButton() const
{
	if (Btn_MapNext)
	{
		return Btn_MapNext;
	}

	if (!WidgetTree)
	{
		return nullptr;
	}

	if (UButton* NamedWidget = Cast<UButton>(WidgetTree->FindWidget(TEXT("Btn_MapNext"))))
	{
		return NamedWidget;
	}

	if (UButton* NamedWidget = Cast<UButton>(WidgetTree->FindWidget(TEXT("Btn_NextMap"))))
	{
		return NamedWidget;
	}

	return nullptr;
}

UTextBlock* ULobbyWidget::FindSelectedMapNameText() const
{
	if (Txt_SelectedMapName)
	{
		return Txt_SelectedMapName;
	}

	if (!WidgetTree)
	{
		return nullptr;
	}

	if (UTextBlock* NamedWidget = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_SelectedMapName"))))
	{
		return NamedWidget;
	}

	if (UTextBlock* NamedWidget = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_MapName"))))
	{
		return NamedWidget;
	}

	return nullptr;
}

UTextBlock* ULobbyWidget::FindSelectedMapPlayerCountText() const
{
	if (Txt_SelectedMapPlayerCount)
	{
		return Txt_SelectedMapPlayerCount;
	}

	if (!WidgetTree)
	{
		return nullptr;
	}

	if (UTextBlock* NamedWidget = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_SelectedMapPlayerCount"))))
	{
		return NamedWidget;
	}

	if (UTextBlock* NamedWidget = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_MapPlayerCount"))))
	{
		return NamedWidget;
	}

	return nullptr;
}

UImage* ULobbyWidget::FindSelectedMapThumbnailImage() const
{
	if (Img_SelectedMapThumbnail)
	{
		return Img_SelectedMapThumbnail;
	}

	if (!WidgetTree)
	{
		return nullptr;
	}

	if (UImage* NamedWidget = Cast<UImage>(WidgetTree->FindWidget(TEXT("Img_SelectedMapThumbnail"))))
	{
		return NamedWidget;
	}

	if (UImage* NamedWidget = Cast<UImage>(WidgetTree->FindWidget(TEXT("Img_MapThumbnail"))))
	{
		return NamedWidget;
	}

	return nullptr;
}

bool ULobbyWidget::GetSelectedMapOptionForUI(FLobbyMatchMapOption& OutMapOption) const
{
	if (const ALobbyGameState* LobbyGameState = GetWorld() ? GetWorld()->GetGameState<ALobbyGameState>() : nullptr)
	{
		if (!LobbyGameState->GetSelectedMapKey().IsNone())
		{
			OutMapOption = LobbyGameState->GetSelectedMapOption();
			return true;
		}
	}

	if (const ALobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALobbyGameMode>() : nullptr)
	{
		return LobbyGameMode->GetSelectedLobbyMapOption(OutMapOption);
	}

	return false;
}

int32 ULobbyWidget::GetMaxLobbySlotsForUI() const
{
	FLobbyMatchMapOption MapOption;
	const int32 ActivePlayers = GetLobbyPlayerStates().Num();
	if (GetSelectedMapOptionForUI(MapOption))
	{
		return FMath::Max(FMath::Max(MapOption.MaxPlayerCount, ActivePlayers), 1);
	}

	return FMath::Max(FMath::Max(MaxLobbySlots, ActivePlayers), 1);
}

void ULobbyWidget::RefreshSelectedMapUI()
{
	FLobbyMatchMapOption MapOption;
	const bool bHasMapOption = GetSelectedMapOptionForUI(MapOption);
	const bool bIsServer = GetWorld() && GetWorld()->GetAuthGameMode() != nullptr;
	const ALobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALobbyGameMode>() : nullptr;
	const int32 OptionCount = LobbyGameMode ? LobbyGameMode->GetLobbyMapOptionCount() : 0;
	const int32 ActivePlayers = GetLobbyPlayerStates().Num();
	const int32 MaxPlayers = bHasMapOption
		? FMath::Max(MapOption.MaxPlayerCount, 1)
		: GetMaxLobbySlotsForUI();
	const bool bSelectedMapCapacityExceeded = bHasMapOption && ActivePlayers > MaxPlayers;

	if (UTextBlock* MapNameText = FindSelectedMapNameText())
	{
		const FText MapName = bHasMapOption && !MapOption.DisplayName.IsEmpty()
			? MapOption.DisplayName
			: FText::FromString((bHasMapOption ? MapOption.MapKey : NAME_None).ToString());
		MapNameText->SetText(MapName);
	}

	if (UTextBlock* PlayerCountText = FindSelectedMapPlayerCountText())
	{
		PlayerCountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), ActivePlayers, MaxPlayers)));
		if (!bHasDefaultSelectedMapPlayerCountColor)
		{
			DefaultSelectedMapPlayerCountColor = PlayerCountText->GetColorAndOpacity();
			bHasDefaultSelectedMapPlayerCountColor = true;
		}
		PlayerCountText->SetColorAndOpacity(
			bSelectedMapCapacityExceeded
				? FSlateColor(FLinearColor::Red)
				: DefaultSelectedMapPlayerCountColor);
	}

	if (UImage* ThumbnailImage = FindSelectedMapThumbnailImage())
	{
		if (bHasMapOption && MapOption.Thumbnail.Get())
		{
			ThumbnailImage->SetBrushFromTexture(MapOption.Thumbnail.Get());
			ThumbnailImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ThumbnailImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (UButton* PreviousMapButton = FindMapPreviousButton())
	{
		PreviousMapButton->SetVisibility(bIsServer ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		PreviousMapButton->SetIsEnabled(bIsServer && OptionCount > 1 && !IsGameStartPending());
	}

	if (UButton* NextMapButton = FindMapNextButton())
	{
		NextMapButton->SetVisibility(bIsServer ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		NextMapButton->SetIsEnabled(bIsServer && OptionCount > 1 && !IsGameStartPending());
	}
}

bool ULobbyWidget::AreLobbyTeamsBalancedForUI(const TArray<ALobbyPlayerState*>& LobbyPlayerStates) const
{
	TMap<int32, int32> TeamCounts;
	int32 ActivePlayerCount = 0;
	bool bHasUnassignedTeam = false;
	for (const ALobbyPlayerState* LobbyPlayerState : LobbyPlayerStates)
	{
		if (!LobbyPlayerState || LobbyPlayerState->IsLeavingLobby())
		{
			continue;
		}

		++ActivePlayerCount;
		const int32 TeamColorIndex = LobbyPlayerState->GetTeamColorIndex();
		if (TeamColorIndex == INDEX_NONE)
		{
			bHasUnassignedTeam = true;
			continue;
		}

		TeamCounts.FindOrAdd(TeamColorIndex)++;
	}

	if (ActivePlayerCount == 1)
	{
		return true;
	}

	if (ActivePlayerCount < 2 || TeamCounts.Num() < 2 || bHasUnassignedTeam)
	{
		return false;
	}

	int32 ExpectedPlayersPerTeam = INDEX_NONE;
	for (const TPair<int32, int32>& TeamCountPair : TeamCounts)
	{
		if (ExpectedPlayersPerTeam == INDEX_NONE)
		{
			ExpectedPlayersPerTeam = TeamCountPair.Value;
			continue;
		}

		if (TeamCountPair.Value != ExpectedPlayersPerTeam)
		{
			return false;
		}
	}

	return ExpectedPlayersPerTeam > 0;
}

void ULobbyWidget::SetGameStartCountdownVisibility(const ESlateVisibility InVisibility)
{
	if (UWidget* RootWidget = FindGameStartCountdownRoot())
	{
		RootWidget->SetVisibility(InVisibility);
	}

	if (UTextBlock* CountdownText = FindGameStartCountdownText())
	{
		CountdownText->SetVisibility(InVisibility);
	}
}

void ULobbyWidget::SetTeamBalanceWarningVisibility(const ESlateVisibility InVisibility)
{
	if (UWidget* WarningRoot = FindTeamBalanceWarningRoot())
	{
		WarningRoot->SetVisibility(InVisibility);
	}

	if (UTextBlock* WarningText = FindTeamBalanceWarningText())
	{
		WarningText->SetVisibility(InVisibility);

		if (UPanelWidget* ParentWidget = WarningText->GetParent())
		{
			const FString ParentName = ParentWidget->GetName();
			if (!WidgetTree
				|| ParentWidget != WidgetTree->RootWidget
				|| ParentName.Contains(TEXT("Warning"), ESearchCase::IgnoreCase)
				|| ParentName.Contains(TEXT("TeamBalance"), ESearchCase::IgnoreCase))
			{
				ParentWidget->SetVisibility(InVisibility);
			}
		}
	}
}

void ULobbyWidget::RefreshGameStartCountdownUI()
{
	if (UTextBlock* CountdownText = FindGameStartCountdownText())
	{
		CountdownText->SetText(FormatGameStartCountdownText());
	}
}

void ULobbyWidget::HandleGameStartCountdownTick()
{
	if (!IsGameStartPending())
	{
		ApplyReplicatedGameStartState();
		return;
	}

	RefreshGameStartCountdownUI();
	if (GetGameStartRemainingSeconds() <= 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(GameStartCountdownTickHandle);
		}
	}
}

void ULobbyWidget::ApplyReplicatedGameStartState()
{
	const bool bStartPending = IsGameStartPending();
	const float RemainingSeconds = GetGameStartRemainingSeconds();
	if (UWorld* World = GetWorld())
	{
		if (!bStartPending || RemainingSeconds <= 0.0f)
		{
			World->GetTimerManager().ClearTimer(GameStartCountdownTickHandle);
		}
		else if (!World->GetTimerManager().IsTimerActive(GameStartCountdownTickHandle))
		{
			World->GetTimerManager().SetTimer(
				GameStartCountdownTickHandle,
				this,
				&ThisClass::HandleGameStartCountdownTick,
				FMath::Max(GameStartCountdownTickInterval, 0.01f),
				true);
		}
	}

	if (!bStartPending)
	{
		SetLobbyInteractionsLocked(false);
		SetGameStartCountdownVisibility(ESlateVisibility::Collapsed);
		RefreshGameStartCountdownUI();
		RefreshSelectedMapUI();
		return;
	}

	SetLobbyInteractionsLocked(true);
	if (Btn_GameStart)
	{
		Btn_GameStart->SetVisibility(ESlateVisibility::Collapsed);
		Btn_GameStart->SetIsEnabled(false);
	}
	SetTeamBalanceWarningVisibility(ESlateVisibility::Collapsed);
	SetGameStartCountdownVisibility(ESlateVisibility::HitTestInvisible);
	RefreshGameStartCountdownUI();
	RefreshSelectedMapUI();
}

void ULobbyWidget::SetLobbyInteractionsLocked(const bool bLocked)
{
	if (!bLocked)
	{
		for (const TPair<TWeakObjectPtr<UWidget>, bool>& WidgetState : LobbyInteractionEnabledStates)
		{
			if (UWidget* Widget = WidgetState.Key.Get())
			{
				Widget->SetIsEnabled(WidgetState.Value);
			}
		}

		LobbyInteractionEnabledStates.Reset();
		return;
	}

	const auto DisableInteractiveWidgets = [this](const UWidgetTree* TargetWidgetTree)
	{
		if (!TargetWidgetTree)
		{
			return;
		}

		TargetWidgetTree->ForEachWidgetAndDescendants([this](UWidget* Widget)
		{
			if (!Widget
				|| (!Widget->IsA<UButton>() && !Widget->IsA<UComboBoxString>()))
			{
				return;
			}

			const TWeakObjectPtr<UWidget> WidgetKey(Widget);
			if (!LobbyInteractionEnabledStates.Contains(WidgetKey))
			{
				LobbyInteractionEnabledStates.Add(WidgetKey, Widget->GetIsEnabled());
			}
			Widget->SetIsEnabled(false);
		});
	};

	DisableInteractiveWidgets(WidgetTree);
	if (ActiveGameConfigWidget)
	{
		DisableInteractiveWidgets(ActiveGameConfigWidget->WidgetTree);
	}
}

FText ULobbyWidget::FormatGameStartCountdownText() const
{
	const float RemainingSeconds = GetGameStartRemainingSeconds();
	if (!IsGameStartPending() || RemainingSeconds <= 0.0f)
	{
		return GameStartCountdownFinishedText;
	}

	FFormatNamedArguments Arguments;
	Arguments.Add(
		TEXT("Seconds"),
		FText::AsNumber(FMath::CeilToInt(RemainingSeconds)));
	return FText::Format(GameStartCountdownFormatText, Arguments);
}

const ALobbyGameState* ULobbyWidget::GetLobbyGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<ALobbyGameState>() : nullptr;
}

bool ULobbyWidget::IsGameStartPending() const
{
	const ALobbyGameState* LobbyGameState = GetLobbyGameState();
	return LobbyGameState && LobbyGameState->IsGameStartPending();
}

float ULobbyWidget::GetGameStartRemainingSeconds() const
{
	const ALobbyGameState* LobbyGameState = GetLobbyGameState();
	return LobbyGameState
		? LobbyGameState->GetGameStartRemainingSeconds()
		: 0.0f;
}

void ULobbyWidget::ShowConnectingPopup(const bool bShowCancelButton) const
{
	if (UUiSubsystem* UiSubsystem = GetUiSubsystem())
	{
		UiSubsystem->ShowConnectingPopup(bShowCancelButton);
	}
}

void ULobbyWidget::HideConnectingPopup() const
{
	if (UUiSubsystem* UiSubsystem = GetUiSubsystem())
	{
		UiSubsystem->HideConnectingPopup();
	}
}

void ULobbyWidget::DestroySessionForClose()
{
	UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr;
	if (!OnlineSessionsSubsystem)
	{
		TravelToTitleMap();
		return;
	}

	OnlineSessionsSubsystem->DestroySession();
}

void ULobbyWidget::SendRemoteClientsToTitleMap(const FString& TitleMapName) const
{
	UWorld* World = GetWorld();
	if (!World || TitleMapName.IsEmpty())
	{
		return;
	}

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		if (PlayerController && !PlayerController->IsLocalController())
		{
			PlayerController->ClientTravel(TitleMapName, TRAVEL_Absolute);
		}
	}
}

void ULobbyWidget::HandleDestroySessionForClose(const bool bWasSuccessful)
{
	static_cast<void>(bWasSuccessful);

	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr)
	{
		if (DestroySessionCompleteHandle.IsValid())
		{
			OnlineSessionsSubsystem->OnDestroySessionComplete.Remove(DestroySessionCompleteHandle);
			DestroySessionCompleteHandle.Reset();
		}
	}

	if (bPendingCloseAfterDestroy)
	{
		bPendingCloseAfterDestroy = false;
		TravelToTitleMap();
	}
}

void ULobbyWidget::TravelToTitleMap() const
{
	const FString TitleMapName = GetResolvedTitleTravelMapName();
	if (TitleMapName.IsEmpty())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World && World->GetAuthGameMode())
	{
		World->ServerTravel(TitleMapName);
		return;
	}

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->ClientTravel(TitleMapName, TRAVEL_Absolute);
	}
}
