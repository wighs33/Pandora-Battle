#include "UI/HUD/PdHUD.h"
#include "UI/Common/EditorTransactionReset.h"
#include "UI/Core/PdUIActionRouter.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Components/Widget.h"
#include "Component/Player/ControllerPresentationComponent.h"
#include "Definition/Level/LevelDefinition.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Mode/ExperienceGameState.h"
#include "Mode/PdPlayerController.h"
#include "TimerManager.h"
#include "UI/Info/Presenter/InfoUiPresenter.h"
#include "UI/HUD/HudMenuLayer.h"
#include "UI/HUD/HudScoreboardLayer.h"
#include "UI/HUD/HudScreenLayer.h"
#include "UI/HUD/HudUiRouter.h"
#include "UI/Core/UiSubsystem.h"
#include "UI/Core/UiScreen.h"
#include "UI/HUD/Combat/DamageScreenEffectWidget.h"
#include "UI/HUD/Match/GoldenKillAnnouncementWidget.h"
#include "UI/HUD/Player/HudTimerWidget.h"
#include "UI/Info/InfoWidget.h"
#include "UI/HUD/Match/KillLogWidget.h"
#include "UI/HUD/Player/PlayerVitalsWidget.h"
#include "UI/Pandora/SelectPandoraWidget.h"
#include "UI/Pandora/PandoraTreeWidget.h"
#include "UI/HUD/Notification/RightNotificationsWidget.h"
#include "UI/HUD/Player/RespawnDelayWidget.h"
#include "UI/Match/GameResultWidget.h"
#include "UI/Core/WidgetClassDefinition.h"
#include "UI/Common/WidgetLookup.h"

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
	PossessedCharacterReadySubscription.SubscribeToPossessedCharacter(GetOwningPlayerController(),
		FPdAbilitySystemReadyDelegate::FDelegate::CreateUObject(this, &ThisClass::HandlePossessedCharacterReady));
	BindPresentationEvents();
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);
}

void APdHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	PossessedCharacterReadySubscription.Reset();
	UnbindPresentationEvents();
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
	UHudUiRouter* Router = EnsureUiRouter();
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

void APdHUD::CreateAllUi()
{
	if (UHudUiRouter* Router = EnsureUiRouter())
	{
		Router->EnsureCoreLayers();
	}
}

void APdHUD::OpenInfoUiFocused(const EInfoUiSection Section)
{
	EnsureUiRouter();
	UHudScreenLayer* ScreenLayer = GetScreenLayer();
	if (!ScreenLayer)
	{
		return;
	}

	const bool bWasInfoReadyForSectionChange =
		CachedInfoUI
		&& ScreenLayer->IsInfoOpen()
		&& !ScreenLayer->IsInfoClosing()
		&& !IsEscapeMenuOpen();
	if (bWasInfoReadyForSectionChange
		&& CachedInfoUI->GetFocusedSection() == Section)
	{
		ScreenLayer->CloseInfo();
		return;
	}

	if (!bWasInfoReadyForSectionChange)
	{
		ScreenLayer->OpenInfo(Section);
	}

	if (CachedInfoUI && ScreenLayer->IsInfoOpen())
	{
		CachedInfoUI->FocusSection(Section, bWasInfoReadyForSectionChange);
	}
}

void APdHUD::CloseInfoUi()
{
	if (UHudScreenLayer* ScreenLayer = GetScreenLayer())
	{
		ScreenLayer->CloseInfo();
	}
}

void APdHUD::ToggleInfoUi()
{
	EnsureUiRouter();
	if (UHudScreenLayer* ScreenLayer = GetScreenLayer())
	{
		ScreenLayer->ToggleInfo();
	}
}

void APdHUD::OpenPandoraTreeUi()
{
	EnsureUiRouter();
	if (UHudScreenLayer* ScreenLayer = GetScreenLayer())
	{
		ScreenLayer->OpenPandoraTree();
	}
}

void APdHUD::ClosePandoraTreeUi()
{
	if (UHudScreenLayer* ScreenLayer = GetScreenLayer())
	{
		ScreenLayer->ClosePandoraTree();
	}
}

void APdHUD::TogglePandoraTreeUi()
{
	EnsureUiRouter();
	if (UHudScreenLayer* ScreenLayer = GetScreenLayer())
	{
		ScreenLayer->TogglePandoraTree();
	}
}

bool APdHUD::IsPlayerHudSuppressedByUi() const
{
	const UHudScreenLayer* ScreenLayer = GetScreenLayer();
	const UHudScoreboardLayer* ScoreboardLayer = GetScoreboardLayer();
	return (ScreenLayer && ScreenLayer->ShouldSuppressPlayerHud())
		|| (SelectPandoraScreen && SelectPandoraScreen->IsActivated())
		|| IsEscapeMenuOpen()
		|| (ScoreboardLayer && ScoreboardLayer->IsOpen());
}

