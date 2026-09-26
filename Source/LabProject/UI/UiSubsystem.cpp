#include "UI/UiSubsystem.h"
#include "UI/PdUIActionRouter.h"
#include "UI/UiLayerRoot.h"
#include "UI/UiScreen.h"
#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StreamableManager.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/PlayerController.h"
#include "Lobby/UI/ConnectingPopupWidget.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Mode/PdPlayerState.h"
#include "ShaderPipelineCache.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "UI/WidgetContentBundleLease.h"
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
	bTravelLoadingScreenActive = false;
	bTravelLoadingScreenCancelEnabled = false;
	bStartupLoadingScreenPending = false;
	ConfiguredWidgetClassDefinition = nullptr;
	WidgetClassDefinition = nullptr;
	PendingConfiguredWidgetContentBundleLeases.Reset();
	ConfiguredCoreBundleLease.Reset();
	StatusViewModel = NewObject<UStatusViewModel>(this);
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	if (UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr)
	{
		ContentSubsystem->EnsureSkillDataAssetsPreload();
	}
	BeginConfiguredWidgetDefinitionPreload();
	ConfiguredCoreBundleLease = AcquireConfiguredWidgetContentBundle(
		EWidgetContentBundle::Core,
		FSimpleDelegate::CreateUObject(
			this,
			&ThisClass::RefreshConfiguredWidgetContentState));
	RefreshConfiguredWidgetContentState();
	BeginStartupLoadingScreen();
}

void UUiSubsystem::Deinitialize()
{
	bIsDeinitializing = true;
	CancelStartupLoadingScreenReadyCheck();

	if (StatusViewModel && StatusViewModel->IsViewModelInitialized())
	{
		StatusViewModel->UninitializeViewModel();
	}

	bTravelLoadingScreenActive = false;
	bTravelLoadingScreenCancelEnabled = false;
	HideConnectingPopup();
	ReleaseConfiguredWidgetDefinitionPreload();
	if (ScreenRoot) ScreenRoot->RemoveFromParent();
	ScreenRoot = nullptr;
	ModalInputStack.Reset();
	InputStateBeforeModals.Reset();
	RestorePolicyAfterModals = EUiInputRestorePolicy::PreviousState;
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
		if (bTravelLoadingScreenActive)
		{
			ShowConnectingPopup(bTravelLoadingScreenCancelEnabled);
		}
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
		if (bTravelLoadingScreenActive)
		{
			ShowConnectingPopup(bTravelLoadingScreenCancelEnabled);
		}
	}
}

void UUiSubsystem::EnsureConfiguredWidgetContentPreload()
{
	BeginConfiguredWidgetDefinitionPreload();
}

bool UUiSubsystem::IsStartupContentReady() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UGameInstance* GameInstance =
		LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	const UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	return bConfiguredWidgetContentReady
		&& ContentSubsystem
		&& ContentSubsystem->IsSkillDataAssetsReady();
}

TSharedPtr<FWidgetContentBundleLease> UUiSubsystem::AcquireWidgetContentBundle(
	UWidgetClassDefinition* Definition,
	const EWidgetContentBundle Bundle,
	FSimpleDelegate OnComplete)
{
	if (!IsValid(Definition) || bIsDeinitializing)
	{
		return nullptr;
	}

	TSharedPtr<FWidgetContentBundleLease> Lease = MakeShareable(
		new FWidgetContentBundleLease(Bundle, MoveTemp(OnComplete)));
	StartWidgetContentBundleLease(Lease, Definition);
	return Lease;
}

