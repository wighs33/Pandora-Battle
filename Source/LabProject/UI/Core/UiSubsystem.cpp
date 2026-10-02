#include "UI/Core/UiSubsystem.h"
#include "UI/Core/UiLayerRoot.h"
#include "UI/Core/UiScreen.h"
#include "UI/Settings/GameSettingsWidget.h"
#include "Input/CommonUIActionRouterBase.h"
#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "Settings/GameSettingsSubsystem.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Lobby/Contents/LobbyHUD.h"
#include "UI/Lobby/LobbyWidget.h"
#include "Mode/PdHUD.h"
#include "Mode/ExperienceGameState.h"
#include "Character/CharacterBase.h"
#include "Component/Experience/ExperienceManagerComponent.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UI/Core/ConnectingPopupWidget.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Mode/PdPlayerState.h"
#include "ShaderPipelineCache.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "Data/ContentLease.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"
#include "ViewModel/StatusViewModel.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(UiSubsystem)

DEFINE_LOG_CATEGORY(PdUiSubsystemLog);

namespace
{
	void ReleaseUiStreamableHandle(TSharedPtr<FStreamableHandle>& Handle)
	{
		if (Handle.IsValid())
		{
			Handle->CancelHandle();
			Handle->ReleaseHandle();
			Handle.Reset();
		}
	}
}

void UUiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bIsDeinitializing = false;
	bHasExternalWidgetClassDefinition = false;
	bConfiguredWidgetContentPreloadPending = false;
	bConfiguredWidgetContentReady = false;
	ConfiguredWidgetClassDefinition = nullptr;
	WidgetClassDefinition = nullptr;
	PendingConfiguredUiContent.Reset();
	ConfiguredCoreContentLease.Reset();
	StatusViewModel = NewObject<UStatusViewModel>(this);
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	if (UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr)
	{
		ContentSubsystem->EnsureSkillDataAssetsPreload();
	}
	ConfiguredCoreContentLease = AcquireConfiguredUiContent(
		EUiContentGroup::Core,
		FSimpleDelegate::CreateUObject(
			this,
			&ThisClass::RefreshConfiguredWidgetContentState));
	RefreshConfiguredWidgetContentState();
	if (GameInstance)
		PreClientTravelHandle = GameInstance->OnNotifyPreClientTravel().AddUObject(this, &ThisClass::HandlePreClientTravel);
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::HandlePreLoadMap);
	SeamlessTravelHandle = FWorldDelegates::OnSeamlessTravelStart.AddUObject(this, &ThisClass::HandleSeamlessTravelStart);
	if (GEngine)
	{
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
	}
	if (FShaderPipelineCache::NumPrecompilesRemaining() > 0)
		ActiveWaitReasons.Add(EWaitReason::PipelineCompile);
	LoadingWorkTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::TickLoadingWork), 0.05f);
	RefreshLoadingScreen();
}

void UUiSubsystem::Deinitialize()
{
	bIsDeinitializing = true;
	CloseGameSettings();
	FTSTicker::RemoveTicker(LoadingWorkTickerHandle);
	LoadingWorkTickerHandle.Reset();
	if (UGameInstance* GI = GetLocalPlayer()->GetGameInstance())
		GI->OnNotifyPreClientTravel().Remove(PreClientTravelHandle);
	FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadMapHandle);
	FWorldDelegates::OnSeamlessTravelStart.Remove(SeamlessTravelHandle);
	if (GEngine)
	{
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	}

	if (StatusViewModel && StatusViewModel->IsViewModelInitialized())
	{
		StatusViewModel->UninitializeViewModel();
	}

	HideConnectingPopup();
	ReleaseConfiguredWidgetDefinitionPreload();
	if (ScreenRoot) ScreenRoot->RemoveFromParent();
	ScreenRoot = nullptr;
	StatusViewModel = nullptr;
	WidgetClassDefinition = nullptr;
	ConfiguredWidgetClassDefinition = nullptr;
	bHasExternalWidgetClassDefinition = false;
	Super::Deinitialize();
}

void UUiSubsystem::SetWidgetClassDefinition(UWidgetClassDefinition* InWidgetClassDefinition)
{
	if (IsValid(InWidgetClassDefinition))
	{
		bHasExternalWidgetClassDefinition = true;
		WidgetClassDefinition = InWidgetClassDefinition;
		ConnectingPopupWidgetClass = WidgetClassDefinition->GetConnectingPopupWidgetClass();
		RefreshLoadingScreen();
	}
}

