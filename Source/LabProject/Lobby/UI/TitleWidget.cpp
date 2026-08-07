#include "Lobby/UI/TitleWidget.h"

#include "AudioSlider.h"
#include "Components/Button.h"
#include "Definition/Lobby/LobbyModeDefinition.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Lobby/UI/ConnectingPopupWidget.h"
#include "Mode/PdGameInstance.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "TimerManager.h"
#include "UI/Widget/GuideWidget.h"
#include "UI/Widget/AudioVolumeControl.h"
#include "UI/Widget/RecordWidget.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "UI/Shop/ShopWidget.h"
#include "UI/UiSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/TransBuffer.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(TitleWidget)

namespace
{
	void ResetEditorTransactionBufferIfContainsPieObjects(const TCHAR* Context)
	{
#if WITH_EDITOR
		if (GEditor && GEditor->Trans && GEditor->Trans->ContainsPieObjects())
		{
			GEditor->ResetTransaction(NSLOCTEXT(
				"TitleWidget",
				"TransactionContainedTitleUiPieObject",
				"A title UI PIE object was in the transaction buffer and had to be destroyed"));

		}
#endif
	}

	void TravelTitleToListenMap(const UObject* WorldContextObject, const FString& MapName)
	{
		if (UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr)
		{
			if (World->GetNetMode() != NM_Client && World->GetNetDriver())
			{
				World->ServerTravel(FString::Printf(TEXT("%s?listen"), *MapName));
				return;
			}
		}

		UGameplayStatics::OpenLevel(WorldContextObject, FName(*MapName), true, TEXT("listen"));
	}
}

UTitleWidget::UTitleWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTitleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyWidgetDefinitionSettings();
	ResolveWidgets();
	if (const UUiSubsystem* UiSubsystem = GetUiSubsystem();
		!UiSubsystem || !UiSubsystem->IsTravelLoadingScreenActive())
	{
		HideConnectingPopup();
	}
	EnsurePreferredSaveGameLoaded();

	AudioVolumeControl = NewObject<UAudioVolumeControl>(this);
	AudioVolumeControl->Initialize(this, AudioVolumeSlider_, Btn_Sound);

	if (Btn_RoomList)
	{
		Btn_RoomList->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRoomListClicked);
	}

	if (Btn_QuickMatch)
	{
		Btn_QuickMatch->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleQuickMatchClicked);
	}

	if (Btn_TrainingMode)
	{
		Btn_TrainingMode->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTrainingModeClicked);
	}

	if (Btn_PandoraShop)
	{
		Btn_PandoraShop->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePandoraShopClicked);
	}

	if (Btn_Guide)
	{
		Btn_Guide->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGuideClicked);
	}

	if (Btn_Record)
	{
		Btn_Record->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRecordClicked);
	}

	if (Btn_Exit)
	{
		Btn_Exit->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleExitClicked);
	}

	if (Btn_Tutorial)
	{
		Btn_Tutorial->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGuideClicked);
	}

	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr)
	{
		if (!QuickMatchRequestCompleteHandle.IsValid())
		{
			QuickMatchRequestCompleteHandle =
				OnlineSessionsSubsystem->OnQuickMatchRequestComplete.AddUObject(
					this,
					&ThisClass::HandleQuickMatchRequestComplete);
		}
	}
}