TSharedPtr<FWidgetContentBundleLease>
UUiSubsystem::AcquireConfiguredWidgetContentBundle(
	const EWidgetContentBundle Bundle,
	FSimpleDelegate OnComplete)
{
	if (bIsDeinitializing)
	{
		return nullptr;
	}

	TSharedPtr<FWidgetContentBundleLease> Lease = MakeShareable(
		new FWidgetContentBundleLease(Bundle, MoveTemp(OnComplete)));
	if (ConfiguredWidgetClassDefinition)
	{
		StartWidgetContentBundleLease(Lease, ConfiguredWidgetClassDefinition);
	}
	else
	{
		PendingConfiguredWidgetContentBundleLeases.Add(Lease);
		BeginConfiguredWidgetDefinitionPreload();
	}
	return Lease;
}

void UUiSubsystem::BeginConfiguredWidgetDefinitionPreload()
{
	if (ConfiguredWidgetClassDefinition)
	{
		if (ConfiguredCoreBundleLease.IsValid()
			&& ConfiguredCoreBundleLease->GetState()
				== EWidgetContentBundleState::Failed)
		{
			ConfiguredCoreBundleLease.Reset();
		}
		if (!ConfiguredCoreBundleLease.IsValid())
		{
			ConfiguredCoreBundleLease = AcquireConfiguredWidgetContentBundle(
				EWidgetContentBundle::Core,
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
		FailPendingConfiguredWidgetContentBundleLeases();
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
		FailPendingConfiguredWidgetContentBundleLeases();
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
		FailPendingConfiguredWidgetContentBundleLeases();
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

	if (bTravelLoadingScreenActive)
	{
		ShowConnectingPopup(bTravelLoadingScreenCancelEnabled);
	}

	BindPendingConfiguredWidgetContentBundleLeases();
	RefreshConfiguredWidgetContentState();
}

void UUiSubsystem::BindPendingConfiguredWidgetContentBundleLeases()
{
	if (!ConfiguredWidgetClassDefinition)
	{
		return;
	}

	TArray<TWeakPtr<FWidgetContentBundleLease>> PendingLeases =
		MoveTemp(PendingConfiguredWidgetContentBundleLeases);
	PendingConfiguredWidgetContentBundleLeases.Reset();
	for (const TWeakPtr<FWidgetContentBundleLease>& WeakLease : PendingLeases)
	{
		if (const TSharedPtr<FWidgetContentBundleLease> Lease = WeakLease.Pin())
		{
			StartWidgetContentBundleLease(Lease, ConfiguredWidgetClassDefinition);
		}
	}
}

void UUiSubsystem::FailPendingConfiguredWidgetContentBundleLeases()
{
	TArray<TWeakPtr<FWidgetContentBundleLease>> PendingLeases =
		MoveTemp(PendingConfiguredWidgetContentBundleLeases);
	PendingConfiguredWidgetContentBundleLeases.Reset();
	for (const TWeakPtr<FWidgetContentBundleLease>& WeakLease : PendingLeases)
	{
		if (const TSharedPtr<FWidgetContentBundleLease> Lease = WeakLease.Pin())
		{
			Lease->MarkFailed();
		}
	}
}

void UUiSubsystem::StartWidgetContentBundleLease(
	const TSharedPtr<FWidgetContentBundleLease>& Lease,
	UWidgetClassDefinition* Definition)
{
	if (!Lease.IsValid())
	{
		return;
	}
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	Lease->Start(Definition, ContentSubsystem);
}

void UUiSubsystem::RefreshConfiguredWidgetContentState()
{
	if (!ConfiguredWidgetClassDefinition)
	{
		bConfiguredWidgetContentReady = false;
		return;
	}
	const EWidgetContentBundleState CoreState = ConfiguredCoreBundleLease.IsValid()
		? ConfiguredCoreBundleLease->GetState()
		: EWidgetContentBundleState::Unloaded;
	bConfiguredWidgetContentReady = CoreState == EWidgetContentBundleState::Ready;
	bConfiguredWidgetContentPreloadPending = !bConfiguredWidgetContentReady
		&& CoreState == EWidgetContentBundleState::Loading;
	if (bConfiguredWidgetContentReady && bTravelLoadingScreenActive)
	{
		ShowConnectingPopup(bTravelLoadingScreenCancelEnabled);
	}
}

void UUiSubsystem::BeginStartupLoadingScreen()
{
	bStartupLoadingScreenPending = true;
	bTravelLoadingScreenActive = true;
	bTravelLoadingScreenCancelEnabled = false;
	ShowConnectingPopup(false);

	if (!StartupLoadingScreenReadyTickerHandle.IsValid())
	{
		StartupLoadingScreenReadyTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(
				this,
				&ThisClass::TickStartupLoadingScreenReady),
			0.05f);
	}
}

void UUiSubsystem::CancelStartupLoadingScreenReadyCheck()
{
	bStartupLoadingScreenPending = false;
	if (StartupLoadingScreenReadyTickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(StartupLoadingScreenReadyTickerHandle);
		StartupLoadingScreenReadyTickerHandle.Reset();
	}
}

bool UUiSubsystem::TickStartupLoadingScreenReady(float)
{
	if (bIsDeinitializing || !bStartupLoadingScreenPending)
	{
		StartupLoadingScreenReadyTickerHandle.Reset();
		return false;
	}

	if (!IsValid(ActiveConnectingPopupWidget))
	{
		ShowConnectingPopup(false);
	}

	if (!IsStartupContentReady())
	{
		return true;
	}

	// 시작 콘텐츠에서 요청한 PSO가 준비될 때까지 기존 티커로 확인한다.
	if (FShaderPipelineCache::NumPrecompilesRemaining() > 0)
	{
		return true;
	}

	bStartupLoadingScreenPending = false;
	StartupLoadingScreenReadyTickerHandle.Reset();
	HideTravelLoadingScreen();
	return false;
}

void UUiSubsystem::ReleaseConfiguredWidgetDefinitionPreload()
{
	bConfiguredWidgetContentPreloadPending = false;
	bConfiguredWidgetContentReady = false;
	ConfiguredCoreBundleLease.Reset();
	for (const TWeakPtr<FWidgetContentBundleLease>& WeakLease :
		PendingConfiguredWidgetContentBundleLeases)
	{
		if (const TSharedPtr<FWidgetContentBundleLease> Lease = WeakLease.Pin())
		{
			Lease->Release();
		}
	}
	PendingConfiguredWidgetContentBundleLeases.Reset();
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

FGuid UUiSubsystem::AcquireModalInput(
	UObject* Owner,
	UWidget* FocusWidget,
	const FUiModalInputConfig& InputConfig)
{
	if (bIsDeinitializing || !IsValid(Owner))
	{
		return FGuid();
	}

	PruneInvalidModalInputs();

	if (ModalInputStack.IsEmpty())
	{
		APlayerController* PlayerController = GetLocalPlayerController();
		if (!CaptureInputState(PlayerController, InputStateBeforeModals))
		{
			return FGuid();
		}
		RestorePolicyAfterModals = InputConfig.RestorePolicy;
	}

	FModalInputEntry& Entry = ModalInputStack.AddDefaulted_GetRef();
	Entry.Token = FGuid::NewGuid();
	Entry.Owner = Owner;
	Entry.FocusWidget = IsValid(FocusWidget) ? FocusWidget : nullptr;
	const APlayerController* PlayerController = GetLocalPlayerController();
	Entry.World = PlayerController ? PlayerController->GetWorld() : nullptr;
	Entry.InputConfig = InputConfig;
	Entry.bTracksFocusWidgetLifetime = IsValid(FocusWidget);

	ApplyTopModalInput();
	return Entry.Token;
}

bool UUiSubsystem::UpdateModalInput(
	UObject* Owner,
	const FGuid Token,
	UWidget* FocusWidget,
	const FUiModalInputConfig& InputConfig)
{
	if (!IsValid(Owner) || !Token.IsValid())
	{
		return false;
	}

	PruneInvalidModalInputs();

	const int32 EntryIndex = ModalInputStack.IndexOfByPredicate(
		[Owner, &Token](const FModalInputEntry& Entry)
		{
			return Entry.Token == Token && Entry.Owner.Get() == Owner;
		});
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	FModalInputEntry& Entry = ModalInputStack[EntryIndex];
	Entry.FocusWidget = IsValid(FocusWidget) ? FocusWidget : nullptr;
	Entry.InputConfig = InputConfig;
	Entry.bTracksFocusWidgetLifetime = IsValid(FocusWidget);

	if (EntryIndex == 0)
	{
		RestorePolicyAfterModals = InputConfig.RestorePolicy;
	}

	if (EntryIndex == ModalInputStack.Num() - 1)
	{
		ApplyTopModalInput();
	}

	return true;
}

bool UUiSubsystem::ReleaseModalInput(UObject* Owner, const FGuid Token)
{
	if (!Owner || !Token.IsValid())
	{
		return false;
	}

	PruneInvalidModalInputs();
	return ReleaseModalInputInternal(Owner, Token, true);
}

void UUiSubsystem::ReleaseModalInputsForOwner(UObject* Owner)
{
	if (!Owner)
	{
		return;
	}

	PruneInvalidModalInputs();

	const FGuid PreviousTopToken =
		ModalInputStack.IsEmpty() ? FGuid() : ModalInputStack.Last().Token;
	const int32 RemovedCount = ModalInputStack.RemoveAll(
		[Owner](const FModalInputEntry& Entry)
		{
			return Entry.Owner.Get() == Owner;
		});
	if (RemovedCount == 0)
	{
		return;
	}

	if (ModalInputStack.IsEmpty())
	{
		RestoreInputStateAfterLastModal();
	}
	else
	{
		RefreshRestorePolicyFromBottomModal();
		if (ModalInputStack.Last().Token != PreviousTopToken)
		{
			ApplyTopModalInput();
		}
	}
}

bool UUiSubsystem::HasActiveModalInput()
{
	PruneInvalidModalInputs();
	return !ModalInputStack.IsEmpty();
}

UConnectingPopupWidget* UUiSubsystem::ShowConnectingPopup(const bool bEnableCancelButton)
{
	APlayerController* PlayerController = GetLocalPlayerController();
	if (!PlayerController)
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
        ConnectingScreen->SetContent(ActiveConnectingPopupWidget, Config, ActiveConnectingPopupWidget,
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
        ConnectingScreen->RemoveFromParent();
        ConnectingScreen = nullptr;
    }
	ActiveConnectingPopupWidget = nullptr;
}

UConnectingPopupWidget* UUiSubsystem::ShowTravelLoadingScreen(
	const bool bEnableCancelButton)
{
	CancelStartupLoadingScreenReadyCheck();
	bTravelLoadingScreenActive = true;
	bTravelLoadingScreenCancelEnabled = bEnableCancelButton;
	return ShowConnectingPopup(bEnableCancelButton);
}

UConnectingPopupWidget* UUiSubsystem::ShowLobbyEntryLoadingScreen(
	const bool bEnableCancelButton)
{
	EnsureConfiguredWidgetContentPreload();
	UConnectingPopupWidget* LoadingScreen =
		ShowTravelLoadingScreen(bEnableCancelButton);
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	if (ULobbyRuntimeSubsystem* LobbyRuntimeSubsystem =
		GameInstance ? GameInstance->GetSubsystem<ULobbyRuntimeSubsystem>() : nullptr)
	{
		LobbyRuntimeSubsystem->BeginLobbyEntryContentPreload();
	}
	return LoadingScreen;
}

void UUiSubsystem::HideTravelLoadingScreen()
{
	CancelStartupLoadingScreenReadyCheck();
	bTravelLoadingScreenActive = false;
	bTravelLoadingScreenCancelEnabled = false;
	HideConnectingPopup();
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

bool UUiSubsystem::CaptureInputState(APlayerController* Controller, FInputStateSnapshot& OutSnapshot) const
{
    if (!Controller) return false;
    OutSnapshot = BaseInputState;
    OutSnapshot.PlayerController = Controller;
    OutSnapshot.World = Controller->GetWorld();
    OutSnapshot.bValid = true;
    if (const TSharedPtr<const FSlateUser> SlateUser = GetLocalPlayer()->GetSlateUser())
    {
        OutSnapshot.FocusedSlateWidget = SlateUser->GetFocusedWidget();
    }
    return true;
}

bool UUiSubsystem::ApplyInputState(const FInputStateSnapshot& Snapshot) const
{
    APlayerController* Controller = GetLocalPlayerController();
    if (!Snapshot.bValid || !Controller || Snapshot.PlayerController != Controller || Snapshot.World != Controller->GetWorld()) return false;
    UPdUIActionRouter* Router = GetLocalPlayer()->GetSubsystem<UPdUIActionRouter>();
    if (!Router) return false;
    const ECommonInputMode Mode = Snapshot.InputMode == EUiInputMode::UIOnly ? ECommonInputMode::Menu
        : Snapshot.InputMode == EUiInputMode::GameOnly ? ECommonInputMode::Game : ECommonInputMode::All;
    FUIInputConfig Config(Mode, Snapshot.MouseCaptureMode, Snapshot.MouseLockMode, Snapshot.bHideCursorDuringCapture);
    Config.bIgnoreMoveInput = Config.bIgnoreLookInput = Mode != ECommonInputMode::Game;
    Router->SetFallbackInput(Config, Snapshot.FocusedSlateWidget.Pin(), Snapshot.bShowMouseCursor);
    return true;
}

bool UUiSubsystem::ApplyModalInput(APlayerController* PlayerController, UWidget* FocusWidget, const FUiModalInputConfig& InputConfig) const
{
    if (!PlayerController) return false;
    UPdUIActionRouter* Router = GetLocalPlayer()->GetSubsystem<UPdUIActionRouter>();
    if (!Router) return false;
    const ECommonInputMode Mode = InputConfig.InputMode == EUiInputMode::UIOnly ? ECommonInputMode::Menu
        : InputConfig.InputMode == EUiInputMode::GameOnly && InputConfig.bApplyInputMode ? ECommonInputMode::Game : ECommonInputMode::All;
    FUIInputConfig Config(Mode, InputConfig.bShowMouseCursor ? EMouseCaptureMode::NoCapture : EMouseCaptureMode::CapturePermanently,
        InputConfig.MouseLockMode, !InputConfig.bShowMouseCursor);
    Config.bIgnoreMoveInput = Config.bIgnoreLookInput = Mode != ECommonInputMode::Game;
    const TSharedPtr<SWidget> Focus = FocusWidget ? FocusWidget->TakeWidget() : TSharedPtr<SWidget>();
    Router->SetFallbackInput(Config, Focus, InputConfig.bShowMouseCursor);
    return true;
}

bool UUiSubsystem::ApplyGameplayInput(APlayerController* PlayerController) const
{
	FUiModalInputConfig GameplayInputConfig;
	GameplayInputConfig.InputMode = EUiInputMode::GameOnly;
	GameplayInputConfig.bShowMouseCursor = false;
	GameplayInputConfig.bEnableClickEvents = false;
	GameplayInputConfig.bEnableMouseOverEvents = false;
	return ApplyModalInput(PlayerController, nullptr, GameplayInputConfig);
}

void UUiSubsystem::ApplyTopModalInput()
{
	if (bIsDeinitializing || ModalInputStack.IsEmpty())
	{
		return;
	}

	APlayerController* PlayerController = GetLocalPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	if (!InputStateBeforeModals.bValid
		|| InputStateBeforeModals.PlayerController.Get() != PlayerController
		|| InputStateBeforeModals.World.Get() != PlayerController->GetWorld())
	{
		CaptureInputState(PlayerController, InputStateBeforeModals);
	}

	const FModalInputEntry& TopEntry = ModalInputStack.Last();
	ApplyModalInput(PlayerController, TopEntry.FocusWidget.Get(), TopEntry.InputConfig);
}

void UUiSubsystem::RestoreInputStateAfterLastModal()
{
	if (!bIsDeinitializing)
	{
		APlayerController* PlayerController = GetLocalPlayerController();
		const bool bSameInputContext =
			InputStateBeforeModals.bValid
			&& IsValid(PlayerController)
			&& InputStateBeforeModals.PlayerController.Get() == PlayerController
			&& InputStateBeforeModals.World.Get() == PlayerController->GetWorld();

		if (bSameInputContext
			&& RestorePolicyAfterModals == EUiInputRestorePolicy::Gameplay)
		{
			ApplyGameplayInput(PlayerController);
		}
		else
		{
			ApplyInputState(InputStateBeforeModals);
		}
	}
	InputStateBeforeModals.Reset();
	RestorePolicyAfterModals = EUiInputRestorePolicy::PreviousState;
}

void UUiSubsystem::RefreshRestorePolicyFromBottomModal()
{
	if (!ModalInputStack.IsEmpty())
	{
		RestorePolicyAfterModals = ModalInputStack[0].InputConfig.RestorePolicy;
	}
}

bool UUiSubsystem::IsModalInputEntryValid(const FModalInputEntry& Entry) const
{
	if (!Entry.Token.IsValid() || !Entry.Owner.IsValid())
	{
		return false;
	}

	const APlayerController* PlayerController = GetLocalPlayerController();
	const UWorld* CurrentWorld = PlayerController ? PlayerController->GetWorld() : nullptr;
	if (CurrentWorld && Entry.World.Get() != CurrentWorld)
	{
		return false;
	}

	if (!Entry.bTracksFocusWidgetLifetime)
	{
		return true;
	}

	const UWidget* FocusWidget = Entry.FocusWidget.Get();
	if (!IsValid(FocusWidget))
	{
		return false;
	}

	return !CurrentWorld
		|| FocusWidget->GetWorld() == CurrentWorld;
}

void UUiSubsystem::PruneInvalidModalInputs()
{
	if (ModalInputStack.IsEmpty())
	{
		return;
	}

	const FGuid PreviousTopToken = ModalInputStack.Last().Token;
	const int32 RemovedCount = ModalInputStack.RemoveAll(
		[this](const FModalInputEntry& Entry)
		{
			return !IsModalInputEntryValid(Entry);
		});
	if (RemovedCount == 0)
	{
		return;
	}

	if (ModalInputStack.IsEmpty())
	{
		RestoreInputStateAfterLastModal();
	}
	else
	{
		RefreshRestorePolicyFromBottomModal();
		if (ModalInputStack.Last().Token != PreviousTopToken)
		{
			ApplyTopModalInput();
		}
	}
}

bool UUiSubsystem::ReleaseModalInputInternal(
	const UObject* Owner,
	const FGuid& Token,
	const bool bRequireOwnerMatch)
{
	if (!Token.IsValid())
	{
		return false;
	}

	const int32 EntryIndex = ModalInputStack.IndexOfByPredicate(
		[&Token](const FModalInputEntry& Entry)
		{
			return Entry.Token == Token;
		});
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	if (bRequireOwnerMatch && ModalInputStack[EntryIndex].Owner.Get() != Owner)
	{
		UE_LOG(
			PdUiSubsystemLog,
			Warning,
			TEXT("Rejected modal input release because token owner did not match."));
		return false;
	}

	const FGuid PreviousTopToken = ModalInputStack.Last().Token;
	ModalInputStack.RemoveAt(EntryIndex);
	ModalInputStack.RemoveAll(
		[this](const FModalInputEntry& Entry)
		{
			return !IsModalInputEntryValid(Entry);
		});

	const FGuid NewTopToken =
		ModalInputStack.IsEmpty() ? FGuid() : ModalInputStack.Last().Token;
	if (!ModalInputStack.IsEmpty())
	{
		RefreshRestorePolicyFromBottomModal();
	}
	if (NewTopToken != PreviousTopToken)
	{
		if (ModalInputStack.IsEmpty())
		{
			RestoreInputStateAfterLastModal();
		}
		else
		{
			ApplyTopModalInput();
		}
	}

	return true;
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
	bTravelLoadingScreenActive = false;
	bTravelLoadingScreenCancelEnabled = false;
	if (IsValid(ActiveConnectingPopupWidget))
	{
		ActiveConnectingPopupWidget->OnCanceled.RemoveDynamic(
			this,
			&ThisClass::HandleConnectingPopupCanceled);
	}
    if (ConnectingScreen)
    {
        ConnectingScreen->DeactivateWidget();
        ConnectingScreen->RemoveFromParent();
        ConnectingScreen = nullptr;
    }
	ActiveConnectingPopupWidget = nullptr;
}

void UUiSubsystem::PushScreen(UCommonActivatableWidget* Screen, EUiScreenLayer Layer)
{
    APlayerController* Controller = GetLocalPlayerController();
    if (!Controller || !Screen || bIsDeinitializing) return;
    if (!ScreenRoot || ScreenRoot->GetWorld() != Controller->GetWorld())
    {
        if (ScreenRoot) ScreenRoot->RemoveFromParent();
        ScreenRoot = CreateWidget<UUiLayerRoot>(Controller);
        ScreenRoot->AddToPlayerScreen(1000);
    }
    UCommonActivatableWidgetStack* Stack = Layer == EUiScreenLayer::Menu ? ScreenRoot->MenuStack : ScreenRoot->ModalStack;
    Stack->AddWidgetInstance(*Screen);
}

void UUiSubsystem::SetBaseInputMode(APlayerController* Controller, EUiInputMode Mode, UWidget* FocusWidget)
{
    if (!Controller || !Controller->IsLocalController() || !Controller->GetLocalPlayer()) return;
    UUiSubsystem* Ui = Controller->GetLocalPlayer()->GetSubsystem<UUiSubsystem>();
    if (!Ui || Ui->bIsDeinitializing) return;
    FInputStateSnapshot& State = Ui->BaseInputState;
    State.PlayerController = Controller;
    State.World = Controller->GetWorld();
    State.InputMode = Mode;
    State.bShowMouseCursor = Mode != EUiInputMode::GameOnly;
    State.MouseCaptureMode = State.bShowMouseCursor ? EMouseCaptureMode::NoCapture : EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown;
    State.MouseLockMode = State.bShowMouseCursor ? EMouseLockMode::DoNotLock : EMouseLockMode::LockOnCapture;
    State.bHideCursorDuringCapture = !State.bShowMouseCursor;
    State.FocusedSlateWidget = FocusWidget ? FocusWidget->TakeWidget() : TSharedPtr<SWidget>();
    State.bValid = true;
    Ui->PruneInvalidModalInputs();
    if (Ui->ModalInputStack.IsEmpty()) Ui->ApplyInputState(State);
}

void UUiSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
    Super::PlayerControllerChanged(NewPlayerController);
    HideConnectingPopup();
    if (ScreenRoot) ScreenRoot->RemoveFromParent();
    ScreenRoot = nullptr;
    ModalInputStack.Reset();
    InputStateBeforeModals.Reset();
    BaseInputState.Reset();
    RestorePolicyAfterModals = EUiInputRestorePolicy::PreviousState;
    if (NewPlayerController)
    {
        SetBaseInputMode(NewPlayerController, EUiInputMode::GameOnly);
        if (bTravelLoadingScreenActive) ShowConnectingPopup(bTravelLoadingScreenCancelEnabled);
    }
}
