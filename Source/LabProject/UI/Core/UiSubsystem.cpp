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
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdPlayerState.h"
#include "UI/Core/WidgetClassDefinition.h"
#include "UI/Cursor/MouseCursorWidget.h"
#include "Data/ContentLease.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Settings/LocalPlayerSettingsSubsystem.h"
#include "UI/Common/ViewModelBinding.h"
#include "ViewModel/StatusViewModel.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(UiSubsystem)

DEFINE_LOG_CATEGORY(PdUiSubsystemLog);

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
	if (ULocalPlayerSettingsSubsystem* PlayerSettings = Collection.InitializeDependency<ULocalPlayerSettingsSubsystem>())
	{
		CustomMouseCursorSettingsHandle = PlayerSettings->OnCustomMouseCursorSettingsReady.AddUObject(
			this,
			&ThisClass::HandleCustomMouseCursorSettingsReady);
	}
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	if (ULobbyRuntimeSubsystem* LobbyRuntime = GameInstance ? GameInstance->GetSubsystem<ULobbyRuntimeSubsystem>() : nullptr)
	{
		LobbyEntryPreloadRequestedHandle = LobbyRuntime->OnLobbyEntryContentPreloadRequested.AddUObject(
			this,
			&ThisClass::HandleLobbyEntryContentPreloadRequested);
		LobbyEntryReleasedHandle = LobbyRuntime->OnLobbyEntryContentReleased.AddUObject(
			this,
			&ThisClass::HandleLobbyEntryContentReleased);
	}
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
}

void UUiSubsystem::Deinitialize()
{
	bIsDeinitializing = true;
	CloseGameSettings();
	OnWidgetContentChanged.Clear();
	if (ULocalPlayerSettingsSubsystem* PlayerSettings =
		GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<ULocalPlayerSettingsSubsystem>() : nullptr)
	{
		PlayerSettings->OnCustomMouseCursorSettingsReady.Remove(CustomMouseCursorSettingsHandle);
	}
	CustomMouseCursorSettingsHandle.Reset();
	const ULocalPlayer* OwningLocalPlayer = GetLocalPlayer();
	if (ULobbyRuntimeSubsystem* LobbyRuntime = OwningLocalPlayer && OwningLocalPlayer->GetGameInstance()
		? OwningLocalPlayer->GetGameInstance()->GetSubsystem<ULobbyRuntimeSubsystem>()
		: nullptr)
	{
		LobbyRuntime->OnLobbyEntryContentPreloadRequested.Remove(LobbyEntryPreloadRequestedHandle);
		LobbyRuntime->OnLobbyEntryContentReleased.Remove(LobbyEntryReleasedHandle);
	}
	LobbyEntryPreloadRequestedHandle.Reset();
	LobbyEntryReleasedHandle.Reset();
	LobbyContentLease.Reset();

	if (StatusViewModel && StatusViewModel->IsViewModelInitialized())
	{
		StatusViewModel->UninitializeViewModel();
	}

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
		OnWidgetContentChanged.Broadcast();
	}
}

void UUiSubsystem::ClearWidgetClassDefinition(
	const UWidgetClassDefinition* ExpectedWidgetClassDefinition)
{
	if (!ExpectedWidgetClassDefinition || WidgetClassDefinition == ExpectedWidgetClassDefinition)
	{
		bHasExternalWidgetClassDefinition = false;
		WidgetClassDefinition = ConfiguredWidgetClassDefinition;
		OnWidgetContentChanged.Broadcast();
	}
}