void UUiSubsystem::ClearWidgetClassDefinition(
	const UWidgetClassDefinition* ExpectedWidgetClassDefinition)
{
	if (!ExpectedWidgetClassDefinition || WidgetClassDefinition == ExpectedWidgetClassDefinition)
	{
		bHasExternalWidgetClassDefinition = false;
		WidgetClassDefinition = ConfiguredWidgetClassDefinition;
		ConnectingPopupWidgetClass = WidgetClassDefinition
			? WidgetClassDefinition->GetConnectingPopupWidgetClass()
			: nullptr;
		RefreshLoadingScreen();
	}
}

void UUiSubsystem::EnsureConfiguredWidgetContentPreload()
{
	BeginConfiguredWidgetDefinitionPreload();
}

TSharedPtr<FContentLease> UUiSubsystem::AcquireUiContent(
	UWidgetClassDefinition* Definition,
	const EUiContentGroup Group,
	FSimpleDelegate OnComplete)
{
	if (!IsValid(Definition) || bIsDeinitializing)
	{
		return nullptr;
	}
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		UE_LOG(PdUiSubsystemLog, Error, TEXT("UI content preload could not start: ContentDataSubsystem is unavailable."));
		TSharedPtr<FContentLease> Lease = MakeShared<FContentLease>(MoveTemp(OnComplete));
		Lease->MarkFailed();
		return Lease;
	}
	TArray<FSoftObjectPath> AssetPaths;
	Definition->GetRuntimePreloadAssetPaths(Group, AssetPaths);
	return ContentSubsystem->AcquireContent(AssetPaths, MoveTemp(OnComplete));
}

TSharedPtr<FContentLease> UUiSubsystem::AcquireConfiguredUiContent(
	const EUiContentGroup Group,
	FSimpleDelegate OnComplete)
{
	if (bIsDeinitializing)
	{
		return nullptr;
	}
	if (ConfiguredWidgetClassDefinition)
	{
		return AcquireUiContent(ConfiguredWidgetClassDefinition, Group, MoveTemp(OnComplete));
	}

	// Definition을 기다리는 그룹 정보는 UI가 보유하고, lease에는 확정된 경로만 전달한다.
	TSharedPtr<FContentLease> Lease = MakeShared<FContentLease>(MoveTemp(OnComplete));
	PendingConfiguredUiContent.Emplace(Group, Lease);
	BeginConfiguredWidgetDefinitionPreload();
	return Lease;
}

void UUiSubsystem::BeginConfiguredWidgetDefinitionPreload()
{
	if (ConfiguredWidgetClassDefinition)
	{
		if (ConfiguredCoreContentLease.IsValid()
			&& ConfiguredCoreContentLease->HasFailed())
		{
			ConfiguredCoreContentLease.Reset();
		}
		if (!ConfiguredCoreContentLease.IsValid())
		{
			ConfiguredCoreContentLease = AcquireConfiguredUiContent(
				EUiContentGroup::Core,
				FSimpleDelegate::CreateUObject(
					this,
					&ThisClass::RefreshConfiguredWidgetContentState));
		}
		RefreshConfiguredWidgetContentState();
		return;
	}

	if (bConfiguredWidgetContentReady
		|| bConfiguredWidgetContentPreloadPending)
	{
		return;
	}

	if (DefaultWidgetClassDefinition.IsNull())
	{
		bConfiguredWidgetContentReady = false;
		FailPendingConfiguredUiContent();
		UE_LOG(
			PdUiSubsystemLog,
			Error,
			TEXT("Default WidgetClassDefinition is required but was not configured."));
		return;
	}

	bConfiguredWidgetContentReady = false;
	bConfiguredWidgetContentPreloadPending = true;

	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		UE_LOG(
			PdUiSubsystemLog,
			Error,
			TEXT("Default WidgetClassDefinition preload could not start because ContentDataSubsystem is unavailable."));
		bConfiguredWidgetContentPreloadPending = false;
		FailPendingConfiguredUiContent();
		return;
	}

	const TWeakObjectPtr<ThisClass> WeakThis(this);
	ReleaseUiStreamableHandle(ConfiguredDefinitionLoadHandle);
	ConfiguredDefinitionLoadHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
			{DefaultWidgetClassDefinition.ToSoftObjectPath()},
			FSimpleDelegate::CreateLambda(
				[WeakThis]()
				{
					if (ThisClass* This = WeakThis.Get())
					{
						This->HandleConfiguredWidgetDefinitionLoaded();
					}
				}));
}

