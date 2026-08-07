#include "UI/HudUiRouter.h"
#include "UI/HudMenuLayer.h"
#include "UI/HudScoreboardLayer.h"
#include "UI/HudScreenLayer.h"

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

#include UE_INLINE_GENERATED_CPP_BY_NAME(HudUiRouter)

DEFINE_LOG_CATEGORY_STATIC(LogHudUiRouter, Log, All);

void UHudUiRouter::Initialize(APdHUD* InOwnerHud)
{
	if (!IsValid(InOwnerHud) || OwnerHud.Get() == InOwnerHud)
	{
		return;
	}

	Shutdown();
	OwnerHud = InOwnerHud;

	MenuLayer = NewObject<UHudMenuLayer>(this);
	MenuLayer->Initialize(InOwnerHud, this);

	ScreenLayer = NewObject<UHudScreenLayer>(this);
	ScreenLayer->Initialize(InOwnerHud, this);

	ScoreboardLayer = NewObject<UHudScoreboardLayer>(this);
	ScoreboardLayer->Initialize(InOwnerHud, this);
}

void UHudUiRouter::Shutdown()
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

bool UHudUiRouter::AddDefinitionRequest(UWidgetClassDefinition* Definition)
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

bool UHudUiRouter::RemoveDefinitionRequest(const UWidgetClassDefinition* Definition)
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

void UHudUiRouter::EnsureCoreLayers()
{
	APdHUD* Hud = OwnerHud.Get();
	APdPlayerController* Controller = ResolvePlayerController();
	UWidgetClassDefinition* Definition = ActiveDefinition;
	if (!Hud || !Controller || !Controller->IsLocalController() || !Definition)
	{
		return;
	}

	if (bEnsuringCoreLayers)
	{
		return;
	}

	TGuardValue<bool> EnsureCoreLayersGuard(bEnsuringCoreLayers, true);

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

	if (Hud->CachedSelectPandoraUI)
	{
		if (UInfoUiPresenter* Presenter = Hud->GetInfoUiPresenter())
		{
			Hud->CachedSelectPandoraUI->OnSelected.RemoveDynamic(
				Presenter,
				&UInfoUiPresenter::HandleSelectedPandoraDirection);
			Hud->CachedSelectPandoraUI->OnSelected.AddUniqueDynamic(
				Presenter,
				&UInfoUiPresenter::HandleSelectedPandoraDirection);
		}
	}
}

void UHudUiRouter::EnsureInfoLayers()
{
	APdHUD* Hud = OwnerHud.Get();
	APdPlayerController* Controller = ResolvePlayerController();
	UWidgetClassDefinition* Definition = ActiveDefinition;
	if (!Hud || !Controller || !Controller->IsLocalController() || !Definition
		|| bEnsuringInfoLayers)
	{
		return;
	}

	TGuardValue<bool> EnsureInfoLayersGuard(bEnsuringInfoLayers, true);
	if (!Hud->CachedInfoUI)
	{
		if (const TSubclassOf<UInfoWidget> WidgetClass = Definition->GetInfoWidgetClass())
		{
			Hud->CachedInfoUI = CreateWidget<UInfoWidget>(Controller, WidgetClass);
		}
	}
	if (!Hud->CachedPandoraTreeUI)
	{
		if (const TSubclassOf<UPandoraTreeWidget> WidgetClass =
			Definition->GetPandoraTreeWidgetClass())
		{
			Hud->CachedPandoraTreeUI =
				CreateWidget<UPandoraTreeWidget>(Controller, WidgetClass);
			if (Hud->CachedPandoraTreeUI)
			{
				Hud->CachedPandoraTreeUI->SetInputModeManagedExternally(true);
			}
		}
	}

	if (Hud->CachedInfoUI)
	{
		Hud->CachedInfoUI->OnClickedSettingButton.RemoveDynamic(
			Hud,
			&APdHUD::OpenSettingsMenu);
		Hud->CachedInfoUI->OnClickedSettingButton.AddUniqueDynamic(
			Hud,
			&APdHUD::OpenSettingsMenu);
		if (UInfoUiPresenter* Presenter = Hud->GetInfoUiPresenter())
		{
			Presenter->BindInfoUi(Hud->CachedInfoUI);
			Hud->CachedInfoUI->OnClickedInfoCenterButton.RemoveDynamic(
				Presenter,
				&UInfoUiPresenter::HandleClickedInfoCenterButton);
			Hud->CachedInfoUI->OnClickedInfoCenterButton.AddUniqueDynamic(
				Presenter,
				&UInfoUiPresenter::HandleClickedInfoCenterButton);
		}

		if (URightStatusWidget* RightStatusWidget =
			Hud->CachedInfoUI->GetRightStatusWidget())
		{
			Hud->ApplyStatusViewModelToWidget(RightStatusWidget);
		}
		Hud->ApplyStatusViewModelToWidgetTree(Hud->CachedInfoUI);
		Hud->ApplyInventoryWidgetSettings();
	}
}

