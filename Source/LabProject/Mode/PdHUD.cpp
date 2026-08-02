#include "Mode/PdHUD.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Components/Widget.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Mode/PdPlayerController.h"
#include "TimerManager.h"
#include "UI/InfoUiPresenter.h"
#include "UI/PdHudUiRouter.h"
#include "UI/UiSubsystem.h"
#include "UI/Widget/DamageScreenEffectWidget.h"
#include "UI/Widget/GoldenKillAnnouncementWidget.h"
#include "UI/Widget/HudTimerWidget.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/KillLogWidget.h"
#include "UI/Widget/MenuPopupWidget.h"
#include "UI/Widget/PlayerVitalsWidget.h"
#include "UI/Widget/SelectPandoraWidget.h"
#include "UI/Widget/PandoraTreeWidget.h"
#include "UI/Widget/RightNotificationsWidget.h"
#include "UI/Widget/RespawnDelayWidget.h"
#include "Definition/UI/WidgetClassDefinition.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/TransBuffer.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdHUD)

namespace
{
	EEnum_Direction ResolveSelectPandoraDirectionFromIndex(const int32 Index)
	{
		switch (Index)
		{
		case 0:
			return EEnum_Direction::Up;
		case 1:
			return EEnum_Direction::Right;
		case 2:
			return EEnum_Direction::Down;
		case 3:
			return EEnum_Direction::Left;
		default:
			return EEnum_Direction::Center;
		}
	}

	void ResetEditorTransactionBufferIfContainsPieObjects()
	{
#if WITH_EDITOR
		if (GEditor && GEditor->Trans && GEditor->Trans->ContainsPieObjects())
		{
			GEditor->ResetTransaction(NSLOCTEXT(
				"PdHUD",
				"TransactionContainedHudPieObject",
				"A HUD PIE object was in the transaction buffer and had to be destroyed"));
		}
#endif
	}
}

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
	EnsureUiRouter();
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);
}

void APdHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	RemoveAllUiWidgets();
	if (UiRouter)
	{
		UiRouter->Shutdown();
		UiRouter = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void APdHUD::InitializeUi(UWidgetClassDefinition* InWidgetClassDefinition)
{
	UPdHudUiRouter* Router = EnsureUiRouter();
	if (!Router || !InWidgetClassDefinition)
	{
		return;
	}

	const bool bActiveDefinitionChanged =
		Router->GetActiveDefinition() != InWidgetClassDefinition;
	if (bActiveDefinitionChanged && Router->GetActiveDefinition())
	{
		RemoveAllUiWidgets();
	}

	Router->AddDefinitionRequest(InWidgetClassDefinition);
	CreateAllUi();
}

void APdHUD::DeinitializeUi(const UWidgetClassDefinition* InWidgetClassDefinition)
{
	if (!UiRouter || !InWidgetClassDefinition)
	{
		return;
	}

	const bool bWasActiveDefinition =
		UiRouter->GetActiveDefinition() == InWidgetClassDefinition;
	const bool bActiveDefinitionChanged =
		UiRouter->RemoveDefinitionRequest(InWidgetClassDefinition);
	if (bWasActiveDefinition && bActiveDefinitionChanged)
	{
		RemoveAllUiWidgets();
		if (UiRouter->GetActiveDefinition())
		{
			CreateAllUi();
		}
	}
}

void APdHUD::RefreshHudTimerVisibility()
{
	ApplyHudTimerVisibility();
}

void APdHUD::CreateAllUi()
{
	if (UPdHudUiRouter* Router = EnsureUiRouter())
	{
		Router->EnsureCoreLayers();
	}
}

void APdHUD::OpenInfoUiFocused(const EPdInfoUiSection Section)
{
	UPdHudUiRouter* Router = EnsureUiRouter();
	if (!Router)
	{
		return;
	}

	const bool bWasInfoReadyForSectionChange =
		CachedInfoUI
		&& CachedInfoUI->IsInViewport()
		&& !Router->IsInfoClosing()
		&& !Router->IsSettingsMenuOpen();
	if (bWasInfoReadyForSectionChange
		&& CachedInfoUI->GetFocusedSection() == Section)
	{
		Router->CloseInfo();
		return;
	}

	if (!bWasInfoReadyForSectionChange)
	{
		Router->OpenInfo();
	}

	if (CachedInfoUI && CachedInfoUI->IsInViewport())
	{
		CachedInfoUI->FocusSection(Section, bWasInfoReadyForSectionChange);
	}
}

void APdHUD::CloseInfoUi()
{
	if (UiRouter)
	{
		UiRouter->CloseInfo();
	}
}

void APdHUD::CloseInfoUiInternal(const bool bSuppressCameraReturn, const bool bImmediate)
{
	if (UiRouter)
	{
		UiRouter->CloseInfo(bSuppressCameraReturn, bImmediate);
	}
}

void APdHUD::ToggleInfoUi()
{
	if (UPdHudUiRouter* Router = EnsureUiRouter())
	{
		Router->ToggleInfo();
	}
}

void APdHUD::OpenPandoraTreeUi()
{
	if (UPdHudUiRouter* Router = EnsureUiRouter())
	{
		Router->OpenPandoraTree();
	}
}

void APdHUD::ClosePandoraTreeUi()
{
	if (UiRouter)
	{
		UiRouter->ClosePandoraTree();
	}
}

void APdHUD::ClosePandoraTreeUiInternal(const bool bSuppressCameraReturn, const bool bImmediate)
{
	if (UiRouter)
	{
		UiRouter->ClosePandoraTree(bSuppressCameraReturn, bImmediate);
	}
}

void APdHUD::TogglePandoraTreeUi()
{
	if (UPdHudUiRouter* Router = EnsureUiRouter())
	{
		Router->TogglePandoraTree();
	}
}

void APdHUD::ToggleUiMode(bool bOn)
{
	UPdHudUiRouter* Router = EnsureUiRouter();
	if (!Router)
	{
		return;
	}

	RefreshPlayerHudVisibility();

	if (bOn)
	{
		UWidget* WidgetToFocus = nullptr;
		bool bPreserveGameplayInputMode = false;
		if (UMenuPopupWidget* SettingsMenuWidget = GetActiveSettingsMenuWidget())
		{
			if (UWidget* GuideWidget = SettingsMenuWidget->GetActiveGuideWidget())
			{
				WidgetToFocus = GuideWidget;
			}
			else if (SettingsMenuWidget->IsInViewport())
			{
				WidgetToFocus = SettingsMenuWidget;
			}
		}
		else if (CachedSelectPandoraUI && CachedSelectPandoraUI->IsInViewport())
		{
			WidgetToFocus = CachedSelectPandoraUI;
			bPreserveGameplayInputMode = true;
		}
		else if (CachedInfoUI && !Router->IsInfoClosing() && CachedInfoUI->IsInViewport())
		{
			WidgetToFocus = CachedInfoUI;
		}
		else if (CachedPandoraTreeUI && !Router->IsPandoraTreeClosing() && CachedPandoraTreeUI->IsInViewport())
		{
			WidgetToFocus = CachedPandoraTreeUI;
		}

		Router->RouteInput(
			WidgetToFocus,
			bPreserveGameplayInputMode,
			WidgetToFocus != CachedInfoUI.Get());
		return;
	}

	if (IsGameplayInputBlockedByUi())
	{
		ToggleUiMode(true);
		return;
	}

	Router->ReleaseInput();
}

bool APdHUD::IsGameplayInputBlockedByUi() const
{
	return IsPlayerHudSuppressedByUi();
}

bool APdHUD::IsPlayerHudSuppressedByUi() const
{
	return (UiRouter && UiRouter->IsScreenLayerBlockingGameplayInput())
		|| (CachedSelectPandoraUI && CachedSelectPandoraUI->IsInViewport())
		|| (UiRouter && UiRouter->IsSettingsMenuOpen())
		|| (UiRouter && UiRouter->IsScoreboardOpen());
}

bool APdHUD::IsSelectPandoraUiOpen() const
{
	return CachedSelectPandoraUI && CachedSelectPandoraUI->IsInViewport();
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
	// The router changes cursor state without changing Enhanced Input mode, so
	// the held selection action does not complete on the following frame.
	ToggleUiMode(true);
}

bool APdHUD::CloseSelectPandoraUi()
{
	return CloseSelectPandoraUiInternal(true);
}

bool APdHUD::CloseSelectPandoraUiInternal(const bool bCommitSelection)
{
	bool bSelectionWouldChangeLoadout = false;
	if (CachedSelectPandoraUI)
	{
		if (bCommitSelection)
		{
			const EEnum_Direction SelectedDirection = ResolveSelectPandoraDirectionFromIndex(CachedDirIndex);
			if (UInfoUiPresenter* InfoUiPresenter = GetInfoUiPresenter())
			{
				bSelectionWouldChangeLoadout = InfoUiPresenter->WouldSelectedPandoraDirectionChangeLoadout(SelectedDirection);
			}

			CachedSelectPandoraUI->SetDirection(CachedDirIndex);
		}
		CachedSelectPandoraUI->RemoveFromParent();
	}

	SetActorTickEnabled(false);
	ToggleUiMode(false);
	return bSelectionWouldChangeLoadout;
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
	if (UPdHudUiRouter* Router = EnsureUiRouter())
	{
		Router->ShowAimCrosshair(DesiredCrosshairWidgetTag);
	}
}

void APdHUD::HideAimCrosshair()
{
	if (UiRouter)
	{
		UiRouter->HideAimCrosshair();
	}
}

void APdHUD::ShowRightNotification(const FPdNotificationData& NotificationData)
{


	if (!CachedRightNotificationsUI)
	{

		CreateAllUi();
	}

	if (!CachedRightNotificationsUI)
	{

		return;
	}

	if (!CachedRightNotificationsUI->IsInViewport())
	{
		CachedRightNotificationsUI->AddToViewport(20);

	}


	CachedRightNotificationsUI->EnqueueNotification(NotificationData);
}

void APdHUD::ShowDamageScreenEffect(float DamageAmount)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!CachedPlayerHUD)
	{
		CreateAllUi();
	}

	UDamageScreenEffectWidget* DamageScreenEffectWidget = FindDamageScreenEffectWidget();
	if (!DamageScreenEffectWidget)
	{

		return;
	}

	DamageScreenEffectWidget->PlayDamageScreenEffect(DamageAmount);
}