void UUiSubsystem::HandleConfiguredWidgetDefinitionLoaded()
{
	if (bIsDeinitializing)
	{
		return;
	}

	ConfiguredWidgetClassDefinition = DefaultWidgetClassDefinition.Get();
	if (!ConfiguredWidgetClassDefinition)
	{
		bConfiguredWidgetContentPreloadPending = false;
		FailPendingConfiguredUiContent();
		UE_LOG(
			PdUiSubsystemLog,
			Error,
			TEXT("Default WidgetClassDefinition '%s' did not resolve after its asynchronous preload."),
			*DefaultWidgetClassDefinition.ToString());
		return;
	}

	// Publish the root definition as soon as it resolves. The Core lease below owns
	// only always-needed UI; Lobby/InGame/Info/Map are acquired by their screens.
	if (!bHasExternalWidgetClassDefinition)
	{
		WidgetClassDefinition = ConfiguredWidgetClassDefinition;
		ConnectingPopupWidgetClass =
			ConfiguredWidgetClassDefinition->GetConnectingPopupWidgetClass();
	}

	RefreshLoadingScreen();

	BindPendingConfiguredUiContent();
	RefreshConfiguredWidgetContentState();
}

void UUiSubsystem::BindPendingConfiguredUiContent()
{
	if (!ConfiguredWidgetClassDefinition)
	{
		return;
	}
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	auto PendingRequests = MoveTemp(PendingConfiguredUiContent);
	PendingConfiguredUiContent.Reset();
	for (const auto& Request : PendingRequests)
	{
		if (const TSharedPtr<FContentLease> Lease = Request.Value.Pin())
		{
			TArray<FSoftObjectPath> AssetPaths;
			ConfiguredWidgetClassDefinition->GetRuntimePreloadAssetPaths(Request.Key, AssetPaths);
			Lease->Start(AssetPaths, ContentSubsystem);
		}
	}
}

void UUiSubsystem::FailPendingConfiguredUiContent()
{
	auto PendingRequests = MoveTemp(PendingConfiguredUiContent);
	PendingConfiguredUiContent.Reset();
	for (const auto& Request : PendingRequests)
	{
		if (const TSharedPtr<FContentLease> Lease = Request.Value.Pin())
		{
			Lease->MarkFailed();
		}
	}
}

void UUiSubsystem::RefreshConfiguredWidgetContentState()
{
	if (!ConfiguredWidgetClassDefinition)
	{
		bConfiguredWidgetContentReady = false;
		return;
	}
	bConfiguredWidgetContentReady = ConfiguredCoreContentLease.IsValid()
		&& ConfiguredCoreContentLease->IsReady();
	bConfiguredWidgetContentPreloadPending = ConfiguredCoreContentLease.IsValid()
		&& ConfiguredCoreContentLease->IsLoading();
	RefreshLoadingScreen();
}

void UUiSubsystem::ReleaseConfiguredWidgetDefinitionPreload()
{
	bConfiguredWidgetContentPreloadPending = false;
	bConfiguredWidgetContentReady = false;
	ConfiguredCoreContentLease.Reset();
	for (const auto& Request : PendingConfiguredUiContent)
	{
		if (const TSharedPtr<FContentLease> Lease = Request.Value.Pin())
		{
			Lease->Release();
		}
	}
	PendingConfiguredUiContent.Reset();
	ReleaseUiStreamableHandle(ConfiguredDefinitionLoadHandle);
}

#if WITH_EDITOR
UWidgetClassDefinition* UUiSubsystem::LoadConfiguredEditorWidgetClassDefinition()
{
	const UUiSubsystem* DefaultSubsystem = GetDefault<UUiSubsystem>();
	return DefaultSubsystem
		? DefaultSubsystem->DefaultWidgetClassDefinition.LoadSynchronous()
		: nullptr;
}
#endif

bool UUiSubsystem::RefreshStatusViewModel()
{
	if (!StatusViewModel)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = ResolveAbilitySystemComponent();
	if (!ASC)
	{
		if (StatusViewModel->IsViewModelInitialized())
		{
			StatusViewModel->UninitializeViewModel();
		}
		return false;
	}

	StatusViewModel->InitializeViewModel(ASC);
	return StatusViewModel->IsViewModelInitialized();
}