bool APdHUD::IsSelectPandoraUiOpen() const
{
	return SelectPandoraScreen && SelectPandoraScreen->IsActivated();
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

    if (IsSelectPandoraUiOpen()) return;
    SelectPandoraScreen = UUiScreen::CreateBlocking(GetOwningPlayerController(), CachedSelectPandoraUI, nullptr,
        FSimpleDelegate::CreateWeakLambda(this, [this]() { CloseSelectPandoraUiInternal(false); }), ECommonInputMode::All);
    GetOwningPlayerController()->GetLocalPlayer()->GetSubsystem<UUiSubsystem>()->PushScreen(SelectPandoraScreen, EUiScreenLayer::Overlay);
    int32 Width = 0, Height = 0;
    GetOwningPlayerController()->GetViewportSize(Width, Height);
    GetOwningPlayerController()->SetMouseLocation(Width / 2, Height / 2);
    SetActorTickEnabled(true);
    RefreshPlayerHudVisibility();
}

bool APdHUD::CloseSelectPandoraUi()
{
	return CloseSelectPandoraUiInternal(true);
}

bool APdHUD::CloseSelectPandoraUiInternal(const bool bCommitSelection)
{
	if (!IsSelectPandoraUiOpen()) return false;
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

    if (SelectPandoraScreen)
    {
        SelectPandoraScreen->DeactivateWidget();
        SelectPandoraScreen = nullptr;
    }
    SetActorTickEnabled(false);
    RefreshPlayerHudVisibility();
	return bSelectionWouldChangeLoadout;
}

void APdHUD::UpdateSelectPandoraDirectionFromMouse()
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller || !CachedSelectPandoraUI || !IsSelectPandoraUiOpen() || !WidgetClassDefinition)
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
	if (UHudUiRouter* Router = EnsureUiRouter())
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

void APdHUD::BindPresentationEvents()
{
	if (APdPlayerController* Controller = Cast<APdPlayerController>(GetOwningPlayerController()))
	{
		PresentationController = Controller;
		Controller->OnAimCrosshairChanged().AddUObject(this, &ThisClass::HandleAimCrosshairChanged);
		Controller->OnDamageScreenEffectRequested().AddUObject(this, &ThisClass::ShowDamageScreenEffect);
		if (UControllerPresentationComponent* Presentation = Controller->GetControllerPresentationComponent())
		{
			Presentation->OnRightNotificationRequested().AddUObject(this, &ThisClass::ShowRightNotification);
			Presentation->OnKillLogEntryRequested().AddUObject(this, &ThisClass::AddKillLogEntry);
			Presentation->OnGoldenKillAnnouncementRequested().AddUObject(this, &ThisClass::ShowGoldenKillAnnouncement);
			Presentation->OnRespawnDelayChanged().AddUObject(this, &ThisClass::HandleRespawnDelayChanged);
			Presentation->OnInGameScoreboardChanged().AddUObject(this, &ThisClass::HandleInGameScoreboardChanged);
		}
	}

	// 클라이언트에서는 GameState가 HUD보다 늦게 복제될 수 있어, 아직 없으면 생길 때 구독한다.
	UWorld* World = GetWorld();
	if (World && World->GetGameState())
	{
		BindGameStateEvents(World->GetGameState());
	}
	else if (World)
	{
		World->GameStateSetEvent.AddUObject(this, &ThisClass::BindGameStateEvents);
	}
}

void APdHUD::BindGameStateEvents(AGameStateBase* GameState)
{
	AExperienceGameState* ExperienceGameState = Cast<AExperienceGameState>(GameState);
	if (!ExperienceGameState || PresentationGameState.Get() == ExperienceGameState)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GameStateSetEvent.RemoveAll(this);
	}
	PresentationGameState = ExperienceGameState;
	ExperienceGameState->OnMatchTimerChanged().AddUObject(this, &ThisClass::RefreshHudTimerVisibility);
	ExperienceGameState->OnGameResultReceived().AddUObject(this, &ThisClass::ShowGameResult);
}

void APdHUD::UnbindPresentationEvents()
{
	// HUD가 구독한 알림은 이 HUD 객체 기준으로 한 번에 해제한다.
	if (APdPlayerController* Controller = PresentationController.Get())
	{
		Controller->OnAimCrosshairChanged().RemoveAll(this);
		Controller->OnDamageScreenEffectRequested().RemoveAll(this);
		if (UControllerPresentationComponent* Presentation = Controller->GetControllerPresentationComponent())
		{
			Presentation->OnRightNotificationRequested().RemoveAll(this);
			Presentation->OnKillLogEntryRequested().RemoveAll(this);
			Presentation->OnGoldenKillAnnouncementRequested().RemoveAll(this);
			Presentation->OnRespawnDelayChanged().RemoveAll(this);
			Presentation->OnInGameScoreboardChanged().RemoveAll(this);
		}
	}
	if (AExperienceGameState* ExperienceGameState = PresentationGameState.Get())
	{
		ExperienceGameState->OnMatchTimerChanged().RemoveAll(this);
		ExperienceGameState->OnGameResultReceived().RemoveAll(this);
	}
	if (UWorld* World = GetWorld())
	{
		World->GameStateSetEvent.RemoveAll(this);
	}

	PresentationController.Reset();
	PresentationGameState.Reset();
}

