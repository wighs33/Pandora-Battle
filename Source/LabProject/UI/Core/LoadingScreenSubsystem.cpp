#include "UI/Core/LoadingScreenSubsystem.h"

#include "Character/CharacterBase.h"
#include "CommonActivatableWidget.h"
#include "Component/Experience/ExperienceManagerComponent.h"
#include "Component/Player/ControllerPresentationComponent.h"
#include "Data/ContentDataSubsystem.h"
#include "UI/Core/WidgetClassDefinition.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "UI/Lobby/LobbyHUD.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Misc/PackageName.h"
#include "Mode/ExperienceGameState.h"
#include "UI/HUD/PdHUD.h"
#include "Mode/PdPlayerController.h"
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
		ConnectingScreen = UUiScreen::CreateBlocking(PlayerController, ActiveConnectingPopupWidget, ActiveConnectingPopupWidget,
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
		// 요청 번호를 다시 확인한다. 늦게 들어온 클릭이 뒤에 시작된 작업을 취소하면 안 된다.
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
	const bool bGameStartPreparationPending = Runtime && Runtime->IsGameStartPreparationPending();
	const bool bFinishingContentPSO = ActiveWaitReasons.Contains(EWaitReason::PipelineCompile);
	ActiveWaitReasons.Reset();
	CancelableSessionRequestId = 0;
	if ((UiSubsystem && UiSubsystem->IsConfiguredWidgetContentPreloadPending())
		|| (Content && Content->IsSkillDataAssetsLoading()))
	{
		ActiveWaitReasons.Add(EWaitReason::StartupContent);
	}
	if ((Runtime && Runtime->IsLobbyEntryContentLoading()) || (UiSubsystem && UiSubsystem->IsLobbyContentLoading()))
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
		// 출발 월드에 있는 동안에는 콘텐츠 로딩이 끝나도 맵 이동을 끝내지 않는다.
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
	// 맵 이동 직후의 훈련실 일시정지는 컨트롤러가 정하므로 대기 여부만 알려 준다.
	if (const APdPlayerController* PdController = Cast<APdPlayerController>(GetLocalPlayerController()))
	{
		if (UControllerPresentationComponent* Presentation = PdController->GetControllerPresentationComponent())
		{
			Presentation->SetLoadingScreenWaiting(HasBlockingWait());
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

void ULoadingScreenSubsystem::BeginTravel(UWorld* SourceWorld, const FString& URL)
{
	if (!bTravelPending)
	{
		TravelSourceWorld = SourceWorld;
		TravelDestinationMap = URL.Left(URL.Find(TEXT("?")) == INDEX_NONE ? URL.Len() : URL.Find(TEXT("?")));
		// 원격 주소로는 맵을 알 수 없으므로, 도착 준비는 여전히 새 월드가 생겨야 끝난다.
		if (!TravelDestinationMap.StartsWith(TEXT("/Game/")))
		{
			TravelDestinationMap.Reset();
		}
		bTravelPending = true;
	}
	// 호스트가 시작한 준비 작업은 이제 엔진의 실제 맵 이동이 이어받는다.
	if (ULobbyRuntimeSubsystem* Runtime = GetLocalPlayer()->GetGameInstance()->GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		Runtime->SetGameStartPreparationPending(false);
	}
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
	TravelSourceWorld.Reset();
	TravelDestinationMap.Reset();
	if (ULobbyRuntimeSubsystem* Runtime = GetLocalPlayer()->GetGameInstance()->GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		Runtime->SetGameStartPreparationPending(false);
		Runtime->ReleaseGameEntryContentPreload();
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
	// Experience 로드 실패도 끝난 상태로 본다. 이미 떠 있는 오류 UI를 쓸 수 있게 둔다.
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
	// 타이틀과 방 목록은 캐릭터 Pawn이 필요 없고, 실제 CommonUI 화면만 있으면 된다.
	return UiSubsystem && UiSubsystem->HasActiveScreen(World);
}