bool UUiSubsystem::ApplyStatusViewModelToWidget(UUserWidget* InWidget)
{
	return BindStatusViewModelToWidget(InWidget)
		&& RefreshStatusViewModel();
}

bool UUiSubsystem::ApplyStatusViewModelToWidgetTree(UUserWidget* RootWidget)
{
	if (!RootWidget || !StatusViewModel)
	{
		return false;
	}

	bool bBoundAny = false;
	TSet<UUserWidget*> VisitedWidgets;
	TFunction<void(UUserWidget*)> BindWidgetTree =
		[this, &bBoundAny, &VisitedWidgets, &BindWidgetTree](
			UUserWidget* UserWidget)
	{
		if (!UserWidget || VisitedWidgets.Contains(UserWidget))
		{
			return;
		}

		VisitedWidgets.Add(UserWidget);
		bBoundAny |= BindStatusViewModelToWidget(UserWidget);
		if (!UserWidget->WidgetTree)
		{
			return;
		}

		// ForEachWidget visits nested UUserWidget objects themselves but does not
		// enter their foreign WidgetTrees. Recurse explicitly so MVVM extensions
		// attached to those user widgets are never skipped.
		UserWidget->WidgetTree->ForEachWidget(
			[&BindWidgetTree](UWidget* ChildWidget)
			{
				if (UUserWidget* ChildUserWidget =
					Cast<UUserWidget>(ChildWidget))
				{
					BindWidgetTree(ChildUserWidget);
				}
			});
	};

	BindWidgetTree(RootWidget);
	return bBoundAny && RefreshStatusViewModel();
}

bool UUiSubsystem::BindStatusViewModelToWidget(UUserWidget* InWidget)
{
	if (!InWidget || !StatusViewModel)
	{
		return false;
	}

	UMVVMView* ViewExtension = InWidget->GetExtension<UMVVMView>();
	if (!ViewExtension)
	{
		return false;
	}

	const FName ViewModelSourceName =
		ResolveStatusViewModelSourceName(InWidget);
	return !ViewModelSourceName.IsNone()
		&& ViewExtension->SetViewModel(
			ViewModelSourceName,
			StatusViewModel);
}

UConnectingPopupWidget* UUiSubsystem::ShowConnectingPopup(const bool bEnableCancelButton)
{
	APlayerController* PlayerController = GetLocalPlayerController();
	UWorld* World = IsValid(PlayerController) ? PlayerController->GetWorld() : nullptr;
	if (bIsDeinitializing || ActiveWaitReasons.IsEmpty() || !World || World->bIsTearingDown)
	{
		return nullptr;
	}

	const TSubclassOf<UConnectingPopupWidget> PopupClass = ResolveConnectingPopupWidgetClass();
	if (!PopupClass)
	{
		return nullptr;
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
		return nullptr;
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
        PushScreen(ConnectingScreen, EUiScreenLayer::Modal);
    }

	return ActiveConnectingPopupWidget;
}

void UUiSubsystem::HideConnectingPopup()
{
	UConnectingPopupWidget* PopupWidget = ActiveConnectingPopupWidget.Get();
	if (IsValid(PopupWidget))
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

UAbilitySystemComponent* UUiSubsystem::ResolveAbilitySystemComponent() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return nullptr;
	}

	const APlayerController* PlayerController = LocalPlayer->GetPlayerController(GetWorld());
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return nullptr;
	}

	const APdPlayerState* PdPlayerState = PlayerController->GetPlayerState<APdPlayerState>();
	return PdPlayerState ? PdPlayerState->GetAbilitySystemComponent() : nullptr;
}