TSharedPtr<FContentLease> UUiSubsystem::AcquireUiContent(
	const UWidgetClassDefinition* Definition,
	const EUiContentGroup Group,
	FSimpleDelegate OnComplete)
{
	if (!IsValid(Definition) || bIsDeinitializing)
	{
		return nullptr;
	}
	TArray<FSoftObjectPath> AssetPaths;
	Definition->GetRuntimePreloadAssetPaths(Group, AssetPaths);
	return UContentDataSubsystem::AcquireContent(AssetPaths, MoveTemp(OnComplete));
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

	// 로드에 실패하면 정의가 비어 있으므로 처리기가 대기 표시를 내리고 실패를 알린다.
	ConfiguredDefinitionLease = ContentSubsystem->AcquireContent(
		{DefaultWidgetClassDefinition.ToSoftObjectPath()},
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleConfiguredWidgetDefinitionLoaded));
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

	// 루트 정의는 불러오는 즉시 알린다. 아래 Core lease는 항상 필요한 UI만 붙잡고,
	// 로비·게임·정보창·지도 UI는 각 화면이 직접 붙잡는다.
	if (!bHasExternalWidgetClassDefinition)
	{
		WidgetClassDefinition = ConfiguredWidgetClassDefinition;
	}

	OnWidgetContentChanged.Broadcast();

	BindPendingConfiguredUiContent();
	RefreshConfiguredWidgetContentState();
}

void UUiSubsystem::BindPendingConfiguredUiContent()
{
	if (!ConfiguredWidgetClassDefinition)
	{
		return;
	}
	auto PendingRequests = MoveTemp(PendingConfiguredUiContent);
	PendingConfiguredUiContent.Reset();
	for (const auto& Request : PendingRequests)
	{
		if (const TSharedPtr<FContentLease> Lease = Request.Value.Pin())
		{
			TArray<FSoftObjectPath> AssetPaths;
			ConfiguredWidgetClassDefinition->GetRuntimePreloadAssetPaths(Request.Key, AssetPaths);
			Lease->Start(AssetPaths);
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
	OnWidgetContentChanged.Broadcast();
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
	ConfiguredDefinitionLease.Reset();
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

		// ForEachWidget은 안에 든 UUserWidget 자체는 방문하지만 그 위젯의 WidgetTree 안으로는 들어가지 않는다.
		// 그 위젯들에 붙은 MVVM 확장을 놓치지 않도록 직접 재귀한다.
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
	return PdViewModelBinding::SetViewModel(InWidget, StatusViewModel, UStatusViewModel::ViewModelName);
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

APlayerController* UUiSubsystem::GetLocalPlayerController() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
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
    if (ScreenRoot) ScreenRoot->RemoveFromParent();
    ScreenRoot = nullptr;
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
	PushScreen(UUiScreen::CreateBlocking(Controller, ActiveGameSettings, ActiveGameSettings->GetInitialFocusTarget(),
		FSimpleDelegate::CreateUObject(ActiveGameSettings, &UGameSettingsWidget::CloseSettings)), EUiScreenLayer::Modal);
}

bool UUiSubsystem::HasActiveScreen(const UWorld* World) const
{
	return ScreenRoot && ScreenRoot->GetWorld() == World && ScreenRoot->ScreenStack
		&& ScreenRoot->ScreenStack->GetActiveWidget();
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

// 설정 서브시스템이 사용자 지정 커서 설정이 준비됐다고 알리면 커서 위젯을 뷰포트에 건다.
void UUiSubsystem::HandleCustomMouseCursorSettingsReady(
	APlayerController* PlayerController,
	const UGameSettingDefinition& SettingDefinition)
{
	UMouseCursorWidget::InstallConfiguredCursor(PlayerController, SettingDefinition);
}

// 로비 진입 준비가 시작되면 위젯 정의와 로비 화면 콘텐츠를 붙잡는다. 실패한 이전 요청은 다시 시도한다.
void UUiSubsystem::HandleLobbyEntryContentPreloadRequested()
{
	BeginConfiguredWidgetDefinitionPreload();
	if (LobbyContentLease.IsValid() && LobbyContentLease->HasFailed())
	{
		LobbyContentLease.Reset();
	}
	if (!LobbyContentLease.IsValid())
	{
		LobbyContentLease = AcquireConfiguredUiContent(EUiContentGroup::Lobby);
	}
}

// 경기 화면이 자리를 잡으면 로비 전용 화면 콘텐츠를 놓는다.
void UUiSubsystem::HandleLobbyEntryContentReleased()
{
	LobbyContentLease.Reset();
}

bool UUiSubsystem::IsLobbyContentLoading() const
{
	return LobbyContentLease.IsValid() && LobbyContentLease->IsLoading();
}
