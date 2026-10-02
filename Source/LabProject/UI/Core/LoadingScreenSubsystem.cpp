#include "UI/Core/LoadingScreenSubsystem.h"

#include "Character/CharacterBase.h"
#include "CommonActivatableWidget.h"
#include "Component/Experience/ExperienceManagerComponent.h"
#include "Data/ContentDataSubsystem.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Lobby/Contents/LobbyHUD.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Misc/PackageName.h"
#include "Mode/ExperienceGameState.h"
#include "Mode/PdHUD.h"
#include "Mode/PdPlayerState.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "Settings/GameSettingsSubsystem.h"
#include "ShaderPipelineCache.h"
#include "UI/Core/ConnectingPopupWidget.h"
#include "UI/Core/UiScreen.h"
#include "UI/Core/UiSubsystem.h"
#include "UI/Lobby/LobbyWidget.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LoadingScreenSubsystem)

void ULoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bIsDeinitializing = false;
	UiSubsystem = Collection.InitializeDependency<UUiSubsystem>();
	if (UiSubsystem)
	{
		WidgetContentChangedHandle = UiSubsystem->OnWidgetContentChanged.AddUObject(this, &ThisClass::RefreshLoadingScreen);
	}

	if (UGameInstance* GameInstance = GetLocalPlayer()->GetGameInstance())
	{
		PreClientTravelHandle = GameInstance->OnNotifyPreClientTravel().AddUObject(this, &ThisClass::HandlePreClientTravel);
	}
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::HandlePreLoadMap);
	SeamlessTravelHandle = FWorldDelegates::OnSeamlessTravelStart.AddUObject(this, &ThisClass::HandleSeamlessTravelStart);
	if (GEngine)
	{
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
	}
	if (FShaderPipelineCache::NumPrecompilesRemaining() > 0)
	{
		ActiveWaitReasons.Add(EWaitReason::PipelineCompile);
	}
	LoadingWorkTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::TickLoadingWork), 0.05f);
	RefreshLoadingScreen();
}

void ULoadingScreenSubsystem::Deinitialize()
{
	bIsDeinitializing = true;
	FTSTicker::RemoveTicker(LoadingWorkTickerHandle);
	LoadingWorkTickerHandle.Reset();
	if (UiSubsystem)
	{
		UiSubsystem->OnWidgetContentChanged.Remove(WidgetContentChangedHandle);
	}
	if (UGameInstance* GameInstance = GetLocalPlayer()->GetGameInstance())
	{
		GameInstance->OnNotifyPreClientTravel().Remove(PreClientTravelHandle);
	}
	FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadMapHandle);
	FWorldDelegates::OnSeamlessTravelStart.Remove(SeamlessTravelHandle);
	if (GEngine)
	{
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	}

	HideConnectingPopup();
	UiSubsystem = nullptr;
	Super::Deinitialize();
}

// UUiSubsystem이 새 컨트롤러용 화면 루트를 다시 만들므로 팝업을 내리고, 다음 갱신에서 새 루트에 다시 띄운다.
void ULoadingScreenSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);
	HideConnectingPopup();
}

APlayerController* ULoadingScreenSubsystem::GetLocalPlayerController() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
}

