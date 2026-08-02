#include "Lobby/Contents/LobbyHUD.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Lobby/UI/LobbyWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyHUD)

void ALobbyHUD::BeginPlay()
{
	Super::BeginPlay();
	CreateLobbyUI();
}

void ALobbyHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (LobbyWidget)
	{
		LobbyWidget->RemoveFromParent();
		LobbyWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

bool ALobbyHUD::IsPlayerHudSuppressedByUi() const
{
	return Super::IsPlayerHudSuppressedByUi()
		|| (LobbyWidget && LobbyWidget->IsInViewport());
}

bool ALobbyHUD::HandleEscapeInput()
{
	if (LobbyWidget && LobbyWidget->IsInViewport())
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

	if (!LobbyWidget->IsInViewport())
	{
		LobbyWidget->AddToViewport();
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
	if (!LobbyWidget)
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
	ApplyLobbyWidgetInputMode();
}

void ALobbyHUD::NotifyLobbyWidgetClosed()
{
	RefreshPlayerHudVisibility();
	RestoreGameInputModeIfPossible();
}

void ALobbyHUD::ApplyLobbyWidgetInputMode()
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(PlayerController, LobbyWidget, EMouseLockMode::DoNotLock, false);
	PlayerController->bShowMouseCursor = true;
	PlayerController->bEnableClickEvents = true;
	PlayerController->bEnableMouseOverEvents = true;
	if (LobbyWidget)
	{
		LobbyWidget->SetIsFocusable(true);
		LobbyWidget->SetUserFocus(PlayerController);
		LobbyWidget->SetFocus();
	}
}

void ALobbyHUD::RestoreGameInputModeIfPossible()
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController() || Super::IsGameplayInputBlockedByUi())
	{
		return;
	}

	UWidgetBlueprintLibrary::SetInputMode_GameOnly(PlayerController, false);
	PlayerController->bShowMouseCursor = false;
	PlayerController->bEnableClickEvents = false;
	PlayerController->bEnableMouseOverEvents = false;
}