void UHudUiRouter::ReleaseInfoLayers()
{
	APdHUD* Hud = OwnerHud.Get();
	if (!Hud)
	{
		return;
	}
	if (Hud->CachedInfoUI)
	{
		if (UInfoUiPresenter* Presenter = Hud->CachedInfoUiPresenter)
		{
			Hud->CachedInfoUI->OnClickedInfoCenterButton.RemoveDynamic(
				Presenter,
				&UInfoUiPresenter::HandleClickedInfoCenterButton);
			Presenter->UnbindInfoUi(Hud->CachedInfoUI);
		}
		Hud->CachedInfoUI->OnClickedSettingButton.RemoveDynamic(
			Hud,
			&APdHUD::OpenSettingsMenu);
		Hud->CachedInfoUI->SetReturnCameraOnHide(true);
		Hud->CachedInfoUI->RemoveFromParent();
		Hud->CachedInfoUI = nullptr;
	}
	if (Hud->CachedPandoraTreeUI)
	{
		Hud->CachedPandoraTreeUI->SetReturnCameraOnHide(true);
		Hud->CachedPandoraTreeUI->RemoveFromParent();
		Hud->CachedPandoraTreeUI = nullptr;
	}
}

void UHudUiRouter::ResetLayers()
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
	ReleaseInfoLayers();
	if (Hud->CachedSelectPandoraUI)
	{
		if (UInfoUiPresenter* Presenter = Hud->CachedInfoUiPresenter)
		{
			Hud->CachedSelectPandoraUI->OnSelected.RemoveDynamic(
				Presenter,
				&UInfoUiPresenter::HandleSelectedPandoraDirection);
		}
		Hud->CachedSelectPandoraUI->RemoveFromParent();
		Hud->CachedSelectPandoraUI = nullptr;
	}
	if (Hud->CachedRightNotificationsUI)
	{
		Hud->CachedRightNotificationsUI->RemoveFromParent();
		Hud->CachedRightNotificationsUI = nullptr;
	}
}