void UTitleWidget::NativeDestruct()
{
	if (AudioVolumeControl)
	{
		AudioVolumeControl->Shutdown();
		AudioVolumeControl = nullptr;
	}

	if (Btn_RoomList)
	{
		Btn_RoomList->OnClicked.RemoveDynamic(this, &ThisClass::HandleRoomListClicked);
	}

	if (Btn_QuickMatch)
	{
		Btn_QuickMatch->OnClicked.RemoveDynamic(this, &ThisClass::HandleQuickMatchClicked);
	}

	if (Btn_TrainingMode)
	{
		Btn_TrainingMode->OnClicked.RemoveDynamic(this, &ThisClass::HandleTrainingModeClicked);
	}

	if (Btn_PandoraShop)
	{
		Btn_PandoraShop->OnClicked.RemoveDynamic(this, &ThisClass::HandlePandoraShopClicked);
	}

	if (Btn_Guide)
	{
		Btn_Guide->OnClicked.RemoveDynamic(this, &ThisClass::HandleGuideClicked);
	}

	if (Btn_Record)
	{
		Btn_Record->OnClicked.RemoveDynamic(this, &ThisClass::HandleRecordClicked);
	}

	if (Btn_Exit)
	{
		Btn_Exit->OnClicked.RemoveDynamic(this, &ThisClass::HandleExitClicked);
	}

	if (Btn_Tutorial)
	{
		Btn_Tutorial->OnClicked.RemoveDynamic(this, &ThisClass::HandleGuideClicked);
	}

	ClearQuickMatchDelegates();
	if (ShopWidget)
	{
		ShopWidget->RemoveFromParent();
		ShopWidget = nullptr;
	}
	if (IsValid(GuideWidget))
	{
		GuideWidget->RemoveFromParent();
	}
	GuideWidget = nullptr;
	if (IsValid(RecordWidget))
	{
		UnbindRecordCloseButton();
		RecordWidget->RemoveFromParent();
	}
	RecordWidget = nullptr;

	Super::NativeDestruct();
}

void UTitleWidget::ApplyWidgetDefinitionSettings()
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const FTitleAuxiliaryWidgetSettings& Settings = WidgetDefinition->GetTitleAuxiliaryWidgetSettings();
		if (const TSubclassOf<UShopWidget> ResolvedShopWidgetClass = WidgetDefinition->GetShopWidgetClass())
		{
			ShopWidgetClass = ResolvedShopWidgetClass;
		}
		if (const TSubclassOf<UGuideWidget> ResolvedGuideWidgetClass = WidgetDefinition->GetGuideWidgetClass())
		{
			GuideWidgetClass = TSubclassOf<UUserWidget>(ResolvedGuideWidgetClass.Get());
		}
		if (const TSubclassOf<URecordWidget> ResolvedRecordWidgetClass = WidgetDefinition->GetRecordWidgetClass())
		{
			RecordWidgetClass = TSubclassOf<UUserWidget>(ResolvedRecordWidgetClass.Get());
		}
		QuickMatchMaxSearchResults = FMath::Max(Settings.QuickMatchMaxSearchResults, 1);
		QuickMatchMaxPublicConnections = FMath::Max(Settings.QuickMatchMaxPublicConnections, 1);
		QuickMatchRoomName = Settings.QuickMatchRoomName;
		bQuickMatchLAN = Settings.bQuickMatchLAN;
		bQuickMatchUseLobbies = Settings.bQuickMatchUseLobbies;
	}
}

void UTitleWidget::HandleRoomListClicked()
{
	OpenRoomList();
}

void UTitleWidget::HandleQuickMatchClicked()
{
	StartQuickMatch();
}

void UTitleWidget::HandleTrainingModeClicked()
{
	OpenTrainingRoom();
}

void UTitleWidget::HandlePandoraShopClicked()
{

	OpenShop();
}

void UTitleWidget::HandleGuideClicked()
{
	OpenGuide();
}

void UTitleWidget::HandleRecordClicked()
{
	OpenRecord();
}

void UTitleWidget::HandleRecordCloseClicked()
{
	if (!IsValid(RecordWidget))
	{
		RecordWidget = nullptr;
		return;
	}

	UnbindRecordCloseButton();
	RecordWidget->RemoveFromParent();
}

void UTitleWidget::HandleExitClicked()
{

	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, true);
}

void UTitleWidget::HandleQuickMatchCancel()
{
	bQuickMatchStartPending = false;
	CancelQuickMatchStartTimer();
	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr)
	{
		const uint64 RequestId = ActiveQuickMatchRequestId;
		ActiveQuickMatchRequestId = 0;
		if (RequestId != 0)
		{
			OnlineSessionsSubsystem->CancelSessionRequest(RequestId);
		}
	}

	SetQuickMatchEnabled(true);
	HideQuickMatchLoadingScreen();

}

FString UTitleWidget::GetResolvedLobbyTravelMapName() const
{
	const ULobbyModeDefinition* Definition =
		ULobbyModeDefinition::ResolveDefaultDefinition();
	return Definition ? Definition->GetLobbyTravelMapName() : FString();
}