void APdHUD::HandleAimCrosshairChanged(const bool bVisible, const FGameplayTag CrosshairWidgetTag)
{
	if (bVisible)
	{
		ShowAimCrosshair(CrosshairWidgetTag);
	}
	else
	{
		HideAimCrosshair();
	}
}

// 결과 창은 나가기 전까지 닫히지 않는다.
void APdHUD::ShowGameResult(const FText& WinnerTitle, const int32 WinnerTeamColorIndex, const FText& MaxKillerName,
	const int32 MaxKillCount, const TArray<FGameResultPlayerStat>& PlayerStats)
{
	const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this);
	const TSubclassOf<UGameResultWidget> GameResultWidgetClass = WidgetDefinition ? WidgetDefinition->GetGameResultWidgetClass() : nullptr;
	APlayerController* Controller = GetOwningPlayerController();
	UGameResultWidget* GameResultWidget =
		GameResultWidgetClass && Controller ? CreateWidget<UGameResultWidget>(Controller, GameResultWidgetClass) : nullptr;
	if (!GameResultWidget)
	{
		return;
	}

	GameResultWidget->SetInfo(WinnerTitle, WinnerTeamColorIndex, MaxKillerName, MaxKillCount, PlayerStats);
	GameResultWidget->SetCloseOnlyOnExit(true);
	GameResultWidget->ShowResultScreen();
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

void APdHUD::HandleRespawnDelayChanged(const bool bVisible, const float DelaySeconds)
{
	if (!bVisible)
	{
		if (URespawnDelayWidget* RespawnDelayWidget = FindRespawnDelayWidget())
		{
			RespawnDelayWidget->HideRespawnDelay();
		}
		return;
	}

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!CachedPlayerHUD)
	{
		CreateAllUi();
	}

	if (URespawnDelayWidget* RespawnDelayWidget = FindRespawnDelayWidget())
	{
		RespawnDelayWidget->StartRespawnDelay(DelaySeconds);
	}
}

void APdHUD::HandleInGameScoreboardChanged(const bool bVisible)
{
	if (!bVisible)
	{
		if (UHudScoreboardLayer* ScoreboardLayer = GetScoreboardLayer())
		{
			ScoreboardLayer->Hide();
		}
		return;
	}

	EnsureUiRouter();
	if (UHudScoreboardLayer* ScoreboardLayer = GetScoreboardLayer())
	{
		ScoreboardLayer->Show();
	}
}

void APdHUD::OpenEscapeMenu()
{
	EnsureUiRouter();
	if (UHudMenuLayer* MenuLayer = GetMenuLayer())
	{
		MenuLayer->Open();
	}
}

void APdHUD::ToggleEscapeMenu()
{
	EnsureUiRouter();
	if (UHudMenuLayer* MenuLayer = GetMenuLayer())
	{
		MenuLayer->Toggle();
	}
}

