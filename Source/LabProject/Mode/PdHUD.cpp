#include "Mode/PdHUD.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Components/Widget.h"
#include "Engine/LocalPlayer.h"
#include "Mode/PdPlayerController.h"
#include "TimerManager.h"
#include "UI/InfoUiPresenter.h"
#include "UI/UiSubsystem.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/PlayerVitalsWidget.h"
#include "UI/Widget/SelectPandoraWidget.h"
#include "UI/Widget/PandoraTreeWidget.h"
#include "UI/Widget/RightNotificationsWidget.h"
#include "UI/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdHUD)

DEFINE_LOG_CATEGORY_STATIC(LogPdHUD, Log, All);

APdHUD::APdHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void APdHUD::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

void APdHUD::BeginPlay()
{
	Super::BeginPlay();
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);
}

void APdHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	ClearInfoUiCloseTimer();
	RemoveAllUiWidgets();
	Super::EndPlay(EndPlayReason);
}

void APdHUD::InitializeUi(UWidgetClassDefinition* InWidgetClassDefinition)
{
	if (!InWidgetClassDefinition)
	{
		return;
	}

	WidgetClassDefinition = InWidgetClassDefinition;
	CreateAllUi();
}

void APdHUD::DeinitializeUi(const UWidgetClassDefinition* InWidgetClassDefinition)
{
	if (!InWidgetClassDefinition || WidgetClassDefinition == InWidgetClassDefinition)
	{
		RemoveAllUiWidgets();
		WidgetClassDefinition = nullptr;
	}
}

void APdHUD::CreateAllUi()
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller || !Controller->IsLocalController() || !WidgetClassDefinition)
	{
		return;
	}

	RefreshUiBindings();

	const TSubclassOf<UUserWidget> PlayerHudWidgetClass = WidgetClassDefinition->GetPlayerHudWidgetClass();
	const TSubclassOf<UInfoWidget> InfoWidgetClass = WidgetClassDefinition->GetInfoWidgetClass();
	const TSubclassOf<USelectPandoraWidget> SelectPandoraWidgetClass = WidgetClassDefinition->GetSelectPandoraWidgetClass();
	const TSubclassOf<UUserWidget> AimCrosshairWidgetClass = WidgetClassDefinition->GetAimCrosshairWidgetClass();
	const TSubclassOf<UPandoraTreeWidget> PandoraTreeWidgetClass = WidgetClassDefinition->GetPandoraTreeWidgetClass();
	const TSubclassOf<URightNotificationsWidget> RightNotificationsWidgetClass = WidgetClassDefinition->GetRightNotificationsWidgetClass();

	UE_LOG(LogPdHUD, Log,
		TEXT("[Notification] CreateAllUi. hud=%s controller=%s widgetDefinition=%s rightNotificationClass=%s cachedRightNotification=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Controller),
		*GetNameSafe(WidgetClassDefinition),
		*GetNameSafe(RightNotificationsWidgetClass.Get()),
		*GetNameSafe(CachedRightNotificationsUI.Get()));

	if (!CachedPlayerHUD && PlayerHudWidgetClass)
	{
		CachedPlayerHUD = CreateWidget<UUserWidget>(Controller, PlayerHudWidgetClass);
	}

	if (CachedPlayerHUD && !CachedPlayerHUD->IsInViewport())
	{
		CachedPlayerHUD->AddToViewport();
	}
	ApplyStatusViewModelToPlayerHud();

	if (!CachedInfoUI && InfoWidgetClass)
	{
		CachedInfoUI = CreateWidget<UInfoWidget>(Controller, InfoWidgetClass);
	}

	if (!CachedSelectPandoraUI && SelectPandoraWidgetClass)
	{
		CachedSelectPandoraUI = CreateWidget<USelectPandoraWidget>(Controller, SelectPandoraWidgetClass);
	}

	if (!AimCrosshairWidget && AimCrosshairWidgetClass)
	{
		AimCrosshairWidget = CreateWidget<UUserWidget>(Controller, AimCrosshairWidgetClass);
	}

	if (!CachedPandoraTreeUI && PandoraTreeWidgetClass)
	{
		CachedPandoraTreeUI = CreateWidget<UPandoraTreeWidget>(Controller, PandoraTreeWidgetClass);
	}

	if (!CachedRightNotificationsUI && RightNotificationsWidgetClass)
	{
		CachedRightNotificationsUI = CreateWidget<URightNotificationsWidget>(Controller, RightNotificationsWidgetClass);
		UE_LOG(LogPdHUD, Log,
			TEXT("[Notification] created RightNotifications widget. class=%s widget=%s"),
			*GetNameSafe(RightNotificationsWidgetClass.Get()),
			*GetNameSafe(CachedRightNotificationsUI.Get()));
	}
	else if (!RightNotificationsWidgetClass)
	{
		UE_LOG(LogPdHUD, Warning,
			TEXT("[Notification] RightNotificationsWidgetClass is not set. widgetDefinition=%s"),
			*GetNameSafe(WidgetClassDefinition));
	}

	if (CachedRightNotificationsUI && !CachedRightNotificationsUI->IsInViewport())
	{
		CachedRightNotificationsUI->AddToViewport(20);
		UE_LOG(LogPdHUD, Log,
			TEXT("[Notification] RightNotifications widget added to viewport. widget=%s"),
			*GetNameSafe(CachedRightNotificationsUI.Get()));
	}

	if (CachedInfoUI)
	{
		CachedInfoUI->OnClickedInfoCenterButton.Clear();
		if (UInfoUiPresenter* InfoUiPresenter = GetInfoUiPresenter())
		{
			InfoUiPresenter->BindInfoUi(CachedInfoUI);
			CachedInfoUI->OnClickedInfoCenterButton.AddUniqueDynamic(InfoUiPresenter, &UInfoUiPresenter::HandleClickedInfoCenterButton);
		}

		if (URightStatusWidget* RightStatusWidget = CachedInfoUI->GetRightStatusWidget())
		{
			ApplyStatusViewModelToWidget(RightStatusWidget);
		}
	}

	if (CachedSelectPandoraUI)
	{
		CachedSelectPandoraUI->OnSelected.Clear();
		if (UInfoUiPresenter* InfoUiPresenter = GetInfoUiPresenter())
		{
			CachedSelectPandoraUI->OnSelected.AddUniqueDynamic(InfoUiPresenter, &UInfoUiPresenter::HandleSelectedPandoraDirection);
		}
	}
}