void APdHUD::ShowGoldenKillAnnouncement(const FText& AnnouncementText)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!CachedPlayerHUD)
	{
		CreateAllUi();
	}

	UGoldenKillAnnouncementWidget* GoldenKillWidget = FindGoldenKillAnnouncementWidget();
	if (!GoldenKillWidget)
	{

		return;
	}

	GoldenKillWidget->PlayGoldenKillAnnouncement(AnnouncementText);
}

void APdHUD::AddKillLogEntry(const FKillLogEntry& KillLogEntry)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!CachedPlayerHUD)
	{
		CreateAllUi();
	}

	UKillLogWidget* KillLogWidget = FindKillLogWidget();
	if (!KillLogWidget)
	{

		return;
	}

	KillLogWidget->AddKillLogEntry(KillLogEntry);
}

void APdHUD::ShowRespawnDelay(const float DelaySeconds)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!CachedPlayerHUD)
	{
		CreateAllUi();
	}

	URespawnDelayWidget* RespawnDelayWidget = FindRespawnDelayWidget();
	if (!RespawnDelayWidget)
	{

		return;
	}

	RespawnDelayWidget->StartRespawnDelay(DelaySeconds);
}

void APdHUD::HideRespawnDelay()
{
	if (URespawnDelayWidget* RespawnDelayWidget = FindRespawnDelayWidget())
	{
		RespawnDelayWidget->HideRespawnDelay();
	}
}