FName UUiSubsystem::ResolveStatusViewModelSourceName(const UUserWidget* InWidget) const
{
	if (!InWidget)
	{
		return NAME_None;
	}

	const UMVVMView* ViewExtension = InWidget->GetExtension<UMVVMView>();
	const UMVVMViewClass* ViewClass = ViewExtension ? ViewExtension->GetViewClass() : nullptr;
	if (!ViewClass)
	{
		return NAME_None;
	}

	FName FirstCompatibleSourceName = NAME_None;
	for (const FMVVMViewClass_Source& Source : ViewClass->GetSources())
	{
		if (!Source.IsViewModel() || !Source.CanBeSet())
		{
			continue;
		}

		const UClass* SourceClass = Source.GetSourceClass();
		const bool bIsStatusViewModelSource = SourceClass && StatusViewModel && StatusViewModel->GetClass()->IsChildOf(SourceClass);
		if (!bIsStatusViewModelSource)
		{
			continue;
		}

		if (Source.GetName() == UStatusViewModel::ViewModelName)
		{
			return Source.GetName();
		}

		if (FirstCompatibleSourceName.IsNone())
		{
			FirstCompatibleSourceName = Source.GetName();
		}
	}

	return FirstCompatibleSourceName;
}

APlayerController* UUiSubsystem::GetLocalPlayerController() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
}

TSubclassOf<UConnectingPopupWidget> UUiSubsystem::ResolveConnectingPopupWidgetClass()
{
	if (ConnectingPopupWidgetClass)
	{
		return ConnectingPopupWidgetClass;
	}

	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		ConnectingPopupWidgetClass = WidgetDefinition->GetConnectingPopupWidgetClass();
	}

	return ConnectingPopupWidgetClass;
}

void UUiSubsystem::HandleConnectingPopupCanceled()
{
	const uint64 RequestId = CancelableSessionRequestId;
	CancelableSessionRequestId = 0;
	if (UOnlineSessionsSubsystem* Online = GetLocalPlayer()->GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>())
	{
		// Recheck the request identity: a delayed click must never cancel a later operation.
		if (RequestId && Online->GetPendingUserRequestId(GetLocalPlayer()) == RequestId
			&& Online->IsUserRequestCancelable(RequestId))
			Online->CancelSessionRequest(RequestId);
	}
	RefreshLoadingScreen();
}

void UUiSubsystem::PushScreen(UCommonActivatableWidget* Screen, EUiScreenLayer Layer)
{
    APlayerController* Controller = GetLocalPlayerController();
    if (!Controller || !Screen || bIsDeinitializing) return;
    if (!ScreenRoot || ScreenRoot->GetWorld() != Controller->GetWorld())
    {
        if (ScreenRoot) ScreenRoot->RemoveFromParent();
        ScreenRoot = CreateWidget<UUiLayerRoot>(Controller);
        ScreenRoot->AddToPlayerScreen(UUiLayerRoot::ViewportZOrder);
    }
    switch (Layer)
    {
    case EUiScreenLayer::Screen: ScreenRoot->ScreenStack->AddWidgetInstance(*Screen); break;
    case EUiScreenLayer::Menu: ScreenRoot->MenuStack->AddWidgetInstance(*Screen); break;
    case EUiScreenLayer::Modal: ScreenRoot->ModalStack->AddWidgetInstance(*Screen); break;
    case EUiScreenLayer::Overlay:
        // 선택창과 점수판을 겹쳐 표시한다. 입력 우선순위는 CommonUI가 결정한다.
        UOverlaySlot* Slot = ScreenRoot->OverlayLayer->AddChildToOverlay(Screen);
        Slot->SetHorizontalAlignment(HAlign_Fill);
        Slot->SetVerticalAlignment(VAlign_Fill);
        Screen->OnDeactivated().AddWeakLambda(Screen, [Screen]() { Screen->RemoveFromParent(); });
        Screen->ActivateWidget();
        break;
    }
}

void UUiSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
    CloseGameSettings();
    Super::PlayerControllerChanged(NewPlayerController);
    HideConnectingPopup();
    if (ScreenRoot) ScreenRoot->RemoveFromParent();
    ScreenRoot = nullptr;
    RefreshLoadingScreen();
}