void APdHUD::OpenInfoUi()
{
	ClearInfoUiCloseTimer();

	APdPlayerController* Controller = GetPdController();
	if (!Controller)
	{
		return;
	}

	if (!CachedInfoUI)
	{
		CreateAllUi();
	}

	if (!CachedInfoUI)
	{
		return;
	}

	CachedInfoUI->OnClickedInfoCenterButton.Clear();
	UInfoUiPresenter* InfoUiPresenter = GetInfoUiPresenter();
	if (InfoUiPresenter)
	{
		InfoUiPresenter->BindInfoUi(CachedInfoUI);
		CachedInfoUI->OnClickedInfoCenterButton.AddUniqueDynamic(InfoUiPresenter, &UInfoUiPresenter::HandleClickedInfoCenterButton);
	}

	if (URightStatusWidget* RightStatusWidget = CachedInfoUI->GetRightStatusWidget())
	{
		ApplyStatusViewModelToWidget(RightStatusWidget);
	}

	if (CachedPlayerHUD)
	{
		CachedPlayerHUD->SetVisibility(ESlateVisibility::Collapsed);
	}

	CachedInfoUI->AddToViewport();
	ToggleUiMode(true);
	CachedInfoUI->ShowInfoUi();

	UE_LOG(LogPdHUD, Log,
		TEXT("[InfoAnimation] OpenInfoUi widget=%s inViewport=%s visibility=%s"),
		*GetNameSafe(CachedInfoUI.Get()),
		CachedInfoUI->IsInViewport() ? TEXT("true") : TEXT("false"),
		*UEnum::GetValueAsString(CachedInfoUI->GetVisibility()));

	if (InfoUiPresenter)
	{
		InfoUiPresenter->HandleOpenedInfoUi();
	}

	CachedInfoUI->SelectProfileTab();
}