void APdHUD::ShowInGameScoreboard()
{
	if (UPdHudUiRouter* Router = EnsureUiRouter())
	{
		Router->ShowScoreboard();
	}
}

void APdHUD::HideInGameScoreboard()
{
	if (UiRouter)
	{
		UiRouter->HideScoreboard();
	}
}

void APdHUD::RefreshInGameScoreboard()
{
	if (UiRouter)
	{
		UiRouter->RefreshScoreboard();
	}
}

void APdHUD::OpenSettingsMenu()
{
	if (UPdHudUiRouter* Router = EnsureUiRouter())
	{
		Router->OpenSettingsMenu();
	}
}

void APdHUD::ToggleSettingsMenu()
{
	if (UPdHudUiRouter* Router = EnsureUiRouter())
	{
		Router->ToggleSettingsMenu();
	}
}

bool APdHUD::HandleEscapeInput()
{
	if (UiRouter && UiRouter->IsSettingsMenuOpen())
	{
		if (UMenuPopupWidget* SettingsMenuWidget = GetActiveSettingsMenuWidget();
			SettingsMenuWidget && SettingsMenuWidget->CloseGuide())
		{
			return true;
		}

		CloseActiveSettingsMenuPopup();
		return true;
	}

	if (CachedSelectPandoraUI && CachedSelectPandoraUI->IsInViewport())
	{
		CloseSelectPandoraUiInternal(false);
		return true;
	}

	if (CachedInfoUI && CachedInfoUI->IsInViewport())
	{
		if (!UiRouter || !UiRouter->IsInfoClosing())
		{
			CloseInfoUi();
		}
		return true;
	}

	if (CachedPandoraTreeUI && CachedPandoraTreeUI->IsInViewport())
	{
		if (!UiRouter || !UiRouter->IsPandoraTreeClosing())
		{
			ClosePandoraTreeUi();
		}
		return true;
	}

	OpenSettingsMenu();
	return UiRouter && UiRouter->IsSettingsMenuOpen();
}

void APdHUD::RefreshUiBindings()
{
	if (UUiSubsystem* UiSubsystem = GetUiSubsystem())
	{
		UiSubsystem->RefreshStatusViewModel();
	}
}

void APdHUD::OnOpenSettingsMenuInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);
	ToggleSettingsMenu();
}

void APdHUD::OnSelectPandoraInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);
	OpenSelectPandoraUi();
}

bool APdHUD::OnSelectPandoraInputEnded(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);
	return CloseSelectPandoraUi();
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

UPdHudUiRouter* APdHUD::EnsureUiRouter()
{
	if (!UiRouter)
	{
		UiRouter = NewObject<UPdHudUiRouter>(this);
		if (UiRouter)
		{
			UiRouter->Initialize(this);
		}
	}
	return UiRouter;
}