void ULoadingScreenSubsystem::ShowConnectingPopup(const bool bEnableCancelButton)
{
	APlayerController* PlayerController = GetLocalPlayerController();
	UWorld* World = IsValid(PlayerController) ? PlayerController->GetWorld() : nullptr;
	if (bIsDeinitializing || ActiveWaitReasons.IsEmpty() || !World || World->bIsTearingDown || !UiSubsystem)
	{
		return;
	}

	const UWidgetClassDefinition* WidgetDefinition = UiSubsystem->GetWidgetClassDefinition();
	const TSubclassOf<UConnectingPopupWidget> PopupClass =
		WidgetDefinition ? WidgetDefinition->GetConnectingPopupWidgetClass() : nullptr;
	if (!PopupClass)
	{
		return;
	}

	if (ActiveConnectingPopupWidget && !IsValid(ActiveConnectingPopupWidget))
	{
		ActiveConnectingPopupWidget = nullptr;
	}

	if (ActiveConnectingPopupWidget && ActiveConnectingPopupWidget->GetWorld() != PlayerController->GetWorld())
	{
		HideConnectingPopup();
	}

	if (!ActiveConnectingPopupWidget || ActiveConnectingPopupWidget->GetClass() != PopupClass)
	{
		HideConnectingPopup();
		ActiveConnectingPopupWidget = CreateWidget<UConnectingPopupWidget>(PlayerController, PopupClass);
	}

	if (!ActiveConnectingPopupWidget)
	{
		return;
	}

	ActiveConnectingPopupWidget->OnCanceled.RemoveDynamic(this, &ThisClass::HandleConnectingPopupCanceled);
	ActiveConnectingPopupWidget->OnCanceled.AddUniqueDynamic(this, &ThisClass::HandleConnectingPopupCanceled);
	ActiveConnectingPopupWidget->SetCancelButtonEnabled(bEnableCancelButton);

	if (!ConnectingScreen)
	{
		ConnectingScreen = CreateWidget<UUiScreen>(PlayerController);
		FUIInputConfig Config(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
		Config.bIgnoreMoveInput = Config.bIgnoreLookInput = true;
		ConnectingScreen->SetContent(ActiveConnectingPopupWidget, Config, EPdGameplayInputPolicy::Block, ActiveConnectingPopupWidget,
			FSimpleDelegate::CreateUObject(ActiveConnectingPopupWidget, &UConnectingPopupWidget::HandleCancelClicked));
		UiSubsystem->PushScreen(ConnectingScreen, EUiScreenLayer::Modal);
	}
}

void ULoadingScreenSubsystem::HideConnectingPopup()
{
	if (UConnectingPopupWidget* PopupWidget = ActiveConnectingPopupWidget.Get(); IsValid(PopupWidget))
	{
		PopupWidget->OnCanceled.RemoveDynamic(this, &ThisClass::HandleConnectingPopupCanceled);
		PopupWidget->OnCanceled.Clear();
		PopupWidget->RemoveFromParent();
	}

	if (ConnectingScreen)
	{
		ConnectingScreen->DeactivateWidget();
		ConnectingScreen = nullptr;
	}
	ActiveConnectingPopupWidget = nullptr;
}

void ULoadingScreenSubsystem::HandleConnectingPopupCanceled()
{
	const uint64 RequestId = CancelableSessionRequestId;
	CancelableSessionRequestId = 0;
	if (UOnlineSessionsSubsystem* Online = GetLocalPlayer()->GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>())
	{
		// Recheck the request identity: a delayed click must never cancel a later operation.
		if (RequestId && Online->GetPendingUserRequestId(GetLocalPlayer()) == RequestId
			&& Online->IsUserRequestCancelable(RequestId))
		{
			Online->CancelSessionRequest(RequestId);
		}
	}
	RefreshLoadingScreen();
}

bool ULoadingScreenSubsystem::TickLoadingWork(float)
{
	if (bIsDeinitializing)
	{
		return false;
	}
	RefreshLoadingScreen();
	return true;
}

void ULoadingScreenSubsystem::RefreshLoadingScreen()
{
	if (bIsDeinitializing)
	{
		return;
	}
	UGameInstance* GI = GetLocalPlayer()->GetGameInstance();
	const UContentDataSubsystem* Content = GI ? GI->GetSubsystem<UContentDataSubsystem>() : nullptr;
	const UGameSettingsSubsystem* Settings = GI ? GI->GetSubsystem<UGameSettingsSubsystem>() : nullptr;
	ULobbyRuntimeSubsystem* Runtime = GI ? GI->GetSubsystem<ULobbyRuntimeSubsystem>() : nullptr;
	const UOnlineSessionsSubsystem* Online = GI ? GI->GetSubsystem<UOnlineSessionsSubsystem>() : nullptr;
	const bool bFinishingContentPSO = ActiveWaitReasons.Contains(EWaitReason::PipelineCompile);
	ActiveWaitReasons.Reset();
	CancelableSessionRequestId = 0;
	if ((UiSubsystem && UiSubsystem->IsConfiguredWidgetContentPreloadPending())
		|| (Content && Content->IsSkillDataAssetsLoading()))
	{
		ActiveWaitReasons.Add(EWaitReason::StartupContent);
	}
	if (Runtime && Runtime->IsLobbyEntryContentLoading())
	{
		ActiveWaitReasons.Add(EWaitReason::LobbyEntryContent);
	}
	if (Settings && Settings->IsRuntimeContentLoading())
	{
		ActiveWaitReasons.Add(EWaitReason::StartupContent);
	}
	if (Runtime && Runtime->GetGameEntryContentPreloadResult() == ELobbyContentPreloadResult::Loading)
	{
		ActiveWaitReasons.Add(EWaitReason::GameEntryContent);
	}
	if ((bTravelPending || bGameStartPreparationPending || !ActiveWaitReasons.IsEmpty() || bFinishingContentPSO)
		&& FShaderPipelineCache::NumPrecompilesRemaining() > 0)
	{
		ActiveWaitReasons.Add(EWaitReason::PipelineCompile);
	}
	if (Online)
	{
		if (const uint64 RequestId = Online->GetPendingUserRequestId(GetLocalPlayer()))
		{
			ActiveWaitReasons.Add(EWaitReason::SessionRequest);
			if (Online->IsUserRequestCancelable(RequestId))
			{
				CancelableSessionRequestId = RequestId;
			}
		}
		if (Online->IsSessionLifecyclePending())
		{
			ActiveWaitReasons.Add(EWaitReason::SessionLifecycle);
		}
	}
	if (bGameStartPreparationPending)
	{
		ActiveWaitReasons.Add(EWaitReason::GameStartPreparation);
	}
	if (bTravelPending)
	{
		// Content completion cannot finish a travel while still in its source world.
		if (IsDestinationPresentationReady()
			&& !ActiveWaitReasons.Contains(EWaitReason::StartupContent)
			&& !ActiveWaitReasons.Contains(EWaitReason::LobbyEntryContent)
			&& !ActiveWaitReasons.Contains(EWaitReason::GameEntryContent)
			&& !ActiveWaitReasons.Contains(EWaitReason::PipelineCompile))
		{
			bTravelPending = false;
			TravelSourceWorld.Reset();
			TravelDestinationMap.Reset();
			if (Runtime && GetWorld() && !GetWorld()->GetGameState<ALobbyGameState>())
			{
				Runtime->ReleaseLobbyEntryContentPreload();
			}
		}
		else
		{
			ActiveWaitReasons.Add(EWaitReason::Travel);
		}
	}
	if (ActiveWaitReasons.IsEmpty())
	{
		HideConnectingPopup();
	}
	else
	{
		ShowConnectingPopup(CancelableSessionRequestId != 0 && !bTravelPending && !bGameStartPreparationPending);
	}
}

void ULoadingScreenSubsystem::SetGameStartPreparationPending(const bool bPending)
{
	bGameStartPreparationPending = bPending;
	RefreshLoadingScreen();
}

void ULoadingScreenSubsystem::BeginTravel(UWorld* SourceWorld, const FString& URL)
{
	if (!bTravelPending)
	{
		TravelSourceWorld = SourceWorld;
		TravelDestinationMap = URL.Left(URL.Find(TEXT("?")) == INDEX_NONE ? URL.Len() : URL.Find(TEXT("?")));
		// Remote addresses do not identify a map; destination readiness still requires a new world.
		if (!TravelDestinationMap.StartsWith(TEXT("/Game/")))
		{
			TravelDestinationMap.Reset();
		}
		bTravelPending = true;
	}
	// The host's preparation transaction now belongs to actual engine travel.
	bGameStartPreparationPending = false;
	RefreshLoadingScreen();
}

void ULoadingScreenSubsystem::HandlePreClientTravel(const FString& URL, ETravelType, bool)
{
	BeginTravel(GetWorld(), URL);
}

void ULoadingScreenSubsystem::HandlePreLoadMap(const FWorldContext& Context, const FString& MapName)
{
	if (Context.OwningGameInstance == GetLocalPlayer()->GetGameInstance())
	{
		BeginTravel(Context.World(), MapName);
	}
}

void ULoadingScreenSubsystem::HandleSeamlessTravelStart(UWorld* World, const FString& URL)
{
	if (World && World->GetGameInstance() == GetLocalPlayer()->GetGameInstance())
	{
		BeginTravel(World, URL);
	}
}

void ULoadingScreenSubsystem::AbortTravel()
{
	bTravelPending = false;
	bGameStartPreparationPending = false;
	TravelSourceWorld.Reset();
	TravelDestinationMap.Reset();
	if (ULobbyRuntimeSubsystem* Runtime = GetLocalPlayer()->GetGameInstance()->GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		Runtime->CancelGameEntryContentPreload();
	}
	RefreshLoadingScreen();
}

void ULoadingScreenSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type, const FString&)
{
	if (World && World->GetGameInstance() == GetLocalPlayer()->GetGameInstance())
	{
		AbortTravel();
	}
}

void ULoadingScreenSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver*, ENetworkFailure::Type, const FString&)
{
	if (World && World->GetGameInstance() == GetLocalPlayer()->GetGameInstance())
	{
		AbortTravel();
	}
}

bool ULoadingScreenSubsystem::IsDestinationPresentationReady() const
{
	APlayerController* PC = GetLocalPlayerController();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!World || World == TravelSourceWorld.Get() || World->bIsTearingDown
		|| !World->HasBegunPlay() || World->IsInSeamlessTravel())
	{
		return false;
	}
	if (!TravelDestinationMap.IsEmpty()
		&& !World->GetMapName().EndsWith(FPackageName::GetShortName(TravelDestinationMap)))
	{
		return false;
	}
	// A failed Experience is terminal too: leave its existing error UI accessible.
	if (const ALobbyGameState* Lobby = World->GetGameState<ALobbyGameState>())
	{
		if (Lobby->HasExperienceLoadFailed())
		{
			return true;
		}
		const ALobbyHUD* HUD = PC->GetHUD<ALobbyHUD>();
		return HUD && IsValid(HUD->GetLobbyWidget()) && HUD->GetLobbyWidget()->GetParent()
			&& Lobby->IsSelectedMapImageReady() && Lobby->GetExperienceManagerComponent()->IsExperienceLoaded();
	}
	if (const AExperienceGameState* GameState = World->GetGameState<AExperienceGameState>())
	{
		const UExperienceManagerComponent* Experience = GameState->GetExperienceManagerComponent();
		if (Experience->HasExperienceLoadFailed())
		{
			return true;
		}
		const APdHUD* HUD = PC->GetHUD<APdHUD>();
		return Experience->IsExperienceLoaded() && Cast<ACharacterBase>(PC->GetPawn())
			&& PC->GetPlayerState<APdPlayerState>() && HUD && HUD->GetPlayerHudWidget();
	}
	// Title and Room List do not require a character pawn. Their real CommonUI screen must exist.
	return UiSubsystem && UiSubsystem->HasActiveScreen(World);
}