void APdHUD::CloseInfoUi()
{
	if (!CachedInfoUI)
	{
		if (CachedPlayerHUD)
		{
			CachedPlayerHUD->SetVisibility(ESlateVisibility::Visible);
		}
		ToggleUiMode(false);
		return;
	}

	ClearInfoUiCloseTimer();
	CachedInfoUI->HideInfoUi();

	const float HideAnimationDelay = CachedInfoUI->GetHideAnimationDelay();
	UE_LOG(LogPdHUD, Log,
		TEXT("[InfoAnimation] CloseInfoUi widget=%s hideDelay=%.3f inViewport=%s visibility=%s"),
		*GetNameSafe(CachedInfoUI.Get()),
		HideAnimationDelay,
		CachedInfoUI->IsInViewport() ? TEXT("true") : TEXT("false"),
		*UEnum::GetValueAsString(CachedInfoUI->GetVisibility()));

	if (HideAnimationDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			InfoUiCloseTimerHandle,
			this,
			&ThisClass::FinishCloseInfoUi,
			HideAnimationDelay,
			false);
		return;
	}

	FinishCloseInfoUi();
}

void APdHUD::ToggleInfoUi()
{
	if (CachedInfoUI && CachedInfoUI->IsInViewport())
	{
		CloseInfoUi();
		return;
	}

	OpenInfoUi();
}

void APdHUD::OpenPandoraTreeUi()
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller)
	{
		return;
	}

	if (!CachedPandoraTreeUI)
	{
		CreateAllUi();
	}

	if (!CachedPandoraTreeUI)
	{
		return;
	}

	if (!CachedPandoraTreeUI->IsInViewport())
	{
		CachedPandoraTreeUI->AddToViewport();
	}

	CachedPandoraTreeUI->ShowPandoraTree();
}

void APdHUD::ClosePandoraTreeUi()
{
	if (CachedPandoraTreeUI)
	{
		CachedPandoraTreeUI->HidePandoraTree();
	}
}

void APdHUD::TogglePandoraTreeUi()
{
	if (CachedPandoraTreeUI && CachedPandoraTreeUI->IsInViewport())
	{
		ClosePandoraTreeUi();
		return;
	}

	OpenPandoraTreeUi();
}

void APdHUD::ToggleUiMode(bool bOn)
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller)
	{
		return;
	}

	if (bOn)
	{
		UWidget* WidgetToFocus = nullptr;
		if (CachedSelectPandoraUI && CachedSelectPandoraUI->IsInViewport())
		{
			WidgetToFocus = CachedSelectPandoraUI;
		}
		else if (CachedInfoUI && CachedInfoUI->IsInViewport())
		{
			WidgetToFocus = CachedInfoUI;
		}
		else if (CachedPandoraTreeUI && CachedPandoraTreeUI->IsInViewport())
		{
			WidgetToFocus = CachedPandoraTreeUI;
		}

		UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(Controller, WidgetToFocus, EMouseLockMode::DoNotLock, false, false);
		Controller->bShowMouseCursor = true;
		Controller->bEnableClickEvents = true;
		Controller->bEnableMouseOverEvents = true;

		int32 ViewportSizeX = 0;
		int32 ViewportSizeY = 0;
		Controller->GetViewportSize(ViewportSizeX, ViewportSizeY);
		Controller->SetMouseLocation(ViewportSizeX / 2, ViewportSizeY / 2);
		return;
	}

	if (IsGameplayInputBlockedByUi())
	{
		ToggleUiMode(true);
		return;
	}

	UWidgetBlueprintLibrary::SetInputMode_GameOnly(Controller, false);
	Controller->bShowMouseCursor = false;
	Controller->bEnableClickEvents = false;
	Controller->bEnableMouseOverEvents = false;
}