FString UTitleWidget::GetResolvedRoomTravelMapName() const
{
	const ULobbyModeDefinition* Definition =
		ULobbyModeDefinition::ResolveDefaultDefinition();
	return Definition ? Definition->GetRoomTravelMapName() : FString();
}

FString UTitleWidget::GetResolvedTrainingRoomTravelMapName() const
{
	const UMatchRuleDefinition* Definition =
		UMatchRuleDefinition::ResolveDefaultDefinition();
	return Definition
		? Definition->GetTrainingRoomTravelMapName()
		: FString();
}

void UTitleWidget::OpenRoomList()
{
	const FString RoomMapName = GetResolvedRoomTravelMapName();
	if (RoomMapName.IsEmpty())
	{

		return;
	}

ResetEditorTransactionBufferIfContainsPieObjects(TEXT("OpenRoomList"));
	UGameplayStatics::OpenLevel(this, FName(*RoomMapName));
}

void UTitleWidget::OpenTrainingRoom()
{
	const FString TrainingRoomMapName = GetResolvedTrainingRoomTravelMapName();
	if (TrainingRoomMapName.IsEmpty())
	{

		return;
	}

	if (UUiSubsystem* UiSubsystem = GetUiSubsystem())
	{
		UiSubsystem->ShowTravelLoadingScreen();
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (ULobbyRuntimeSubsystem* LobbyRuntimeSubsystem =
			GameInstance->GetSubsystem<ULobbyRuntimeSubsystem>())
		{
			LobbyRuntimeSubsystem->BeginGameEntryContentPreload();
		}
	}
	ResetEditorTransactionBufferIfContainsPieObjects(TEXT("OpenTrainingRoom"));
	UGameplayStatics::OpenLevel(this, FName(*TrainingRoomMapName));
}

void UTitleWidget::OpenShop()
{
	EnsurePreferredSaveGameLoaded();
	ResetEditorTransactionBufferIfContainsPieObjects(TEXT("OpenShop"));

const TSubclassOf<UShopWidget> ResolvedShopWidgetClass = ResolveShopWidgetClass();
	if (!ResolvedShopWidgetClass)
	{

		return;
	}

	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{

		return;
	}

	if (!ShopWidget || ShopWidget->GetClass() != ResolvedShopWidgetClass.Get())
	{
		if (ShopWidget)
		{
			ShopWidget->RemoveFromParent();
			ShopWidget = nullptr;
		}

		ShopWidget = CreateWidget<UShopWidget>(PlayerController, ResolvedShopWidgetClass);
	}

	if (!ShopWidget)
	{

		return;
	}

	ShopWidget->RefreshUI();
	ShopWidget->SetVisibility(ESlateVisibility::Visible);
	if (!ShopWidget->IsInViewport())
	{
		ShopWidget->AddToViewport(50);
	}

}

void UTitleWidget::OpenGuide()
{
	ResetEditorTransactionBufferIfContainsPieObjects(TEXT("OpenGuide"));

	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{

		return;
	}

	const TSubclassOf<UUserWidget> ResolvedGuideWidgetClass = ResolveGuideWidgetClass();
	if (!ResolvedGuideWidgetClass)
	{

		return;
	}

	if (GuideWidget && (!IsValid(GuideWidget) || GuideWidget->GetWorld() != GetWorld()))
	{
		GuideWidget = nullptr;
	}

	if (!GuideWidget || GuideWidget->GetClass() != ResolvedGuideWidgetClass.Get())
	{
		if (GuideWidget)
		{
			GuideWidget->RemoveFromParent();
			GuideWidget = nullptr;
		}

		GuideWidget = CreateWidget<UUserWidget>(PlayerController, ResolvedGuideWidgetClass);
	}

	if (!GuideWidget)
	{

		return;
	}

	if (UGuideWidget* TypedGuideWidget = Cast<UGuideWidget>(GuideWidget))
	{
		TypedGuideWidget->RefreshGuide();
	}

	GuideWidget->SetVisibility(ESlateVisibility::Visible);
	if (!GuideWidget->IsInViewport())
	{
		GuideWidget->AddToViewport(60);
	}
}