void UUiSubsystem::OpenGameSettings(UUserWidget* OwnerMenu)
{
	if (bIsDeinitializing || !IsValid(OwnerMenu) || OwnerMenu->GetOwningLocalPlayer() != GetLocalPlayer()) return;
	if (ActiveGameSettings && ActiveGameSettings->GetParent())
	{
		if (UCommonActivatableWidget* Screen = UCommonUIActionRouterBase::FindOwningActivatable(
			ActiveGameSettings->GetCachedWidget(), GetLocalPlayer()))
			Screen->RequestRefreshFocus();
		return;
	}
	const UWidgetClassDefinition* Definition = UWidgetClassDefinition::ResolveWidgetClassDefinition(OwnerMenu);
	APlayerController* Controller = GetLocalPlayerController();
	if (!Definition || !Controller) return;
	const TSubclassOf<UGameSettingsWidget> SettingsClass = Definition->SettingsWidgetClass.LoadSynchronous();
	if (!SettingsClass) return;
	ActiveGameSettings = CreateWidget<UGameSettingsWidget>(Controller, SettingsClass);
	if (!ActiveGameSettings) return;
	SettingsOwner = OwnerMenu;
	UUiScreen* Screen = CreateWidget<UUiScreen>(Controller);
	FUIInputConfig Config(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	Config.bIgnoreMoveInput = Config.bIgnoreLookInput = true;
	Screen->SetContent(ActiveGameSettings, Config, EPdGameplayInputPolicy::Block,
		ActiveGameSettings->GetInitialFocusTarget(),
		FSimpleDelegate::CreateUObject(ActiveGameSettings, &UGameSettingsWidget::CloseSettings));
	PushScreen(Screen, EUiScreenLayer::Modal);
}

bool UUiSubsystem::CloseGameSettings(const UUserWidget* ExpectedOwner)
{
	if (ExpectedOwner && SettingsOwner.Get() != ExpectedOwner) return false;
	UGameSettingsWidget* Settings = ActiveGameSettings;
	ActiveGameSettings = nullptr;
	SettingsOwner.Reset();
	if (!IsValid(Settings) || !Settings->GetParent()) return false;
	Settings->CloseSettings();
	return true;
}

bool UUiSubsystem::TickLoadingWork(float)
{
	if (bIsDeinitializing) return false;
	RefreshLoadingScreen();
	return true;
}

void UUiSubsystem::RefreshLoadingScreen()
{
	if (bIsDeinitializing) return;
	const UGameInstance* GI = GetLocalPlayer()->GetGameInstance();
	const UContentDataSubsystem* Content = GI ? GI->GetSubsystem<UContentDataSubsystem>() : nullptr;
	const UGameSettingsSubsystem* Settings = GI ? GI->GetSubsystem<UGameSettingsSubsystem>() : nullptr;
	const ULobbyRuntimeSubsystem* Runtime = GI ? GI->GetSubsystem<ULobbyRuntimeSubsystem>() : nullptr;
	const UOnlineSessionsSubsystem* Online = GI ? GI->GetSubsystem<UOnlineSessionsSubsystem>() : nullptr;
	const bool bFinishingContentPSO = ActiveWaitReasons.Contains(EWaitReason::PipelineCompile);
	ActiveWaitReasons.Reset();
	CancelableSessionRequestId = 0;
	if (bConfiguredWidgetContentPreloadPending || (Content && Content->IsSkillDataAssetsLoading()))
		ActiveWaitReasons.Add(EWaitReason::StartupContent);
	if (Runtime && Runtime->IsLobbyEntryContentLoading())
		ActiveWaitReasons.Add(EWaitReason::LobbyEntryContent);
	if (Settings && Settings->IsRuntimeContentLoading())
		ActiveWaitReasons.Add(EWaitReason::StartupContent);
	if (Runtime && Runtime->GetGameEntryContentPreloadResult() == ELobbyContentPreloadResult::Loading)
		ActiveWaitReasons.Add(EWaitReason::GameEntryContent);
	if ((bTravelPending || bGameStartPreparationPending || !ActiveWaitReasons.IsEmpty() || bFinishingContentPSO)
		&& FShaderPipelineCache::NumPrecompilesRemaining() > 0)
		ActiveWaitReasons.Add(EWaitReason::PipelineCompile);
	if (Online)
	{
		const uint64 RequestId = Online->GetPendingUserRequestId(GetLocalPlayer());
		if (RequestId)
		{
			ActiveWaitReasons.Add(EWaitReason::SessionRequest);
			if (Online->IsUserRequestCancelable(RequestId)) CancelableSessionRequestId = RequestId;
		}
		if (Online->IsSessionLifecyclePending()) ActiveWaitReasons.Add(EWaitReason::SessionLifecycle);
	}
	if (bGameStartPreparationPending) ActiveWaitReasons.Add(EWaitReason::GameStartPreparation);
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
				GI->GetSubsystem<ULobbyRuntimeSubsystem>()->ReleaseLobbyEntryContentPreload();
		}
		else ActiveWaitReasons.Add(EWaitReason::Travel);
	}
	if (ActiveWaitReasons.IsEmpty()) HideConnectingPopup();
	else ShowConnectingPopup(CancelableSessionRequestId != 0
		&& !bTravelPending && !bGameStartPreparationPending);
}