bool APdHUD::IsGameplayInputBlockedByUi() const
{
	return (CachedInfoUI && CachedInfoUI->IsInViewport())
		|| (CachedSelectPandoraUI && CachedSelectPandoraUI->IsInViewport())
		|| (CachedPandoraTreeUI && CachedPandoraTreeUI->IsInViewport());
}

void APdHUD::OpenSelectPandoraUi()
{
	if (!CachedSelectPandoraUI)
	{
		CreateAllUi();
	}

	if (!CachedSelectPandoraUI)
	{
		return;
	}

	CachedSelectPandoraUI->AddToViewport();
	SetActorTickEnabled(true);
	ToggleUiMode(true);
}

void APdHUD::CloseSelectPandoraUi()
{
	if (CachedSelectPandoraUI)
	{
		CachedSelectPandoraUI->SetDirection(CachedDirIndex);
		CachedSelectPandoraUI->RemoveFromParent();
	}

	SetActorTickEnabled(false);
	ToggleUiMode(false);
}

void APdHUD::UpdateSelectPandoraDirectionFromMouse()
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller || !CachedSelectPandoraUI || !CachedSelectPandoraUI->IsInViewport() || !WidgetClassDefinition)
	{
		return;
	}

	const FSelectPandoraWidgetSettings& SelectPandoraSettings = WidgetClassDefinition->GetSelectPandoraWidgetSettings();

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	Controller->GetViewportSize(ViewportSizeX, ViewportSizeY);

	float MouseX = 0.f;
	float MouseY = 0.f;
	Controller->GetMousePosition(MouseX, MouseY);

	const FVector2D MousePosition(MouseX, MouseY);
	const FVector2D ViewportCenter(static_cast<double>(ViewportSizeX) / 2.0, static_cast<double>(ViewportSizeY) / 2.0);
	const FVector2D DirectionFromCenter = MousePosition - ViewportCenter;

	if (DirectionFromCenter.Size() < SelectPandoraSettings.DeadZoneRadius)
	{
		CachedDirIndex = -1;
		return;
	}

	const double SegmentAngle = SelectPandoraSettings.SegmentAngle;
	if (FMath::IsNearlyZero(SegmentAngle))
	{
		return;
	}

	const double DirectionAngle = FMath::RadiansToDegrees(FMath::Atan2(DirectionFromCenter.Y, DirectionFromCenter.X));
	const double NormalizedAngle = FMath::Fmod(DirectionAngle + 450.0 + (SegmentAngle / 2.0), 360.0);
	CachedDirIndex = FMath::FloorToInt(NormalizedAngle / SegmentAngle);
}

void APdHUD::ShowAimCrosshair(FGameplayTag DesiredCrosshairWidgetTag)
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller || !WidgetClassDefinition)
	{
		return;
	}

	TSubclassOf<UUserWidget> DesiredCrosshairWidgetClass = WidgetClassDefinition->FindWidgetClassByTag(DesiredCrosshairWidgetTag);
	if (!DesiredCrosshairWidgetClass)
	{
		DesiredCrosshairWidgetClass = WidgetClassDefinition->GetAimCrosshairWidgetClass();
	}

	if (!DesiredCrosshairWidgetClass)
	{
		return;
	}

	if (!AimCrosshairWidget || AimCrosshairWidget->GetClass() != DesiredCrosshairWidgetClass)
	{
		HideAimCrosshair();
		AimCrosshairWidget = CreateWidget<UUserWidget>(Controller, DesiredCrosshairWidgetClass);
	}

	if (AimCrosshairWidget && !AimCrosshairWidget->IsInViewport())
	{
		AimCrosshairWidget->AddToViewport();
	}
}

void APdHUD::HideAimCrosshair()
{
	if (AimCrosshairWidget)
	{
		AimCrosshairWidget->RemoveFromParent();
	}
}

