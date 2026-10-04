#include "UI/Lobby/LobbyWidget.h"
#include "Component/Lobby/LobbyPlayerStateComponent.h"
#include "Component/Player/PlayerMatchComponent.h"

#include "Component/Lobby/LobbyConfigurationComponent.h"
#include "AudioSlider.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/VerticalBox.h"
#include "Definition/Level/LevelDefinition.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "UI/Lobby/LobbyHUD.h"
#include "Mode/PdPlayerState.h"
#include "UI/Lobby/LobbyUserWidget.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "TimerManager.h"
#include "Input/CommonUIActionRouterBase.h"
#include "UI/Settings/AudioVolumeControl.h"
#include "UI/Core/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyWidget)

void ULobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	ApplyWidgetDefinitionSettings();
	if (const UTextBlock* WarningText = Txt_Warning)
	{
		DefaultTeamBalanceWarningText = WarningText->GetText();
	}
	ApplyLobbyInputPassthroughVisibility();
	if (!bHasDefaultSelectedMapPlayerCountColor)
	{
		if (const UTextBlock* PlayerCountText = Txt_SelectedMapPlayerCount)
		{
			DefaultSelectedMapPlayerCountColor = PlayerCountText->GetColorAndOpacity();
			bHasDefaultSelectedMapPlayerCountColor = true;
		}
	}

	AudioVolumeControl = NewObject<UAudioVolumeControl>(this);
	AudioVolumeControl->Initialize(this, AudioVolumeSlider_, Btn_Sound);

	if (Btn_Close)
	{
		Btn_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}

	if (Btn_GameStart)
	{
		Btn_GameStart->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGameStartClicked);
	}

	if (UButton* EnterButton = Btn_Enter)
	{
		EnterButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleEnterClicked);
	}

	if (Btn_Invite)
	{
		Btn_Invite->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleInviteClicked);
	}

	if (UButton* PreviousMapButton = Btn_MapPrevious)
	{
		PreviousMapButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMapPreviousClicked);
	}

	if (UButton* NextMapButton = Btn_MapNext)
	{
		NextMapButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMapNextClicked);
	}

	ApplyReplicatedGameStartState();
	SetInfo();
}

FReply ULobbyWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (UCommonUIActionRouterBase::FindOwningActivatable(GetCachedWidget(), GetOwningLocalPlayer()) || InKeyEvent.GetKey() != EKeys::Escape)
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
	return CloseGameSettings() || IsGameStartPending();
}

void ULobbyWidget::NativeDestruct()
{
	if (UTextBlock* WarningText = Txt_Warning)
	{
		WarningText->SetText(DefaultTeamBalanceWarningText);
	}

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

	if (Btn_GameStart)
	{
		Btn_GameStart->OnClicked.RemoveDynamic(this, &ThisClass::HandleGameStartClicked);
	}

	if (UButton* EnterButton = Btn_Enter)
	{
		EnterButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleEnterClicked);
	}

	if (Btn_Invite)
	{
		Btn_Invite->OnClicked.RemoveDynamic(this, &ThisClass::HandleInviteClicked);
	}

	if (UButton* PreviousMapButton = Btn_MapPrevious)
	{
		PreviousMapButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleMapPreviousClicked);
	}

	if (UButton* NextMapButton = Btn_MapNext)
	{
		NextMapButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleMapNextClicked);
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
	if (RebuildPlayerSlots())
	{
		RefreshUI();
	}
}

// 슬롯 생성만 담당한다. 실패 시 화면 갱신을 다시 호출하지 않는다.
bool ULobbyWidget::RebuildPlayerSlots()
{
	if (!UserList)
	{
		return false;
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

	if (!LobbyUserWidgetClass || !GetOwningPlayer())
	{
		return false;
	}

	const int32 SlotCount = GetMaxLobbySlotsForUI();
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		ULobbyUserWidget* ChildWidget = CreateWidget<ULobbyUserWidget>(GetOwningPlayer(), LobbyUserWidgetClass);
		if (!ChildWidget)
		{
			return false;
		}

		UserList->AddChildToVerticalBox(ChildWidget);
		LobbyUsers.Add(ChildWidget);
	}

	return true;
}