void UHudUiRouter::RouteInput(
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

	FUiModalInputConfig InputConfig;
	InputConfig.RestorePolicy = EUiInputRestorePolicy::Gameplay;
	if (bPreserveGameplayInputMode)
	{
		InputConfig.InputMode = EUiInputMode::GameOnly;
		InputConfig.bApplyInputMode = false;
	}

	if (!UiSubsystem->UpdateModalInput(this, ModalInputToken, FocusWidget, InputConfig))
	{
		ModalInputToken.Invalidate();
		ModalInputToken = UiSubsystem->AcquireModalInput(this, FocusWidget, InputConfig);
	}
	if (!ModalInputToken.IsValid())
	{
		UE_LOG(LogHudUiRouter, Error, TEXT("Failed to acquire the HUD modal input route."));
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

void UHudUiRouter::ReleaseInput()
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

bool UHudUiRouter::OpenSettingsMenu()
{
	return MenuLayer && MenuLayer->Open();
}

bool UHudUiRouter::ToggleSettingsMenu()
{
	return MenuLayer && MenuLayer->Toggle();
}

bool UHudUiRouter::CloseSettingsMenu()
{
	return MenuLayer && MenuLayer->Close();
}

bool UHudUiRouter::IsSettingsMenuOpen() const
{
	return MenuLayer && MenuLayer->IsOpen();
}

UMenuPopupWidget* UHudUiRouter::GetSettingsMenuWidget() const
{
	return MenuLayer ? MenuLayer->GetWidget() : nullptr;
}

void UHudUiRouter::ShowScoreboard()
{
	if (ScoreboardLayer)
	{
		ScoreboardLayer->Show();
	}
}

void UHudUiRouter::HideScoreboard()
{
	if (ScoreboardLayer)
	{
		ScoreboardLayer->Hide();
	}
}

void UHudUiRouter::RefreshScoreboard()
{
	if (ScoreboardLayer)
	{
		ScoreboardLayer->Refresh();
	}
}

bool UHudUiRouter::IsScoreboardOpen() const
{
	return ScoreboardLayer && ScoreboardLayer->IsOpen();
}

void UHudUiRouter::ShowAimCrosshair(const FGameplayTag DesiredCrosshairWidgetTag)
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

void UHudUiRouter::HideAimCrosshair()
{
	if (APdHUD* Hud = OwnerHud.Get())
	{
		if (Hud->AimCrosshairWidget)
		{
			Hud->AimCrosshairWidget->RemoveFromParent();
		}
	}
}

void UHudUiRouter::OpenInfo()
{
	if (ScreenLayer)
	{
		ScreenLayer->OpenInfo();
	}
}

void UHudUiRouter::OpenInfo(const EInfoUiSection InitialSection)
{
	if (ScreenLayer)
	{
		ScreenLayer->OpenInfo(InitialSection);
	}
}

void UHudUiRouter::CloseInfo(
	const bool bSuppressCameraReturn,
	const bool bImmediate)
{
	if (ScreenLayer)
	{
		ScreenLayer->CloseInfo(bSuppressCameraReturn, bImmediate);
	}
}

void UHudUiRouter::ToggleInfo()
{
	if (ScreenLayer)
	{
		ScreenLayer->ToggleInfo();
	}
}

void UHudUiRouter::OpenPandoraTree()
{
	if (ScreenLayer)
	{
		ScreenLayer->OpenPandoraTree();
	}
}

void UHudUiRouter::ClosePandoraTree(
	const bool bSuppressCameraReturn,
	const bool bImmediate)
{
	if (ScreenLayer)
	{
		ScreenLayer->ClosePandoraTree(bSuppressCameraReturn, bImmediate);
	}
}

void UHudUiRouter::TogglePandoraTree()
{
	if (ScreenLayer)
	{
		ScreenLayer->TogglePandoraTree();
	}
}

bool UHudUiRouter::IsInfoClosing() const
{
	return ScreenLayer && ScreenLayer->IsInfoClosing();
}

bool UHudUiRouter::IsPandoraTreeClosing() const
{
	return ScreenLayer && ScreenLayer->IsPandoraTreeClosing();
}

bool UHudUiRouter::IsScreenLayerBlockingGameplayInput() const
{
	return ScreenLayer && ScreenLayer->IsBlockingGameplayInput();
}

void UHudUiRouter::RefreshTrainingRoomPause(const UUserWidget* IgnoredWidget)
{
	if (ScreenLayer)
	{
		ScreenLayer->RefreshTrainingRoomPause(IgnoredWidget);
	}
}

void UHudUiRouter::ScheduleTrainingRoomPause(const float DelaySeconds)
{
	if (ScreenLayer)
	{
		ScreenLayer->ScheduleTrainingRoomPause(DelaySeconds);
	}
}

void UHudUiRouter::ApplyActiveDefinition(UWidgetClassDefinition* NewDefinition)
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

UUiSubsystem* UHudUiRouter::ResolveUiSubsystem() const
{
	const APdPlayerController* Controller = ResolvePlayerController();
	const ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
}

APdPlayerController* UHudUiRouter::ResolvePlayerController() const
{
	const APdHUD* Hud = OwnerHud.Get();
	return Hud ? Cast<APdPlayerController>(Hud->GetOwningPlayerController()) : nullptr;
}