void UTitleWidget::OpenRecord()
{
	ResetEditorTransactionBufferIfContainsPieObjects(TEXT("OpenRecord"));

	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{

		return;
	}

	const TSubclassOf<UUserWidget> ResolvedRecordWidgetClass = ResolveRecordWidgetClass();
	if (!ResolvedRecordWidgetClass)
	{

		return;
	}

	if (RecordWidget && (!IsValid(RecordWidget) || RecordWidget->GetWorld() != GetWorld()))
	{
		RecordWidget = nullptr;
	}

	if (!RecordWidget || RecordWidget->GetClass() != ResolvedRecordWidgetClass.Get())
	{
		if (RecordWidget)
		{
			UnbindRecordCloseButton();
			RecordWidget->RemoveFromParent();
			RecordWidget = nullptr;
		}

		RecordWidget = CreateWidget<UUserWidget>(PlayerController, ResolvedRecordWidgetClass);
	}

	if (!RecordWidget)
	{

		return;
	}

	RecordWidget->SetVisibility(ESlateVisibility::Visible);
	if (!RecordWidget->IsInViewport())
	{
		RecordWidget->AddToViewport(60);
	}

	if (URecordWidget* TypedRecordWidget = Cast<URecordWidget>(RecordWidget))
	{
		TypedRecordWidget->RefreshRecords();
	}
	else
	{
		BindRecordCloseButton();
	}
}

void UTitleWidget::BindRecordCloseButton()
{
	UButton* RecordCloseButton = FindRecordCloseButton();
	if (!RecordCloseButton)
	{

		return;
	}

	RecordCloseButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleRecordCloseClicked);
	RecordCloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRecordCloseClicked);
}

void UTitleWidget::UnbindRecordCloseButton()
{
	if (UButton* RecordCloseButton = FindRecordCloseButton())
	{
		RecordCloseButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleRecordCloseClicked);
	}
}

UButton* UTitleWidget::FindRecordCloseButton() const
{
	return IsValid(RecordWidget) ? Cast<UButton>(RecordWidget->GetWidgetFromName(TEXT("Btn_Close"))) : nullptr;
}

FString UTitleWidget::ResolveTitleSavePlayerId() const
{
	UPdGameInstance* PdGameInstance = GetGameInstance<UPdGameInstance>();
	if (!PdGameInstance)
	{
		return FString();
	}

	FString PlayerId = PdGameInstance->GetPreferredSavePlayerId();
	PlayerId.TrimStartAndEndInline();
	if (!PlayerId.IsEmpty())
	{
		return PlayerId;
	}

	const APlayerController* PlayerController = GetOwningPlayer();
	const APlayerState* PlayerState = PlayerController ? PlayerController->PlayerState : nullptr;
	PlayerId = PdGameInstance->ResolveSavePlayerId(PlayerController, PlayerState);
	PlayerId.TrimStartAndEndInline();
	if (!PlayerId.IsEmpty())
	{
		PdGameInstance->SetPreferredSavePlayerId(PlayerId);
	}

	return PlayerId;
}

void UTitleWidget::ResolveWidgets()
{
	if (!Btn_RoomList)
	{
		Btn_RoomList = Cast<UButton>(GetWidgetFromName(TEXT("Btn_RoomList")));
	}

	if (!Btn_QuickMatch)
	{
		Btn_QuickMatch = Cast<UButton>(GetWidgetFromName(TEXT("Btn_QuickMatch")));
	}

	if (!Btn_TrainingMode)
	{
		Btn_TrainingMode = Cast<UButton>(GetWidgetFromName(TEXT("Btn_TrainingMode")));
	}

	if (!Btn_PandoraShop)
	{
		Btn_PandoraShop = Cast<UButton>(GetWidgetFromName(TEXT("Btn_PandoraShop")));
	}

	if (!Btn_Guide)
	{
		Btn_Guide = Cast<UButton>(GetWidgetFromName(TEXT("Btn_Guide")));
	}

	if (!Btn_Record)
	{
		Btn_Record = Cast<UButton>(GetWidgetFromName(TEXT("Btn_Record")));
	}

	if (!Btn_Exit)
	{
		Btn_Exit = Cast<UButton>(GetWidgetFromName(TEXT("Btn_Exit")));
	}

	if (!Btn_Tutorial)
	{
		Btn_Tutorial = Cast<UButton>(GetWidgetFromName(TEXT("Btn_Tutorial")));
	}

	if (!AudioVolumeSlider_)
	{
		AudioVolumeSlider_ = Cast<UAudioVolumeSlider>(GetWidgetFromName(TEXT("AudioVolumeSlider_")));
	}

	if (!Btn_Sound)
	{
		Btn_Sound = Cast<UButton>(GetWidgetFromName(TEXT("Btn_Sound")));
	}
}

