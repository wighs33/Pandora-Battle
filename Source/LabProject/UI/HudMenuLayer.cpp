#include "UI/HudMenuLayer.h"
#include "UI/HudUiRouter.h"

#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
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
#include "UI/Presenter/InfoUiPresenter.h"
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

#include UE_INLINE_GENERATED_CPP_BY_NAME(HudMenuLayer)

DEFINE_LOG_CATEGORY_STATIC(LogHudUiRouter, Log, All);

void UHudMenuLayer::Initialize(APdHUD* InOwnerHud, UHudUiRouter* InRouter)
{
	OwnerHud = InOwnerHud;
	Router = InRouter;
}

bool UHudMenuLayer::Open()
{
	APdHUD* Hud = OwnerHud.Get();
	UHudUiRouter* UiRouter = Router.Get();
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
		UE_LOG(LogHudUiRouter, Error, TEXT("Settings menu layer has no configured widget class."));
		return false;
	}

	ActiveWidget = CreateWidget<UMenuPopupWidget>(Controller, MenuPopupClass);
	if (!ActiveWidget)
	{
		UE_LOG(LogHudUiRouter, Error, TEXT("Failed to create settings menu widget."));
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

bool UHudMenuLayer::Toggle()
{
	return IsOpen() ? Close() : Open();
}

bool UHudMenuLayer::Close()
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

bool UHudMenuLayer::IsOpen() const
{
	return IsValid(ActiveWidget) && ActiveWidget->IsInViewport();
}

void UHudMenuLayer::Shutdown()
{
	if (IsValid(ActiveWidget))
	{
		ActiveWidget->OnMenuClosed.RemoveDynamic(this, &ThisClass::HandleMenuClosed);
		ActiveWidget->RemoveFromParent();
	}
	ActiveWidget = nullptr;
}

void UHudMenuLayer::HandleMenuClosed(UMenuPopupWidget* ClosedWidget)
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