void ULobbyWidget::RefreshUI()
{
	if (UserList && LobbyUsers.Num() != GetMaxLobbySlotsForUI())
	{
		if (!RebuildPlayerSlots())
		{
			return;
		}
	}

	const TArray<APdPlayerState*> LobbyPlayerStates = GetLobbyPlayerStates();
	const bool bStartPending = IsGameStartPending();
	if (!bStartPending)
	{
		SetLobbyInteractionsLocked(false);
	}

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

	if (UTextBlock* WarningText = Txt_Warning)
	{
		const ALobbyGameState* GameState = GetLobbyGameState();
		const bool bExperienceFailed = GameState && GameState->HasExperienceLoadFailed();
		const bool bShowWarning = !bStartPending && (bExperienceFailed || (LobbyPlayerStates.Num() > 0 && !bTeamsBalanced));
		WarningText->SetText(bExperienceFailed
			? MenuText(TEXT("Lobby.LoadFailed"))
			: MenuText(TEXT("Lobby.TeamWarning")));
		SetTeamBalanceWarningVisibility(bShowWarning ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	ApplyReplicatedGameStartState();
}

TArray<APdPlayerState*> ULobbyWidget::GetLobbyPlayerStates() const
{
	TArray<APdPlayerState*> LobbyPlayerStates;

	const AGameStateBase* GameState = UGameplayStatics::GetGameState(this);
	if (!GameState)
	{
		return LobbyPlayerStates;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (APdPlayerState* LobbyPlayerState = Cast<APdPlayerState>(PlayerState))
		{
			if (!LobbyPlayerState->GetLobbyPlayerStateComponent()->IsLeavingLobby())
			{
				LobbyPlayerStates.Add(LobbyPlayerState);
			}
		}
	}

	LobbyPlayerStates.Sort([](const APdPlayerState& Left, const APdPlayerState& Right)
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
	DestroySessionCompleteHandle = OnlineSessionsSubsystem->OnDestroySessionComplete.AddUObject(this,
		&ThisClass::HandleDestroySessionForClose);

	if (UWorld* World = GetWorld(); World && World->GetAuthGameMode())
	{
		SendRemoteClientsToTitleMap(TitleMapName);
		World->GetTimerManager().SetTimer(CloseDestroyTimerHandle, this, &ThisClass::DestroySessionForClose, 0.25f,
			false);
		return;
	}

	OnlineSessionsSubsystem->DestroySession();
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
	RemoveFromParent();

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (ALobbyHUD* LobbyHUD = PlayerController->GetHUD<ALobbyHUD>())
		{
			LobbyHUD->NotifyLobbyWidgetClosed();
		}
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
	const ULevelDefinition* Definition = ULevelDefinition::ResolveDefaultDefinition();
	return Definition ? Definition->GetTitleTravelMapName() : FString();
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
		return LobbyGameMode->GetLobbyConfigurationComponent()->GetSelectedLobbyMapOption(OutMapOption);
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
	const int32 OptionCount = LobbyGameMode ? LobbyGameMode->GetLobbyConfigurationComponent()->GetLobbyMapOptionCount() : 0;
	const int32 ActivePlayers = GetLobbyPlayerStates().Num();
	const int32 MaxPlayers = bHasMapOption ? FMath::Max(MapOption.MaxPlayerCount, 1) : GetMaxLobbySlotsForUI();
	const bool bSelectedMapCapacityExceeded = bHasMapOption && ActivePlayers > MaxPlayers;

	if (UTextBlock* MapNameText = Txt_SelectedMapName)
	{
		const FText MapName = bHasMapOption && !MapOption.DisplayName.IsEmpty()
			? MapOption.DisplayName
			: FText::FromString((bHasMapOption ? MapOption.MapKey : NAME_None).ToString());
		MapNameText->SetText(MenuTextOrFallback(FName(*(TEXT("Map.") + MapOption.MapKey.ToString())), MapName));
	}

	if (UTextBlock* PlayerCountText = Txt_SelectedMapPlayerCount)
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

	if (UImage* ThumbnailImage = Img_SelectedMapThumbnail)
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

	if (UButton* PreviousMapButton = Btn_MapPrevious)
	{
		PreviousMapButton->SetVisibility(bIsServer ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		PreviousMapButton->SetIsEnabled(bIsServer && OptionCount > 1 && !IsGameStartPending());
	}

	if (UButton* NextMapButton = Btn_MapNext)
	{
		NextMapButton->SetVisibility(bIsServer ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		NextMapButton->SetIsEnabled(bIsServer && OptionCount > 1 && !IsGameStartPending());
	}
}

bool ULobbyWidget::AreLobbyTeamsBalancedForUI(const TArray<APdPlayerState*>& LobbyPlayerStates) const
{
	TMap<int32, int32> TeamCounts;
	int32 ActivePlayerCount = 0;
	bool bHasUnassignedTeam = false;
	for (const APdPlayerState* LobbyPlayerState : LobbyPlayerStates)
	{
		if (!LobbyPlayerState || LobbyPlayerState->GetLobbyPlayerStateComponent()->IsLeavingLobby())
		{
			continue;
		}

		++ActivePlayerCount;
		const int32 TeamColorIndex = LobbyPlayerState->GetPlayerMatchComponent()->GetMatchTeamColorIndex();
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
	if (UWidget* RootWidget = GameStartCountdownRoot)
	{
		RootWidget->SetVisibility(InVisibility);
	}

	if (UTextBlock* CountdownText = Txt_GameStartCountdown)
	{
		CountdownText->SetVisibility(InVisibility);
	}
}

void ULobbyWidget::SetTeamBalanceWarningVisibility(const ESlateVisibility InVisibility)
{
	if (UWidget* WarningRoot = TeamBalanceWarningRoot)
	{
		WarningRoot->SetVisibility(InVisibility);
	}

	if (UTextBlock* WarningText = Txt_Warning)
	{
		WarningText->SetVisibility(InVisibility);

		if (UPanelWidget* ParentWidget = WarningText->GetParent())
		{
			const FString ParentName = ParentWidget->GetName();
			if (!WidgetTree || ParentWidget != WidgetTree->RootWidget
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
	if (UTextBlock* CountdownText = Txt_GameStartCountdown)
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
			World->GetTimerManager().SetTimer(GameStartCountdownTickHandle, this,
				&ThisClass::HandleGameStartCountdownTick, FMath::Max(GameStartCountdownTickInterval, 0.01f), true);
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
			if (!Widget || (!Widget->IsA<UButton>() && !Widget->IsA<UComboBoxString>()))
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
}

FText ULobbyWidget::FormatGameStartCountdownText() const
{
	const float RemainingSeconds = GetGameStartRemainingSeconds();
	if (!IsGameStartPending() || RemainingSeconds <= 0.0f)
	{
		return MenuTextOrFallback(TEXT("Lobby.Start"), GameStartCountdownFinishedText);
	}

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Seconds"), FText::AsNumber(FMath::CeilToInt(RemainingSeconds)));
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
	return LobbyGameState ? LobbyGameState->GetGameStartRemainingSeconds() : 0.0f;
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

void ULobbyWidget::OnMenuLanguageChanged()
{
	RefreshUI();
	RefreshSelectedMapUI();
	RefreshGameStartCountdownUI();
}