void APdHUD::ShowRightNotification(const FPdNotificationData& NotificationData)
{
	UE_LOG(LogPdHUD, Log,
		TEXT("[Notification] ShowRightNotification. hud=%s cachedWidget=%s inViewport=%s text=%s icon=%s"),
		*GetNameSafe(this),
		*GetNameSafe(CachedRightNotificationsUI.Get()),
		CachedRightNotificationsUI && CachedRightNotificationsUI->IsInViewport() ? TEXT("true") : TEXT("false"),
		*NotificationData.Text.ToString(),
		*GetNameSafe(NotificationData.IconResource));

	if (!CachedRightNotificationsUI)
	{
		UE_LOG(LogPdHUD, Log,
			TEXT("[Notification] cached RightNotifications widget missing, calling CreateAllUi. hud=%s"),
			*GetNameSafe(this));
		CreateAllUi();
	}

	if (!CachedRightNotificationsUI)
	{
		UE_LOG(LogPdHUD, Warning,
			TEXT("[Notification] skipped: RightNotificationsWidgetClass is not set. text=%s"),
			*NotificationData.Text.ToString());
		return;
	}

	if (!CachedRightNotificationsUI->IsInViewport())
	{
		CachedRightNotificationsUI->AddToViewport(20);
		UE_LOG(LogPdHUD, Log,
			TEXT("[Notification] RightNotifications widget re-added to viewport. widget=%s"),
			*GetNameSafe(CachedRightNotificationsUI.Get()));
	}

	UE_LOG(LogPdHUD, Log,
		TEXT("[Notification] enqueue to RightNotifications widget. widget=%s text=%s"),
		*GetNameSafe(CachedRightNotificationsUI.Get()),
		*NotificationData.Text.ToString());
	CachedRightNotificationsUI->EnqueueNotification(NotificationData);
}

void APdHUD::RefreshUiBindings()
{
	if (UUiSubsystem* UiSubsystem = GetUiSubsystem())
	{
		UiSubsystem->RefreshStatusViewModel();
	}
}

void APdHUD::OnOpenInfoUiInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);
	ToggleInfoUi();
}

void APdHUD::OnSelectPandoraInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);
	OpenSelectPandoraUi();
}

void APdHUD::OnSelectPandoraInputEnded(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);
	CloseSelectPandoraUi();
}

void APdHUD::OnPandoraTreeInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);
	TogglePandoraTreeUi();
}

void APdHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateSelectPandoraDirectionFromMouse();
}

APdPlayerController* APdHUD::GetPdController() const
{
	return Cast<APdPlayerController>(GetOwningPlayerController());
}

UInfoUiPresenter* APdHUD::GetInfoUiPresenter()
{
	if (CachedInfoUiPresenter)
	{
		return CachedInfoUiPresenter;
	}

	const TSubclassOf<UInfoUiPresenter> PresenterClass = WidgetClassDefinition ? WidgetClassDefinition->GetInfoWidgetSettings().PresenterClass : nullptr;
	APdPlayerController* Controller = GetPdController();
	if (!Controller || !PresenterClass)
	{
		return nullptr;
	}

	CachedInfoUiPresenter = NewObject<UInfoUiPresenter>(Controller, PresenterClass);
	if (CachedInfoUiPresenter)
	{
		CachedInfoUiPresenter->Initialize(Controller);
	}

	return CachedInfoUiPresenter;
}

UUiSubsystem* APdHUD::GetUiSubsystem() const
{
	const APdPlayerController* Controller = GetPdController();
	ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
}

bool APdHUD::ApplyStatusViewModelToWidget(UUserWidget* InWidget)
{
	if (UUiSubsystem* UiSubsystem = GetUiSubsystem())
	{
		return UiSubsystem->ApplyStatusViewModelToWidget(InWidget);
	}

	return false;
}