UInfoUiPresenter* APdHUD::GetInfoUiPresenter()
{
	if (CachedInfoUiPresenter)
	{
		return CachedInfoUiPresenter;
	}

	const TSubclassOf<UInfoUiPresenter> PresenterClass =
		WidgetClassDefinition ? WidgetClassDefinition->GetInfoPresenterClass() : nullptr;
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

bool APdHUD::ApplyStatusViewModelToWidgetTree(UUserWidget* RootWidget)
{
	if (UUiSubsystem* UiSubsystem = GetUiSubsystem())
	{
		return UiSubsystem->ApplyStatusViewModelToWidgetTree(RootWidget);
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

UDamageScreenEffectWidget* APdHUD::FindDamageScreenEffectWidget()
{
	if (CachedDamageScreenEffectWidget)
	{
		return CachedDamageScreenEffectWidget;
	}

	if (!CachedPlayerHUD || !CachedPlayerHUD->WidgetTree)
	{
		return nullptr;
	}

	if (UDamageScreenEffectWidget* NamedWidget = Cast<UDamageScreenEffectWidget>(CachedPlayerHUD->WidgetTree->FindWidget(TEXT("WBP_DamageScreenEffect"))))
	{
		CachedDamageScreenEffectWidget = NamedWidget;
		return NamedWidget;
	}

	if (UDamageScreenEffectWidget* NamedWidget = Cast<UDamageScreenEffectWidget>(CachedPlayerHUD->WidgetTree->FindWidget(TEXT("DamageScreenEffect"))))
	{
		CachedDamageScreenEffectWidget = NamedWidget;
		return NamedWidget;
	}

	UDamageScreenEffectWidget* FoundWidget = nullptr;
	CachedPlayerHUD->WidgetTree->ForEachWidget([&FoundWidget](UWidget* Widget)
	{
		if (!FoundWidget)
		{
			FoundWidget = Cast<UDamageScreenEffectWidget>(Widget);
		}
	});

	CachedDamageScreenEffectWidget = FoundWidget;
	return FoundWidget;
}

UGoldenKillAnnouncementWidget* APdHUD::FindGoldenKillAnnouncementWidget()
{
	if (CachedGoldenKillAnnouncementWidget)
	{
		return CachedGoldenKillAnnouncementWidget;
	}

	if (!CachedPlayerHUD || !CachedPlayerHUD->WidgetTree)
	{
		return nullptr;
	}

	if (UGoldenKillAnnouncementWidget* NamedWidget = Cast<UGoldenKillAnnouncementWidget>(CachedPlayerHUD->WidgetTree->FindWidget(TEXT("WBP_GoldenKillAnnouncement"))))
	{
		CachedGoldenKillAnnouncementWidget = NamedWidget;
		return NamedWidget;
	}

	if (UGoldenKillAnnouncementWidget* NamedWidget = Cast<UGoldenKillAnnouncementWidget>(CachedPlayerHUD->WidgetTree->FindWidget(TEXT("GoldenKillAnnouncement"))))
	{
		CachedGoldenKillAnnouncementWidget = NamedWidget;
		return NamedWidget;
	}

	if (UGoldenKillAnnouncementWidget* NamedWidget = Cast<UGoldenKillAnnouncementWidget>(CachedPlayerHUD->WidgetTree->FindWidget(TEXT("GoldenKillText"))))
	{
		CachedGoldenKillAnnouncementWidget = NamedWidget;
		return NamedWidget;
	}

	UGoldenKillAnnouncementWidget* FoundWidget = nullptr;
	CachedPlayerHUD->WidgetTree->ForEachWidget([&FoundWidget](UWidget* Widget)
	{
		if (!FoundWidget)
		{
			FoundWidget = Cast<UGoldenKillAnnouncementWidget>(Widget);
		}
	});

	CachedGoldenKillAnnouncementWidget = FoundWidget;
	return FoundWidget;
}

UKillLogWidget* APdHUD::FindKillLogWidget()
{
	if (CachedKillLogWidget)
	{
		return CachedKillLogWidget;
	}

	if (!CachedPlayerHUD || !CachedPlayerHUD->WidgetTree)
	{
		return nullptr;
	}

	if (UKillLogWidget* NamedWidget = Cast<UKillLogWidget>(CachedPlayerHUD->WidgetTree->FindWidget(TEXT("WBP_KillLog"))))
	{
		CachedKillLogWidget = NamedWidget;
		return NamedWidget;
	}

	if (UKillLogWidget* NamedWidget = Cast<UKillLogWidget>(CachedPlayerHUD->WidgetTree->FindWidget(TEXT("KillLog"))))
	{
		CachedKillLogWidget = NamedWidget;
		return NamedWidget;
	}

	UKillLogWidget* FoundWidget = nullptr;
	CachedPlayerHUD->WidgetTree->ForEachWidget([&FoundWidget](UWidget* Widget)
	{
		if (!FoundWidget)
		{
			FoundWidget = Cast<UKillLogWidget>(Widget);
		}
	});

	CachedKillLogWidget = FoundWidget;
	return FoundWidget;
}

UHudTimerWidget* APdHUD::FindHudTimerWidget()
{
	if (CachedHudTimerWidget)
	{
		return CachedHudTimerWidget;
	}

	if (!CachedPlayerHUD)
	{
		CreateAllUi();
	}

	if (!CachedPlayerHUD || !CachedPlayerHUD->WidgetTree)
	{
		return nullptr;
	}

	if (UHudTimerWidget* NamedWidget = Cast<UHudTimerWidget>(CachedPlayerHUD->WidgetTree->FindWidget(TEXT("WBP_HudTimer"))))
	{
		CachedHudTimerWidget = NamedWidget;
		return NamedWidget;
	}

	if (UHudTimerWidget* NamedWidget = Cast<UHudTimerWidget>(CachedPlayerHUD->WidgetTree->FindWidget(TEXT("HudTimer"))))
	{
		CachedHudTimerWidget = NamedWidget;
		return NamedWidget;
	}

	if (UHudTimerWidget* NamedWidget = Cast<UHudTimerWidget>(CachedPlayerHUD->WidgetTree->FindWidget(TEXT("TimerWidget"))))
	{
		CachedHudTimerWidget = NamedWidget;
		return NamedWidget;
	}

	UHudTimerWidget* FoundWidget = nullptr;
	CachedPlayerHUD->WidgetTree->ForEachWidget([&FoundWidget](UWidget* Widget)
	{
		if (!FoundWidget)
		{
			FoundWidget = Cast<UHudTimerWidget>(Widget);
		}
	});

	CachedHudTimerWidget = FoundWidget;
	return FoundWidget;
}

URespawnDelayWidget* APdHUD::FindRespawnDelayWidget()
{
	if (CachedRespawnDelayWidget)
	{
		return CachedRespawnDelayWidget;
	}

	if (!CachedPlayerHUD)
	{
		CreateAllUi();
	}

	if (!CachedPlayerHUD || !CachedPlayerHUD->WidgetTree)
	{
		return nullptr;
	}

	if (URespawnDelayWidget* NamedWidget = Cast<URespawnDelayWidget>(CachedPlayerHUD->WidgetTree->FindWidget(TEXT("WBP_RespawnDelay"))))
	{
		CachedRespawnDelayWidget = NamedWidget;
		return NamedWidget;
	}

	if (URespawnDelayWidget* NamedWidget = Cast<URespawnDelayWidget>(CachedPlayerHUD->WidgetTree->FindWidget(TEXT("RespawnDelay"))))
	{
		CachedRespawnDelayWidget = NamedWidget;
		return NamedWidget;
	}

	if (URespawnDelayWidget* NamedWidget = Cast<URespawnDelayWidget>(CachedPlayerHUD->WidgetTree->FindWidget(TEXT("RespawnDelayWidget"))))
	{
		CachedRespawnDelayWidget = NamedWidget;
		return NamedWidget;
	}

	URespawnDelayWidget* FoundWidget = nullptr;
	CachedPlayerHUD->WidgetTree->ForEachWidget([&FoundWidget](UWidget* Widget)
	{
		if (!FoundWidget)
		{
			FoundWidget = Cast<URespawnDelayWidget>(Widget);
		}
	});

	CachedRespawnDelayWidget = FoundWidget;
	return FoundWidget;
}

bool APdHUD::IsTrainingRoomMap() const
{
	if (!WidgetClassDefinition)
	{
		return false;
	}

	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	for (const FName TrainingMapName : WidgetClassDefinition->GetTrainingRoomMapNames())
	{
		if (TrainingMapName.IsNone())
		{
			continue;
		}

		if (CurrentLevelName.Equals(TrainingMapName.ToString(), ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

void APdHUD::RefreshTrainingRoomUiPause(const UUserWidget* IgnoredWidget)
{
	if (UiRouter)
	{
		UiRouter->RefreshTrainingRoomPause(IgnoredWidget);
	}
}

bool APdHUD::ShouldSuppressHudTimer()
{
	const UHudTimerWidget* HudTimerWidget = FindHudTimerWidget();
	return HudTimerWidget && HudTimerWidget->ShouldSuppressTimer();
}

void APdHUD::ApplyHudTimerVisibility()
{
	UHudTimerWidget* HudTimerWidget = FindHudTimerWidget();
	if (!HudTimerWidget)
	{
		return;
	}

	if (ShouldSuppressHudTimer())
	{
		HudTimerWidget->StopTimer(true);
		HudTimerWidget->SetVisibility(ESlateVisibility::Collapsed);

		return;
	}

	if (HudTimerWidget->GetVisibility() == ESlateVisibility::Collapsed)
	{
		HudTimerWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	HudTimerWidget->StartTimer();
}

void APdHUD::RetryApplyStatusViewModelToPlayerHud()
{
	ApplyStatusViewModelToPlayerHud();
}

void APdHUD::HandleSettingsMenuLayerClosed()
{
	RefreshTrainingRoomUiPause();
	RefreshPlayerHudVisibility();

	if (CachedInfoUI && (!UiRouter || !UiRouter->IsInfoClosing()) && CachedInfoUI->IsInViewport())
	{
		RestoreInfoUiInputMode();
		GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::RestoreInfoUiInputMode);
		return;
	}

	ToggleUiMode(false);
}

void APdHUD::CloseActiveSettingsMenuPopup()
{
	if (UiRouter)
	{
		UiRouter->CloseSettingsMenu();
	}
}

UMenuPopupWidget* APdHUD::GetActiveSettingsMenuWidget() const
{
	return UiRouter ? UiRouter->GetSettingsMenuWidget() : nullptr;
}

void APdHUD::RestoreInfoUiInputMode()
{
	if (!CachedInfoUI || (UiRouter && UiRouter->IsInfoClosing()) || !CachedInfoUI->IsInViewport())
	{
		return;
	}

	ToggleUiMode(true);
}

void APdHUD::ApplyInventoryWidgetSettings()
{
	if (!CachedInfoUI || !WidgetClassDefinition)
	{
		return;
	}

	URightInventoryWidget* RightInventoryWidget = CachedInfoUI->GetRightInventoryWidget();
	if (!RightInventoryWidget)
	{
		return;
	}

	const bool bTrainingRoom = IsTrainingRoomMap();
	const int32 InventoryItemCountLimit = WidgetClassDefinition->GetInventoryItemCountLimit(bTrainingRoom);
	RightInventoryWidget->SetInventorySlotCount(InventoryItemCountLimit);


}

void APdHUD::RefreshPlayerHudVisibility()
{
	if (!CachedPlayerHUD)
	{
		return;
	}

	CachedPlayerHUD->SetVisibility(
		IsPlayerHudSuppressedByUi()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
}

void APdHUD::RemoveAllUiWidgets()
{
	ResetEditorTransactionBufferIfContainsPieObjects();

	HideAimCrosshair();
	GetWorldTimerManager().ClearTimer(PlayerHudStatusViewModelRetryTimerHandle);
	CachedDamageScreenEffectWidget = nullptr;
	CachedGoldenKillAnnouncementWidget = nullptr;
	CachedKillLogWidget = nullptr;
	CachedHudTimerWidget = nullptr;
	CachedRespawnDelayWidget = nullptr;

	if (UiRouter)
	{
		UiRouter->ResetLayers();
	}

	if (CachedInfoUiPresenter)
	{
		CachedInfoUiPresenter->Deinitialize();
		CachedInfoUiPresenter = nullptr;
	}

	ResetEditorTransactionBufferIfContainsPieObjects();
}
