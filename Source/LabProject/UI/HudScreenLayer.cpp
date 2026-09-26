#include "UI/HudScreenLayer.h"
#include "UI/UiScreen.h"
#include "UI/HudUiRouter.h"

#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/PdPlayer.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Match/GameResultWidget.h"
#include "Mode/PdHUD.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "TimerManager.h"
#include "UI/Presenter/InfoUiPresenter.h"
#include "UI/UiSubsystem.h"
#include "UI/WidgetContentBundleLease.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/MenuPopupWidget.h"
#include "UI/Widget/PandoraTreeWidget.h"
#include "UI/Widget/PlayerHudWidget.h"
#include "UI/Widget/RightNotificationsWidget.h"
#include "UI/Widget/SelectPandoraWidget.h"
#include "UI/Widget/TrainingRoomMenuPopupWidget.h"
#include "Definition/UI/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HudScreenLayer)

namespace
{
	constexpr float InfoUiTrainingRoomPauseDelaySeconds = 0.05f;

}

void UHudScreenLayer::Initialize(APdHUD* InOwnerHud, UHudUiRouter* InRouter)
{
	OwnerHud = InOwnerHud;
	Router = InRouter;
	PendingScreenRequest = EPendingScreenRequest::None;
	PendingInfoSection = EInfoUiSection::Profile;
}

void UHudScreenLayer::Shutdown()
{
	APdHUD* Hud = OwnerHud.Get();
	ClearInfoCloseTimer();
	ClearTrainingRoomPauseTimer();
	SetTrainingRoomPaused(false);
	PendingScreenRequest = EPendingScreenRequest::None;
	bScreenHandoffInProgress = false;

	if (Hud && Hud->CachedPandoraTreeUI)
	{
		Hud->CachedPandoraTreeUI->OnPandoraTreeClosed.RemoveDynamic(
			this,
			&ThisClass::HandlePandoraTreeClosed);
	}

	bInfoClosing = false;
	bPandoraTreeClosing = false;
	if (InfoScreen)
	{
		InfoScreen->DeactivateWidget();
		InfoScreen = nullptr;
	}
	ReleaseInfoContent();
}

void UHudScreenLayer::OpenInfo()
{
	OpenInfo(EInfoUiSection::Profile);
}

void UHudScreenLayer::OpenInfo(const EInfoUiSection InitialSection)
{
	APdHUD* Hud = OwnerHud.Get();
	UHudUiRouter* UiRouter = Router.Get();
	APdPlayerController* Controller = Hud ? Hud->GetPdController() : nullptr;
	if (!Hud || !UiRouter || !Controller)
	{
		return;
	}
	PendingInfoSection = InitialSection;
	if (!EnsureInfoContentReady(EPendingScreenRequest::Info))
	{
		return;
	}
	PendingScreenRequest = EPendingScreenRequest::None;

	ClearInfoCloseTimer();
	UiRouter->EnsureCoreLayers();
	UiRouter->EnsureInfoLayers();
	if (!Hud->CachedInfoUI)
	{
		ReleaseInfoContentIfUnused();
		return;
	}

	if (UiRouter->IsSettingsMenuOpen())
	{
		UiRouter->CloseSettingsMenu();
	}
	TGuardValue<bool> ScreenHandoffGuard(bScreenHandoffInProgress, true);
	if (IsPandoraTreeOpen())
	{
		ClosePandoraTree(true, true);
	}

	bInfoClosing = false;
	Hud->CachedInfoUI->SetReturnCameraOnHide(true);
	Hud->CachedInfoUI->OnClickedSettingButton.RemoveDynamic(Hud, &APdHUD::OpenSettingsMenu);
	Hud->CachedInfoUI->OnClickedSettingButton.AddUniqueDynamic(Hud, &APdHUD::OpenSettingsMenu);

	UInfoUiPresenter* Presenter = Hud->GetInfoUiPresenter();
	if (Presenter)
	{
		Presenter->BindInfoUi(Hud->CachedInfoUI);
		Hud->CachedInfoUI->OnClickedInfoCenterButton.RemoveDynamic(
			Presenter,
			&UInfoUiPresenter::HandleClickedInfoCenterButton);
		Hud->CachedInfoUI->OnClickedInfoCenterButton.AddUniqueDynamic(
			Presenter,
			&UInfoUiPresenter::HandleClickedInfoCenterButton);
	}

	Hud->ApplyStatusViewModelToWidgetTree(Hud->CachedInfoUI);
	Hud->ApplyInventoryWidgetSettings();

	if (!InfoScreen)
	{
		InfoScreen = CreateWidget<UUiScreen>(Controller);
		FUIInputConfig Config(ECommonInputMode::All, EMouseCaptureMode::NoCapture);
		Config.bIgnoreMoveInput = Config.bIgnoreLookInput = true;
		InfoScreen->SetContent(Hud->CachedInfoUI, Config, EPdGameplayInputPolicy::Block, Hud->CachedInfoUI,
			FSimpleDelegate::CreateWeakLambda(this, [this]()
			{
				if (IsPandoraTreeOpen()) ClosePandoraTree();
				else if (!bInfoClosing) CloseInfo();
			}));
	}
	if (!InfoScreen->IsActivated())
		Controller->GetLocalPlayer()->GetSubsystem<UUiSubsystem>()->PushScreen(InfoScreen, EUiScreenLayer::Screen);
	Hud->RefreshPlayerHudVisibility();
	Hud->CachedInfoUI->ShowInfoUi();
	ScheduleTrainingRoomPause(InfoUiTrainingRoomPauseDelaySeconds);

	if (Presenter)
	{
		Presenter->HandleOpenedInfoUi();
	}
	Hud->CachedInfoUI->FocusSection(InitialSection, false);
}

