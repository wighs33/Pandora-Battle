#include "UI/UiSubsystem.h"

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
	if (!InWidget || !StatusViewModel)
	{
		return false;
	}

	const bool bViewModelReady = RefreshStatusViewModel();

	UMVVMView* ViewExtension = InWidget->GetExtension<UMVVMView>();
	if (!ViewExtension)
	{

		return false;
	}

	const FName ViewModelSourceName = ResolveStatusViewModelSourceName(InWidget);
	if (ViewModelSourceName.IsNone())
	{

		return false;
	}

	const bool bSuccess = ViewExtension->SetViewModel(ViewModelSourceName, StatusViewModel);
	if (!bSuccess)
	{

		return false;
	}

	if (bViewModelReady)
	{
		StatusViewModel->UpdateAllData();
	}

	return bViewModelReady;
}

bool UUiSubsystem::ApplyStatusViewModelToWidgetTree(UUserWidget* RootWidget)
{
	if (!RootWidget)
	{
		return false;
	}

	bool bAppliedAny = ApplyStatusViewModelToWidget(RootWidget);

	if (!RootWidget->WidgetTree)
	{
		return bAppliedAny;
	}

	RootWidget->WidgetTree->ForEachWidget([this, &bAppliedAny](UWidget* Widget)
	{
		if (UUserWidget* ChildUserWidget = Cast<UUserWidget>(Widget))
		{
			bAppliedAny |= ApplyStatusViewModelToWidgetTree(ChildUserWidget);
		}
	});

	return bAppliedAny;
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
		ReleaseModalInputInternal(nullptr, ConnectingPopupModalToken, false);
		ConnectingPopupModalToken.Invalidate();
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

	if (!ActiveConnectingPopupWidget->IsInViewport())
	{
		ActiveConnectingPopupWidget->AddToViewport(1000);
	}

	const FUiModalInputConfig InputConfig;
	if (!UpdateModalInput(
			ActiveConnectingPopupWidget,
			ConnectingPopupModalToken,
			ActiveConnectingPopupWidget,
			InputConfig))
	{
		ReleaseModalInputInternal(nullptr, ConnectingPopupModalToken, false);
		ConnectingPopupModalToken = AcquireModalInput(
			ActiveConnectingPopupWidget,
			ActiveConnectingPopupWidget,
			InputConfig);
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

	ReleaseModalInputInternal(nullptr, ConnectingPopupModalToken, false);
	ConnectingPopupModalToken.Invalidate();
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

bool UUiSubsystem::CaptureInputState(
	APlayerController* PlayerController,
	FInputStateSnapshot& OutSnapshot) const
{
	OutSnapshot.Reset();
	if (!IsValid(PlayerController))
	{
		return false;
	}

	OutSnapshot.PlayerController = PlayerController;
	OutSnapshot.World = PlayerController->GetWorld();
	OutSnapshot.bShowMouseCursor = PlayerController->bShowMouseCursor;
	OutSnapshot.bEnableClickEvents = PlayerController->bEnableClickEvents;
	OutSnapshot.bEnableMouseOverEvents = PlayerController->bEnableMouseOverEvents;

	if (UGameViewportClient* GameViewport =
		PlayerController->GetWorld() ? PlayerController->GetWorld()->GetGameViewport() : nullptr)
	{
		OutSnapshot.MouseCaptureMode = GameViewport->GetMouseCaptureMode();
		OutSnapshot.MouseLockMode = GameViewport->GetMouseLockMode();
		OutSnapshot.bIgnoreViewportInput = GameViewport->IgnoreInput();
		OutSnapshot.bHideCursorDuringCapture = GameViewport->HideCursorDuringCapture();

		if (OutSnapshot.bIgnoreViewportInput)
		{
			OutSnapshot.InputMode = EUiInputMode::UIOnly;
		}
		else if (OutSnapshot.MouseCaptureMode == EMouseCaptureMode::CapturePermanently
			|| OutSnapshot.MouseCaptureMode == EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown)
		{
			OutSnapshot.InputMode = EUiInputMode::GameOnly;
		}
		else
		{
			OutSnapshot.InputMode = EUiInputMode::GameAndUI;
		}
	}
	else
	{
		OutSnapshot.InputMode =
			OutSnapshot.bShowMouseCursor ? EUiInputMode::GameAndUI : EUiInputMode::GameOnly;
	}

	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (const TSharedPtr<const FSlateUser> SlateUser = LocalPlayer->GetSlateUser())
		{
			OutSnapshot.FocusedSlateWidget = SlateUser->GetFocusedWidget();
		}
	}

	OutSnapshot.bValid = true;
	return true;
}

bool UUiSubsystem::ApplyInputState(const FInputStateSnapshot& Snapshot) const
{
	APlayerController* PlayerController = GetLocalPlayerController();
	if (!Snapshot.bValid
		|| !IsValid(PlayerController)
		|| Snapshot.PlayerController.Get() != PlayerController
		|| Snapshot.World.Get() != PlayerController->GetWorld())
	{
		return false;
	}

	const TSharedPtr<SWidget> FocusedSlateWidget = Snapshot.FocusedSlateWidget.Pin();
	switch (Snapshot.InputMode)
	{
	case EUiInputMode::UIOnly:
		{
			FInputModeUIOnly InputMode;
			InputMode.SetWidgetToFocus(FocusedSlateWidget);
			InputMode.SetLockMouseToViewportBehavior(Snapshot.MouseLockMode);
			PlayerController->SetInputMode(InputMode);
			break;
		}

	case EUiInputMode::GameAndUI:
		{
			FInputModeGameAndUI InputMode;
			InputMode.SetWidgetToFocus(FocusedSlateWidget);
			InputMode.SetLockMouseToViewportBehavior(Snapshot.MouseLockMode);
			InputMode.SetHideCursorDuringCapture(Snapshot.bHideCursorDuringCapture);
			PlayerController->SetInputMode(InputMode);
			break;
		}

	case EUiInputMode::GameOnly:
	default:
		{
			FInputModeGameOnly InputMode;
			InputMode.SetConsumeCaptureMouseDown(
				Snapshot.MouseCaptureMode !=
				EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
			PlayerController->SetInputMode(InputMode);
			break;
		}
	}

	if (UGameViewportClient* GameViewport =
		PlayerController->GetWorld() ? PlayerController->GetWorld()->GetGameViewport() : nullptr)
	{
		GameViewport->SetIgnoreInput(Snapshot.bIgnoreViewportInput);
		GameViewport->SetMouseCaptureMode(Snapshot.MouseCaptureMode);
		GameViewport->SetMouseLockMode(Snapshot.MouseLockMode);
		GameViewport->SetHideCursorDuringCapture(Snapshot.bHideCursorDuringCapture);
	}

	PlayerController->bShowMouseCursor = Snapshot.bShowMouseCursor;
	PlayerController->bEnableClickEvents = Snapshot.bEnableClickEvents;
	PlayerController->bEnableMouseOverEvents = Snapshot.bEnableMouseOverEvents;

	if (Snapshot.InputMode != EUiInputMode::GameOnly && !FocusedSlateWidget.IsValid())
	{
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			if (const TSharedPtr<FSlateUser> SlateUser = LocalPlayer->GetSlateUser())
			{
				SlateUser->ClearFocus();
			}
		}
	}

	return true;
}

bool UUiSubsystem::ApplyModalInput(
	APlayerController* PlayerController,
	UWidget* FocusWidget,
	const FUiModalInputConfig& InputConfig) const
{
	if (!IsValid(PlayerController))
	{
		return false;
	}

	TSharedPtr<SWidget> SlateWidget;
	if (IsValid(FocusWidget))
	{
		SlateWidget = FocusWidget->TakeWidget();
	}
	if (InputConfig.bApplyInputMode)
	{
		switch (InputConfig.InputMode)
		{
		case EUiInputMode::UIOnly:
			{
				FInputModeUIOnly InputMode;
				InputMode.SetWidgetToFocus(SlateWidget);
				InputMode.SetLockMouseToViewportBehavior(InputConfig.MouseLockMode);
				PlayerController->SetInputMode(InputMode);
				break;
			}

		case EUiInputMode::GameOnly:
			{
				FInputModeGameOnly InputMode;
				PlayerController->SetInputMode(InputMode);
				break;
			}

		case EUiInputMode::GameAndUI:
		default:
			{
				FInputModeGameAndUI InputMode;
				InputMode.SetWidgetToFocus(SlateWidget);
				InputMode.SetLockMouseToViewportBehavior(InputConfig.MouseLockMode);
				InputMode.SetHideCursorDuringCapture(InputConfig.bHideCursorDuringCapture);
				PlayerController->SetInputMode(InputMode);
				break;
			}
		}
	}

	PlayerController->bShowMouseCursor = InputConfig.bShowMouseCursor;
	PlayerController->bEnableClickEvents = InputConfig.bEnableClickEvents;
	PlayerController->bEnableMouseOverEvents = InputConfig.bEnableMouseOverEvents;

	if (InputConfig.bApplyInputMode
		&& IsValid(FocusWidget)
		&& InputConfig.InputMode != EUiInputMode::GameOnly)
	{
		FocusWidget->SetUserFocus(PlayerController);
	}

	if (InputConfig.bFlushInput)
	{
		PlayerController->FlushPressedKeys();
	}

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
	ReleaseModalInputInternal(nullptr, ConnectingPopupModalToken, false);
	ConnectingPopupModalToken.Invalidate();
	ActiveConnectingPopupWidget = nullptr;
}