bool APdHUD::ApplyStatusViewModelToPlayerHud()
{
	if (!CachedPlayerHUD)
	{
		GetWorldTimerManager().ClearTimer(PlayerHudStatusViewModelRetryTimerHandle);
		return false;
	}

	bool bFoundPlayerVitals = false;
	bool bAppliedViewModel = false;

	if (UPlayerVitalsWidget* PlayerVitalsWidget = Cast<UPlayerVitalsWidget>(CachedPlayerHUD))
	{
		bFoundPlayerVitals = true;
		bAppliedViewModel |= ApplyStatusViewModelToWidget(PlayerVitalsWidget);
	}

	ApplyStatusViewModelToPlayerHudRecursive(CachedPlayerHUD, bFoundPlayerVitals, bAppliedViewModel);

	if (!bFoundPlayerVitals || bAppliedViewModel)
	{
		GetWorldTimerManager().ClearTimer(PlayerHudStatusViewModelRetryTimerHandle);
		return bAppliedViewModel;
	}

	if (!GetWorldTimerManager().IsTimerActive(PlayerHudStatusViewModelRetryTimerHandle))
	{
		GetWorldTimerManager().SetTimer(
			PlayerHudStatusViewModelRetryTimerHandle,
			this,
			&ThisClass::RetryApplyStatusViewModelToPlayerHud,
			0.1f,
			true);
	}

	return false;
}

void APdHUD::ApplyStatusViewModelToPlayerHudRecursive(UUserWidget* RootWidget, bool& bFoundPlayerVitals, bool& bAppliedViewModel)
{
	if (!RootWidget || !RootWidget->WidgetTree)
	{
		return;
	}

	RootWidget->WidgetTree->ForEachWidget([this, &bFoundPlayerVitals, &bAppliedViewModel](UWidget* Widget)
	{
		if (!Widget)
		{
			return;
		}

		if (UPlayerVitalsWidget* PlayerVitalsWidget = Cast<UPlayerVitalsWidget>(Widget))
		{
			bFoundPlayerVitals = true;
			bAppliedViewModel |= ApplyStatusViewModelToWidget(PlayerVitalsWidget);
		}

		if (UUserWidget* ChildUserWidget = Cast<UUserWidget>(Widget))
		{
			ApplyStatusViewModelToPlayerHudRecursive(ChildUserWidget, bFoundPlayerVitals, bAppliedViewModel);
		}
	});
}

void APdHUD::RetryApplyStatusViewModelToPlayerHud()
{
	ApplyStatusViewModelToPlayerHud();
}

void APdHUD::FinishCloseInfoUi()
{
	ClearInfoUiCloseTimer();

	if (CachedInfoUI)
	{
		UE_LOG(LogPdHUD, Log,
			TEXT("[InfoAnimation] FinishCloseInfoUi widget=%s inViewportBeforeRemove=%s"),
			*GetNameSafe(CachedInfoUI.Get()),
			CachedInfoUI->IsInViewport() ? TEXT("true") : TEXT("false"));
		CachedInfoUI->RemoveFromParent();
	}

	if (CachedPlayerHUD)
	{
		CachedPlayerHUD->SetVisibility(ESlateVisibility::Visible);
	}

	ToggleUiMode(false);
}

void APdHUD::ClearInfoUiCloseTimer()
{
	GetWorldTimerManager().ClearTimer(InfoUiCloseTimerHandle);
}

void APdHUD::RemoveAllUiWidgets()
{
	HideAimCrosshair();
	ClearInfoUiCloseTimer();
	GetWorldTimerManager().ClearTimer(PlayerHudStatusViewModelRetryTimerHandle);

	if (CachedPlayerHUD)
	{
		CachedPlayerHUD->RemoveFromParent();
		CachedPlayerHUD = nullptr;
	}

	if (CachedInfoUI)
	{
		CachedInfoUI->RemoveFromParent();
		CachedInfoUI = nullptr;
	}

	if (CachedSelectPandoraUI)
	{
		CachedSelectPandoraUI->RemoveFromParent();
		CachedSelectPandoraUI = nullptr;
	}

	if (CachedPandoraTreeUI)
	{
		CachedPandoraTreeUI->RemoveFromParent();
		CachedPandoraTreeUI = nullptr;
	}

	if (CachedRightNotificationsUI)
	{
		CachedRightNotificationsUI->RemoveFromParent();
		CachedRightNotificationsUI = nullptr;
	}

	if (CachedInfoUiPresenter)
	{
		CachedInfoUiPresenter->Deinitialize();
		CachedInfoUiPresenter = nullptr;
	}
}