bool APdHUD::HandleEscapeInput()
{
	if (UHudMenuLayer* MenuLayer = GetMenuLayer(); MenuLayer && MenuLayer->IsOpen())
	{
		MenuLayer->Close();
		return true;
	}

	if (SelectPandoraScreen && SelectPandoraScreen->IsActivated())
	{
		CloseSelectPandoraUiInternal(false);
		return true;
	}

	const UHudScreenLayer* ScreenLayer = GetScreenLayer();
	if (CachedPandoraTreeUI && CachedPandoraTreeUI->IsPandoraTreeShown())
	{
		if (!ScreenLayer || !ScreenLayer->IsPandoraTreeClosing())
		{
			ClosePandoraTreeUi();
		}
		return true;
	}

	if (CachedInfoUI && ScreenLayer && ScreenLayer->IsInfoOpen())
	{
		if (!ScreenLayer->IsInfoClosing())
		{
			CloseInfoUi();
		}
		return true;
	}

	OpenEscapeMenu();
	return IsEscapeMenuOpen();
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
	ToggleEscapeMenu();
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

UHudUiRouter* APdHUD::EnsureUiRouter()
{
	if (!UiRouter)
	{
		UiRouter = NewObject<UHudUiRouter>(this);
		if (UiRouter)
		{
			UiRouter->Initialize(this);
		}
	}
	return UiRouter;
}

UHudScreenLayer* APdHUD::GetScreenLayer() const
{
	return UiRouter ? UiRouter->GetScreenLayer() : nullptr;
}

UHudMenuLayer* APdHUD::GetMenuLayer() const
{
	return UiRouter ? UiRouter->GetMenuLayer() : nullptr;
}

UHudScoreboardLayer* APdHUD::GetScoreboardLayer() const
{
	return UiRouter ? UiRouter->GetScoreboardLayer() : nullptr;
}

bool APdHUD::IsEscapeMenuOpen() const
{
	const UHudMenuLayer* MenuLayer = GetMenuLayer();
	return MenuLayer && MenuLayer->IsOpen();
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

// 플레이어 HUD를 만들 때와 조종 캐릭터의 ASC가 준비될 때 호출된다. ASC가 아직이면 준비 알림에서 다시 적용한다.
bool APdHUD::ApplyStatusViewModelToPlayerHud()
{
	if (!CachedPlayerHUD)
	{
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
	return bAppliedViewModel;
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

// 플레이어 HUD에는 아래 위젯들을 종류별로 하나씩만 배치하므로 이름이 아니라 타입으로 찾는다.
UDamageScreenEffectWidget* APdHUD::FindDamageScreenEffectWidget()
{
	if (!CachedDamageScreenEffectWidget)
	{
		CachedDamageScreenEffectWidget = PdWidgetLookup::FindFirstWidgetOfType<UDamageScreenEffectWidget>(CachedPlayerHUD);
	}
	return CachedDamageScreenEffectWidget;
}

UGoldenKillAnnouncementWidget* APdHUD::FindGoldenKillAnnouncementWidget()
{
	if (!CachedGoldenKillAnnouncementWidget)
	{
		CachedGoldenKillAnnouncementWidget = PdWidgetLookup::FindFirstWidgetOfType<UGoldenKillAnnouncementWidget>(CachedPlayerHUD);
	}
	return CachedGoldenKillAnnouncementWidget;
}

UKillLogWidget* APdHUD::FindKillLogWidget()
{
	if (!CachedKillLogWidget)
	{
		CachedKillLogWidget = PdWidgetLookup::FindFirstWidgetOfType<UKillLogWidget>(CachedPlayerHUD);
	}
	return CachedKillLogWidget;
}

UHudTimerWidget* APdHUD::FindHudTimerWidget()
{
	if (!CachedHudTimerWidget)
	{
		CachedHudTimerWidget = PdWidgetLookup::FindFirstWidgetOfType<UHudTimerWidget>(CachedPlayerHUD);
	}
	return CachedHudTimerWidget;
}

URespawnDelayWidget* APdHUD::FindRespawnDelayWidget()
{
	if (!CachedRespawnDelayWidget)
	{
		CachedRespawnDelayWidget = PdWidgetLookup::FindFirstWidgetOfType<URespawnDelayWidget>(CachedPlayerHUD);
	}
	return CachedRespawnDelayWidget;
}

bool APdHUD::IsTrainingRoomMap() const
{
	return ULevelDefinition::IsTrainingRoomWorld(this);
}

void APdHUD::RefreshTrainingRoomUiPause(const UUserWidget* IgnoredWidget)
{
	if (UHudScreenLayer* ScreenLayer = GetScreenLayer())
	{
		ScreenLayer->RefreshTrainingRoomPause(IgnoredWidget);
	}
}

bool APdHUD::ShouldSuppressHudTimer()
{
	const UHudTimerWidget* HudTimerWidget = FindHudTimerWidget();
	return HudTimerWidget && HudTimerWidget->ShouldSuppressTimer();
}

void APdHUD::RefreshHudTimerVisibility()
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

void APdHUD::HandlePossessedCharacterReady(ACharacterBase* Character, UPdAbilitySystemComponent* AbilitySystemComponent)
{
	static_cast<void>(Character);
	static_cast<void>(AbilitySystemComponent);
	ApplyStatusViewModelToPlayerHud();
}

void APdHUD::HandleEscapeMenuClosed()
{
	RefreshTrainingRoomUiPause();
	RefreshPlayerHudVisibility();
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
	PdEditorTransaction::ResetIfContainsPieObjects();

	HideAimCrosshair();
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

	PdEditorTransaction::ResetIfContainsPieObjects();
}

// 화면 입력 라우터가 게임플레이 입력을 막고 있는지 입력 컴포넌트에 알려 준다.
bool APdHUD::IsGameplayInputBlockedByUi() const
{
	const APlayerController* Controller = GetOwningPlayerController();
	const ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
	const UPdUIActionRouter* Router = LocalPlayer ? LocalPlayer->GetSubsystem<UPdUIActionRouter>() : nullptr;
	return Router && Router->IsGameplayInputBlocked();
}
