#include "Lobby/Contents/LobbyHUD.h"
#include "UI/UiSubsystem.h"
#include "UI/UiScreen.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Lobby/UI/LobbyWidget.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyHUD)

void ALobbyHUD::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		World->GameStateSetEvent.AddUObject(this, &ThisClass::HandleGameStateSet);
		HandleGameStateSet(World->GetGameState());
	}
	CreateLobbyUI();
}

void ALobbyHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LobbyUIRefreshTimerHandle);
		World->GameStateSetEvent.RemoveAll(this);
	}
	if (ALobbyGameState* GameState = ObservedLobbyGameState.Get())
	{
		GameState->OnLobbyStateChanged.RemoveAll(this);
	}
	ObservedLobbyGameState.Reset();

	if (LobbyWidget)
	{
		LobbyWidget->RemoveFromParent();
		LobbyWidget = nullptr;
	}

    if (LobbyScreen)
    {
        LobbyScreen->DeactivateWidget();
        LobbyScreen->RemoveFromParent();
        LobbyScreen = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}

bool ALobbyHUD::IsPlayerHudSuppressedByUi() const
{
	return Super::IsPlayerHudSuppressedByUi()
		|| (LobbyScreen && LobbyScreen->IsActivated());
}

bool ALobbyHUD::HandleEscapeInput()
{
	if (LobbyScreen && LobbyScreen->IsActivated())
	{
		if (LobbyWidget->CloseTopmostUiForEscape())
		{
			return true;
		}

		LobbyWidget->RemoveFromParent();
		NotifyLobbyWidgetClosed();
		return true;
	}

	return Super::HandleEscapeInput();
}

ULobbyWidget* ALobbyHUD::CreateLobbyUI()
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return nullptr;
	}

	// A pending-kill UObject can remain non-null until GC. Treat it as absent so the same
	// input that requested the lobby can create and display a valid widget immediately.
	if (!IsValid(LobbyWidget))
	{
		LobbyWidget = nullptr;
	}

	if (!LobbyWidget && LobbyWidgetClass)
	{
		LobbyWidget = CreateWidget<ULobbyWidget>(PlayerController, LobbyWidgetClass);
	}

	if (!LobbyWidget)
	{
		return nullptr;
	}

    if (!LobbyScreen)
    {
        LobbyScreen = CreateWidget<UUiScreen>(PlayerController);
        FUIInputConfig Config(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
        Config.bIgnoreMoveInput = Config.bIgnoreLookInput = true;
        LobbyScreen->SetContent(LobbyWidget, Config, LobbyWidget,
            FSimpleDelegate::CreateWeakLambda(this, [this]() { HandleEscapeInput(); }));
        LobbyScreen->AddToPlayerScreen();
        LobbyScreen->ActivateWidget();
    }

	LobbyWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	LobbyWidget->SetIsEnabled(true);
	LobbyWidget->SetRenderOpacity(1.0f);
	if (UWidget* RootWidget = LobbyWidget->GetRootWidget())
	{
		RootWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		RootWidget->SetIsEnabled(true);
		RootWidget->SetRenderOpacity(1.0f);
	}
	LobbyWidget->ForceLayoutPrepass();

	NotifyLobbyWidgetOpened();

	return LobbyWidget;
}

void ALobbyHUD::RefreshLobbyUI()
{
	// 예약 실행과 직접 갱신이 같은 경로를 사용하며, 남은 중복 예약은 취소한다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LobbyUIRefreshTimerHandle);
	}
	const ALobbyGameState* GameState = ObservedLobbyGameState.Get();
	if (!IsValid(LobbyWidget) || (GameState && GameState->IsGameStartPending()))
	{
		CreateLobbyUI();
	}

	if (LobbyWidget)
	{
		LobbyWidget->RefreshUI();
	}
}

void ALobbyHUD::NotifyLobbyWidgetOpened()
{
    RefreshPlayerHudVisibility();
    if (LobbyScreen) LobbyScreen->RequestRefreshFocus();
}

void ALobbyHUD::NotifyLobbyWidgetClosed()
{
    if (LobbyScreen)
    {
        LobbyScreen->DeactivateWidget();
        LobbyScreen->RemoveFromParent();
        LobbyScreen = nullptr;
    }
    UUiSubsystem::SetBaseInputMode(GetOwningPlayerController(), EUiInputMode::GameOnly);
    RefreshPlayerHudVisibility();
}

// HUD와 GameState가 어느 순서로 생성되든 현재 로비를 구독하고 최초 상태를 표시한다.
void ALobbyHUD::HandleGameStateSet(AGameStateBase* GameState)
{
	if (ALobbyGameState* PreviousGameState = ObservedLobbyGameState.Get())
	{
		PreviousGameState->OnLobbyStateChanged.RemoveAll(this);
	}
	ObservedLobbyGameState = Cast<ALobbyGameState>(GameState);
	if (ALobbyGameState* LobbyGameState = ObservedLobbyGameState.Get())
	{
		LobbyGameState->OnLobbyStateChanged.AddUObject(this, &ThisClass::RequestLobbyUIRefresh);
	}
	RequestLobbyUIRefresh();
}

// 같은 프레임에 도착한 목록·이름·팀 복제를 한 번의 화면 갱신으로 모은다.
void ALobbyHUD::RequestLobbyUIRefresh()
{
	if (UWorld* World = GetWorld(); World && !LobbyUIRefreshTimerHandle.IsValid())
	{
		LobbyUIRefreshTimerHandle = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RefreshLobbyUI);
	}
}