void UUiSubsystem::SetGameStartPreparationPending(const bool bPending)
{
	bGameStartPreparationPending = bPending;
	RefreshLoadingScreen();
}

void UUiSubsystem::BeginTravel(UWorld* SourceWorld, const FString& URL)
{
	if (!bTravelPending)
	{
		TravelSourceWorld = SourceWorld;
		TravelDestinationMap = URL.Left(URL.Find(TEXT("?")) == INDEX_NONE ? URL.Len() : URL.Find(TEXT("?")));
		// Remote addresses do not identify a map; destination readiness still requires a new world.
		if (!TravelDestinationMap.StartsWith(TEXT("/Game/"))) TravelDestinationMap.Reset();
		bTravelPending = true;
	}
	// The host's preparation transaction now belongs to actual engine travel.
	bGameStartPreparationPending = false;
	RefreshLoadingScreen();
}

void UUiSubsystem::HandlePreClientTravel(const FString& URL, ETravelType, bool)
{
	BeginTravel(GetWorld(), URL);
}

void UUiSubsystem::HandlePreLoadMap(const FWorldContext& Context, const FString& MapName)
{
	if (Context.OwningGameInstance == GetLocalPlayer()->GetGameInstance())
		BeginTravel(Context.World(), MapName);
}

void UUiSubsystem::HandleSeamlessTravelStart(UWorld* World, const FString& URL)
{
	if (World && World->GetGameInstance() == GetLocalPlayer()->GetGameInstance())
		BeginTravel(World, URL);
}

void UUiSubsystem::AbortTravel()
{
	bTravelPending = false;
	bGameStartPreparationPending = false;
	TravelSourceWorld.Reset();
	TravelDestinationMap.Reset();
	if (ULobbyRuntimeSubsystem* Runtime = GetLocalPlayer()->GetGameInstance()->GetSubsystem<ULobbyRuntimeSubsystem>())
		Runtime->CancelGameEntryContentPreload();
	RefreshLoadingScreen();
}

void UUiSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type, const FString&)
{
	if (World && World->GetGameInstance() == GetLocalPlayer()->GetGameInstance()) AbortTravel();
}

void UUiSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver*, ENetworkFailure::Type, const FString&)
{
	if (World && World->GetGameInstance() == GetLocalPlayer()->GetGameInstance()) AbortTravel();
}

bool UUiSubsystem::IsDestinationPresentationReady() const
{
	APlayerController* PC = GetLocalPlayerController();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!World || World == TravelSourceWorld.Get() || World->bIsTearingDown
		|| !World->HasBegunPlay() || World->IsInSeamlessTravel()) return false;
	if (!TravelDestinationMap.IsEmpty()
		&& !World->GetMapName().EndsWith(FPackageName::GetShortName(TravelDestinationMap))) return false;
	// A failed Experience is terminal too: leave its existing error UI accessible.
	if (const ALobbyGameState* Lobby = World->GetGameState<ALobbyGameState>())
	{
		if (Lobby->HasExperienceLoadFailed()) return true;
		const ALobbyHUD* HUD = PC->GetHUD<ALobbyHUD>();
		return HUD && IsValid(HUD->GetLobbyWidget()) && HUD->GetLobbyWidget()->GetParent()
			&& Lobby->IsSelectedMapImageReady() && Lobby->GetExperienceManagerComponent()->IsExperienceLoaded();
	}
	if (const AExperienceGameState* GameState = World->GetGameState<AExperienceGameState>())
	{
		const UExperienceManagerComponent* Experience = GameState->GetExperienceManagerComponent();
		if (Experience->HasExperienceLoadFailed()) return true;
		const APdHUD* HUD = PC->GetHUD<APdHUD>();
		return Experience->IsExperienceLoaded() && Cast<ACharacterBase>(PC->GetPawn())
			&& PC->GetPlayerState<APdPlayerState>() && HUD && HUD->GetPlayerHudWidget();
	}
	// Title and Room List do not require a character pawn. Their real CommonUI screen must exist.
	return ScreenRoot && ScreenRoot->GetWorld() == World && ScreenRoot->ScreenStack
		&& ScreenRoot->ScreenStack->GetActiveWidget();
}