TSubclassOf<UShopWidget> UTitleWidget::ResolveShopWidgetClass() const
{
	if (ShopWidgetClass)
	{
		return ShopWidgetClass;
	}

	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		return WidgetDefinition->GetShopWidgetClass();
	}

	return nullptr;
}

TSubclassOf<UUserWidget> UTitleWidget::ResolveGuideWidgetClass() const
{
	if (GuideWidgetClass)
	{
		return GuideWidgetClass;
	}

	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		return TSubclassOf<UUserWidget>(WidgetDefinition->GetGuideWidgetClass().Get());
	}

	return nullptr;
}

TSubclassOf<UUserWidget> UTitleWidget::ResolveRecordWidgetClass() const
{
	if (RecordWidgetClass)
	{
		return RecordWidgetClass;
	}

	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		return TSubclassOf<UUserWidget>(WidgetDefinition->GetRecordWidgetClass().Get());
	}

	return nullptr;
}

void UTitleWidget::EnsurePreferredSaveGameLoaded() const
{
	UPdGameInstance* PdGameInstance = GetGameInstance<UPdGameInstance>();
	if (!PdGameInstance)
	{
		return;
	}

	const FString PlayerId = ResolveTitleSavePlayerId();
	if (PlayerId.IsEmpty())
	{
		return;
	}

	PdGameInstance->LoadGame(PlayerId);

}

void UTitleWidget::StartQuickMatch()
{
	if (bQuickMatchStartPending || ActiveQuickMatchRequestId != 0)
	{
		return;
	}

	SetQuickMatchEnabled(false);
	bQuickMatchStartPending = true;
	TryStartQuickMatchAfterLoadingScreen();
}

void UTitleWidget::TryStartQuickMatchAfterLoadingScreen()
{
	if (!bQuickMatchStartPending)
	{
		return;
	}

	UConnectingPopupWidget* LoadingScreen = ShowQuickMatchLoadingScreen();
	UWorld* World = GetWorld();
	if (!World)
	{
		bQuickMatchStartPending = false;
		SetQuickMatchEnabled(true);
		HideQuickMatchLoadingScreen();
		return;
	}

	CancelQuickMatchStartTimer();
	if (!IsValid(LoadingScreen) || !LoadingScreen->IsInViewport())
	{
		World->GetTimerManager().SetTimer(
			QuickMatchStartTimerHandle,
			this,
			&ThisClass::TryStartQuickMatchAfterLoadingScreen,
			0.05f,
			false);
		return;
	}

	// Let Slate paint the loading screen once before session discovery begins.
	QuickMatchStartTimerHandle = World->GetTimerManager().SetTimerForNextTick(
		this,
		&ThisClass::BeginQuickMatchRequest);
}

void UTitleWidget::BeginQuickMatchRequest()
{
	CancelQuickMatchStartTimer();
	if (!bQuickMatchStartPending)
	{
		return;
	}

	bQuickMatchStartPending = false;
	UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr;
	if (!OnlineSessionsSubsystem)
	{
		SetQuickMatchEnabled(true);
		HideQuickMatchLoadingScreen();
		return;
	}

	if (!QuickMatchRequestCompleteHandle.IsValid())
	{
		QuickMatchRequestCompleteHandle =
			OnlineSessionsSubsystem->OnQuickMatchRequestComplete.AddUObject(
				this,
				&ThisClass::HandleQuickMatchRequestComplete);
	}

	const uint64 RequestId = OnlineSessionsSubsystem->BeginQuickMatch(
		GetOwningLocalPlayer(),
		QuickMatchMaxSearchResults,
		QuickMatchMaxPublicConnections,
		QuickMatchRoomName,
		TEXT("Lobby"),
		bQuickMatchLAN,
		bQuickMatchUseLobbies);
	if (RequestId == 0)
	{
		SetQuickMatchEnabled(true);
		HideQuickMatchLoadingScreen();
	}
	else
	{
		ActiveQuickMatchRequestId = RequestId;
	}
}