void UHudScreenLayer::CloseInfo(
	const bool bSuppressCameraReturn,
	const bool bImmediate)
{
	APdHUD* Hud = OwnerHud.Get();
	if (!Hud)
	{
		return;
	}
	if (PendingScreenRequest == EPendingScreenRequest::Info
		&& !Hud->CachedInfoUI)
	{
		PendingScreenRequest = EPendingScreenRequest::None;
		ReleaseInfoContentIfUnused();
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
		ReleaseInfoContentIfUnused();
		return;
	}

	ClearInfoCloseTimer();
	bInfoClosing = true;
	if (IsPandoraTreeOpen()) ClosePandoraTree(true, true);
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

void UHudScreenLayer::ToggleInfo()
{
	if (PendingScreenRequest == EPendingScreenRequest::Info)
	{
		PendingScreenRequest = EPendingScreenRequest::None;
		ReleaseInfoContentIfUnused();
		return;
	}
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

void UHudScreenLayer::OpenPandoraTree()
{
	APdHUD* Hud = OwnerHud.Get();
	UHudUiRouter* UiRouter = Router.Get();
	APdPlayerController* Controller = Hud ? Hud->GetPdController() : nullptr;
	if (!Hud || !UiRouter || !Controller)
	{
		return;
	}
	if (!EnsureInfoContentReady(EPendingScreenRequest::PandoraTree))
	{
		return;
	}
	PendingScreenRequest = EPendingScreenRequest::None;

	UiRouter->EnsureCoreLayers();
	UiRouter->EnsureInfoLayers();
	if (!Hud->CachedPandoraTreeUI)
	{
		ReleaseInfoContentIfUnused();
		return;
	}

	if (UiRouter->IsSettingsMenuOpen())
	{
		UiRouter->CloseSettingsMenu();
	}
	TGuardValue<bool> ScreenHandoffGuard(bScreenHandoffInProgress, true);
	if (!IsInfoOpen() || bInfoClosing) OpenInfo(EInfoUiSection::Pandora);
	if (!Hud->CachedInfoUI || !IsInfoOpen()) return;
	if (Hud->CachedInfoUI->GetFocusedSection() != EInfoUiSection::Pandora)
		Hud->CachedInfoUI->FocusSection(EInfoUiSection::Pandora, false);
	if (IsPandoraTreeOpen() && !bPandoraTreeClosing) return;

	bPandoraTreeClosing = false;
	Hud->CachedPandoraTreeUI->SetDockedInInfo(true);
	Hud->CachedPandoraTreeUI->SetReturnCameraOnHide(false);
	Hud->CachedPandoraTreeUI->OnPandoraTreeClosed.AddUniqueDynamic(this, &ThisClass::HandlePandoraTreeClosed);
	if (!Hud->CachedInfoUI->AttachPandoraTree(Hud->CachedPandoraTreeUI)) return;
	Hud->CachedPandoraTreeUI->ShowPandoraTree();
	Hud->RefreshPlayerHudVisibility();
	ScheduleTrainingRoomPause(InfoUiTrainingRoomPauseDelaySeconds);
}

void UHudScreenLayer::ClosePandoraTree(
	const bool bSuppressCameraReturn,
	const bool bImmediate)
{
	APdHUD* Hud = OwnerHud.Get();
	if (!Hud || !Hud->CachedPandoraTreeUI)
	{
		if (PendingScreenRequest == EPendingScreenRequest::PandoraTree)
		{
			PendingScreenRequest = EPendingScreenRequest::None;
			ReleaseInfoContentIfUnused();
		}
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

void UHudScreenLayer::TogglePandoraTree()
{
	if (PendingScreenRequest == EPendingScreenRequest::PandoraTree)
	{
		PendingScreenRequest = EPendingScreenRequest::None;
		ReleaseInfoContentIfUnused();
		return;
	}
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

bool UHudScreenLayer::IsInfoOpen() const
{
	return InfoScreen && InfoScreen->IsActivated();
}

bool UHudScreenLayer::IsPandoraTreeOpen() const
{
	const APdHUD* Hud = OwnerHud.Get();
	return Hud && Hud->CachedPandoraTreeUI && Hud->CachedPandoraTreeUI->IsPandoraTreeShown();
}

bool UHudScreenLayer::ShouldSuppressPlayerHud() const
{
	return (!bInfoClosing && IsInfoOpen())
		|| (!bPandoraTreeClosing && IsPandoraTreeOpen());
}

void UHudScreenLayer::RefreshTrainingRoomPause(const UUserWidget* IgnoredWidget)
{
	ClearTrainingRoomPauseTimer();
	SetTrainingRoomPaused(IsTrainingRoomPauseUiOpen(IgnoredWidget));
}

void UHudScreenLayer::ScheduleTrainingRoomPause(const float DelaySeconds)
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

void UHudScreenLayer::HandlePandoraTreeClosed(UPandoraTreeWidget* ClosedWidget)
{
	APdHUD* Hud = OwnerHud.Get();
	if (!Hud || Hud->CachedPandoraTreeUI != ClosedWidget)
	{
		return;
	}

	bPandoraTreeClosing = false;
	ClosedWidget->OnPandoraTreeClosed.RemoveDynamic(
		this,
		&ThisClass::HandlePandoraTreeClosed);
	ClosedWidget->SetReturnCameraOnHide(false);
	if (Hud->CachedInfoUI) Hud->CachedInfoUI->OnPandoraDrawerClosed();
	RefreshTrainingRoomPause();
	Hud->RefreshPlayerHudVisibility();
	ReleaseInfoContentIfUnused();
}

void UHudScreenLayer::FinishCloseInfo()
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
	if (InfoScreen)
	{
		InfoScreen->DeactivateWidget();
		InfoScreen = nullptr;
	}
	bInfoClosing = false;

	RefreshTrainingRoomPause();
	Hud->RefreshPlayerHudVisibility();
	ReleaseInfoContentIfUnused();
}

bool UHudScreenLayer::EnsureInfoContentReady(
	const EPendingScreenRequest Request)
{
	UHudUiRouter* UiRouter = Router.Get();
	UUiSubsystem* UiSubsystem = UiRouter ? UiRouter->ResolveUiSubsystem() : nullptr;
	UWidgetClassDefinition* Definition =
		UiRouter ? UiRouter->GetActiveDefinition() : nullptr;
	if (!UiSubsystem || !Definition)
	{
		return false;
	}

	PendingScreenRequest = Request;
	if (InfoContentBundleLease.IsValid()
		&& InfoContentBundleLease->GetDefinition() != Definition)
	{
		ReleaseInfoContent();
	}
	if (!InfoContentBundleLease.IsValid())
	{
		InfoContentBundleLease = UiSubsystem->AcquireWidgetContentBundle(
			Definition,
			EWidgetContentBundle::Info,
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this]()
				{
					ContinuePendingScreenOpen();
				}));
	}
	if (!InfoContentBundleLease.IsValid())
	{
		PendingScreenRequest = EPendingScreenRequest::None;
		return false;
	}
	return InfoContentBundleLease->IsReady();
}

void UHudScreenLayer::ContinuePendingScreenOpen()
{
	const EPendingScreenRequest Request = PendingScreenRequest;
	if (Request == EPendingScreenRequest::None)
	{
		return;
	}
	UHudUiRouter* UiRouter = Router.Get();
	UWidgetClassDefinition* Definition =
		UiRouter ? UiRouter->GetActiveDefinition() : nullptr;
	if (!InfoContentBundleLease.IsValid()
		|| !InfoContentBundleLease->IsReady()
		|| InfoContentBundleLease->GetDefinition() != Definition)
	{
		PendingScreenRequest = EPendingScreenRequest::None;
		ReleaseInfoContent();
		return;
	}

	if (Request == EPendingScreenRequest::Info)
	{
		OpenInfo(PendingInfoSection);
	}
	else if (Request == EPendingScreenRequest::PandoraTree)
	{
		OpenPandoraTree();
	}
}

void UHudScreenLayer::ReleaseInfoContentIfUnused()
{
	if (bScreenHandoffInProgress
		|| PendingScreenRequest != EPendingScreenRequest::None
		|| bInfoClosing
		|| bPandoraTreeClosing
		|| IsInfoOpen()
		|| IsPandoraTreeOpen())
	{
		return;
	}
	ReleaseInfoContent();
}

void UHudScreenLayer::ReleaseInfoContent()
{
	APdHUD* Hud = OwnerHud.Get();
	if (Hud && Hud->CachedPandoraTreeUI)
	{
		Hud->CachedPandoraTreeUI->OnPandoraTreeClosed.RemoveDynamic(
			this,
			&ThisClass::HandlePandoraTreeClosed);
	}
	if (UHudUiRouter* UiRouter = Router.Get())
	{
		UiRouter->ReleaseInfoLayers();
	}
	InfoContentBundleLease.Reset();
}

void UHudScreenLayer::ClearInfoCloseTimer()
{
	if (APdHUD* Hud = OwnerHud.Get())
	{
		Hud->GetWorldTimerManager().ClearTimer(InfoCloseTimerHandle);
	}
	InfoCloseTimerHandle.Invalidate();
}

void UHudScreenLayer::ClearTrainingRoomPauseTimer()
{
	if (APdHUD* Hud = OwnerHud.Get())
	{
		Hud->GetWorldTimerManager().ClearTimer(TrainingRoomPauseTimerHandle);
	}
	TrainingRoomPauseTimerHandle.Invalidate();
}

void UHudScreenLayer::HandleDelayedTrainingRoomPause()
{
	TrainingRoomPauseTimerHandle.Invalidate();
	RefreshTrainingRoomPause();
}

bool UHudScreenLayer::IsTrainingRoomPauseUiOpen(const UUserWidget* IgnoredWidget) const
{
	const APdHUD* Hud = OwnerHud.Get();
	const UHudUiRouter* UiRouter = Router.Get();
	if (!Hud)
	{
		return false;
	}


	return (!bInfoClosing && Hud->CachedInfoUI != IgnoredWidget && IsInfoOpen())
		|| (UiRouter && UiRouter->IsSettingsMenuOpen())
		|| (!bPandoraTreeClosing && Hud->CachedPandoraTreeUI != IgnoredWidget && IsPandoraTreeOpen());
}

void UHudScreenLayer::SetTrainingRoomPaused(const bool bPaused)
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