void UTitleWidget::CancelQuickMatchStartTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(QuickMatchStartTimerHandle);
	}
	QuickMatchStartTimerHandle.Invalidate();
}

void UTitleWidget::OpenLobbyAsListenServer() const
{
	const FString LobbyMapName = GetResolvedLobbyTravelMapName();
	if (LobbyMapName.IsEmpty())
	{

		return;
	}

	if (UUiSubsystem* UiSubsystem = GetUiSubsystem())
	{
		UiSubsystem->ShowLobbyEntryLoadingScreen();
	}

	ResetEditorTransactionBufferIfContainsPieObjects(TEXT("OpenLobbyAsListenServer"));
	TravelTitleToListenMap(this, LobbyMapName);
}

void UTitleWidget::SetQuickMatchEnabled(const bool bEnabled) const
{
	if (Btn_QuickMatch)
	{
		Btn_QuickMatch->SetIsEnabled(bEnabled);
	}
}

void UTitleWidget::ClearQuickMatchDelegates()
{
	bQuickMatchStartPending = false;
	CancelQuickMatchStartTimer();
	UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr;
	if (!OnlineSessionsSubsystem)
	{
		QuickMatchRequestCompleteHandle.Reset();
		ActiveQuickMatchRequestId = 0;
		return;
	}

	if (QuickMatchRequestCompleteHandle.IsValid())
	{
		OnlineSessionsSubsystem->OnQuickMatchRequestComplete.Remove(
			QuickMatchRequestCompleteHandle);
		QuickMatchRequestCompleteHandle.Reset();
	}

	const uint64 RequestId = ActiveQuickMatchRequestId;
	ActiveQuickMatchRequestId = 0;
	if (RequestId != 0)
	{
		OnlineSessionsSubsystem->CancelSessionRequest(RequestId);
	}
}

UUiSubsystem* UTitleWidget::GetUiSubsystem() const
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
}

UConnectingPopupWidget* UTitleWidget::ShowQuickMatchLoadingScreen()
{
	UUiSubsystem* UiSubsystem = GetUiSubsystem();
	if (!UiSubsystem)
	{
		return nullptr;
	}

	UConnectingPopupWidget* PopupWidget =
		UiSubsystem->ShowLobbyEntryLoadingScreen(true);
	if (PopupWidget)
	{
		PopupWidget->OnCanceled.RemoveDynamic(this, &ThisClass::HandleQuickMatchCancel);
		PopupWidget->OnCanceled.AddUniqueDynamic(this, &ThisClass::HandleQuickMatchCancel);
	}

	return PopupWidget;
}

void UTitleWidget::HideQuickMatchLoadingScreen() const
{
	if (UUiSubsystem* UiSubsystem = GetUiSubsystem())
	{
		UiSubsystem->HideTravelLoadingScreen();
	}
}

void UTitleWidget::HideConnectingPopup() const
{
	if (UUiSubsystem* UiSubsystem = GetUiSubsystem())
	{
		UiSubsystem->HideConnectingPopup();
	}
}

void UTitleWidget::HandleQuickMatchRequestComplete(
	const uint64 RequestId,
	const bool bWasSuccessful,
	const bool bCreatedRoom)
{
	if (RequestId == 0 || RequestId != ActiveQuickMatchRequestId)
	{
		return;
	}

	ActiveQuickMatchRequestId = 0;

	if (bWasSuccessful)
	{
		if (bCreatedRoom)
		{
			OpenLobbyAsListenServer();
		}
		return;
	}

	SetQuickMatchEnabled(true);
	HideQuickMatchLoadingScreen();
}
