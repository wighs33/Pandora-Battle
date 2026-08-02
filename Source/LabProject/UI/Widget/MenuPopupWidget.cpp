#include "UI/Widget/MenuPopupWidget.h"

#include "AudioSlider.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Mode/PdHUD.h"
#include "Mode/PdPlayerController.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "UI/UiSubsystem.h"
#include "UI/Widget/AudioVolumeControl.h"
#include "UI/Widget/GuideWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MenuPopupWidget)

UMenuPopupWidget::UMenuPopupWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UMenuPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	if (Btn_Resume)
	{
		Btn_Resume->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleResumeClicked);
	}

	if (Btn_Exit)
	{
		Btn_Exit->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleExitClicked);
	}

	if (Btn_Guide)
	{
		Btn_Guide->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGuideClicked);
	}

	if (!AudioVolumeSlider_)
	{
		AudioVolumeSlider_ = Cast<UAudioVolumeSlider>(GetWidgetFromName(TEXT("AudioVolumeSlider_")));
	}
	if (!Btn_Sound)
	{
		Btn_Sound = Cast<UButton>(GetWidgetFromName(TEXT("Btn_Sound")));
	}
	if (AudioVolumeSlider_ || Btn_Sound)
	{
		AudioVolumeControl = NewObject<UAudioVolumeControl>(this);
		AudioVolumeControl->Initialize(this, AudioVolumeSlider_, Btn_Sound);
	}

	if (!bInputModeManagedExternally)
	{
		ApplyMenuInputMode();
	}

	if (bPauseGameWhenOpened)
	{
		SetRequestedPause(true);
	}
}

void UMenuPopupWidget::NativeDestruct()
{
	ClearDestroySessionDelegate();
	DiscardGuideWidget();

	if (AudioVolumeControl)
	{
		AudioVolumeControl->Shutdown();
		AudioVolumeControl = nullptr;
	}

	if (Btn_Resume)
	{
		Btn_Resume->OnClicked.RemoveDynamic(this, &ThisClass::HandleResumeClicked);
	}

	if (Btn_Exit)
	{
		Btn_Exit->OnClicked.RemoveDynamic(this, &ThisClass::HandleExitClicked);
	}

	if (Btn_Guide)
	{
		Btn_Guide->OnClicked.RemoveDynamic(this, &ThisClass::HandleGuideClicked);
	}

	if (bAppliedPause)
	{
		SetRequestedPause(false);
	}

	ReleaseMenuInputMode();
	Super::NativeDestruct();
}

void UMenuPopupWidget::CloseMenu()
{
	if (bAppliedPause)
	{
		SetRequestedPause(false);
	}

	const bool bWasUsingModalInputRouter = MenuModalInputToken.IsValid();
	RemoveFromParent();
	ReleaseMenuInputMode();
	OnMenuClosed.Broadcast(this);

	if (!bInputModeManagedExternally && !bWasUsingModalInputRouter && bRestoreGameInputOnClose)
	{
		RestoreGameInputMode();
	}
}

void UMenuPopupWidget::SetRestoreGameInputOnClose(const bool bInRestoreGameInputOnClose)
{
	bRestoreGameInputOnClose = bInRestoreGameInputOnClose;
}

void UMenuPopupWidget::SetInputModeManagedExternally(const bool bManagedExternally)
{
	if (bInputModeManagedExternally == bManagedExternally)
	{
		return;
	}

	bInputModeManagedExternally = bManagedExternally;
	if (bInputModeManagedExternally)
	{
		ReleaseMenuInputMode();
	}
	else if (IsInViewport())
	{
		ApplyMenuInputMode();
	}
}

bool UMenuPopupWidget::CloseGuide()
{
	if (!IsValid(ActiveGuideWidget) || !ActiveGuideWidget->IsInViewport())
	{
		ActiveGuideWidget = nullptr;
		return false;
	}

	ActiveGuideWidget->CloseGuide();
	return true;
}

UWidget* UMenuPopupWidget::GetActiveGuideWidget() const
{
	return IsValid(ActiveGuideWidget) && ActiveGuideWidget->IsInViewport()
		? ActiveGuideWidget.Get()
		: nullptr;
}

void UMenuPopupWidget::ExitToTitleMap()
{


	if (bAppliedPause)
	{
		SetRequestedPause(false);
	}

	APlayerController* PlayerController = GetOwningPlayer();
	if (PlayerController)
	{
		if (APdPlayerController* PdPlayerController = Cast<APdPlayerController>(PlayerController))
		{
			if (PdPlayerController->RequestExitMatchToTitle())
			{
				return;
			}
		}
	}
	if (const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UUiSubsystem* UiSubsystem = LocalPlayer->GetSubsystem<UUiSubsystem>())
		{
			UiSubsystem->HideConnectingPopup();
		}
	}

	UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr;
	const bool bNeedsSessionDestroy = bDestroySessionOnExit
		&& OnlineSessionsSubsystem
		&& OnlineSessionsSubsystem->HasNamedSession();
	if (bNeedsSessionDestroy)
	{
		if (const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
		{
			if (UUiSubsystem* UiSubsystem = LocalPlayer->GetSubsystem<UUiSubsystem>())
			{
				UiSubsystem->ShowConnectingPopup(false);
			}
		}

		ClearDestroySessionDelegate();
		DestroySessionCompleteHandle = OnlineSessionsSubsystem->OnDestroySessionComplete.AddUObject(
			this,
			&ThisClass::HandleDestroySessionForExit);
		OnlineSessionsSubsystem->DestroySession();
		return;
	}

	TravelToTitleMap();
}

void UMenuPopupWidget::HandleResumeClicked()
{
	CloseMenu();
}

void UMenuPopupWidget::HandleExitClicked()
{

	ExitToTitleMap();
}

void UMenuPopupWidget::HandleGuideClicked()
{
	OpenGuide();
}

void UMenuPopupWidget::HandleGuideClosed(UGuideWidget* ClosedGuideWidget)
{
	if (ActiveGuideWidget != ClosedGuideWidget)
	{
		return;
	}

	ActiveGuideWidget->OnGuideClosed.RemoveDynamic(this, &ThisClass::HandleGuideClosed);
	ActiveGuideWidget = nullptr;
	RestoreMenuAfterGuide();
}

void UMenuPopupWidget::OpenGuide()
{
	APlayerController* PlayerController = GetOwningPlayer();
	const UWidgetClassDefinition* WidgetDefinition =
		UWidgetClassDefinition::ResolveWidgetClassDefinition(this);
	const TSubclassOf<UGuideWidget> GuideWidgetClass =
		WidgetDefinition ? WidgetDefinition->GetGuideWidgetClass() : nullptr;
	if (!PlayerController || !PlayerController->IsLocalController() || !GuideWidgetClass)
	{
		return;
	}

	if (ActiveGuideWidget
		&& (!IsValid(ActiveGuideWidget)
			|| ActiveGuideWidget->GetWorld() != GetWorld()
			|| ActiveGuideWidget->GetClass() != GuideWidgetClass.Get()))
	{
		DiscardGuideWidget();
	}

	if (!ActiveGuideWidget)
	{
		ActiveGuideWidget = CreateWidget<UGuideWidget>(PlayerController, GuideWidgetClass);
	}
	if (!ActiveGuideWidget)
	{
		return;
	}

	ActiveGuideWidget->SetOpenedFromGameplayMenu(true);
	ActiveGuideWidget->OnGuideClosed.RemoveDynamic(this, &ThisClass::HandleGuideClosed);
	ActiveGuideWidget->OnGuideClosed.AddUniqueDynamic(this, &ThisClass::HandleGuideClosed);
	SetVisibility(ESlateVisibility::Collapsed);
	ActiveGuideWidget->SetVisibility(ESlateVisibility::Visible);
	if (!ActiveGuideWidget->IsInViewport())
	{
		ActiveGuideWidget->AddToViewport(110);
	}
	ActiveGuideWidget->RefreshGuide();

	if (APdHUD* Hud = PlayerController->GetHUD<APdHUD>())
	{
		Hud->ToggleUiMode(true);
	}
}

void UMenuPopupWidget::DiscardGuideWidget()
{
	if (IsValid(ActiveGuideWidget))
	{
		ActiveGuideWidget->OnGuideClosed.RemoveDynamic(this, &ThisClass::HandleGuideClosed);
		ActiveGuideWidget->RemoveFromParent();
	}
	ActiveGuideWidget = nullptr;
}

void UMenuPopupWidget::RestoreMenuAfterGuide()
{
	if (!IsInViewport())
	{
		return;
	}

	SetVisibility(ESlateVisibility::Visible);
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (APdHUD* Hud = PlayerController->GetHUD<APdHUD>())
		{
			Hud->ToggleUiMode(true);
		}
	}
}

void UMenuPopupWidget::ApplyMenuInputMode()
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{
		return;
	}

	if (const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UUiSubsystem* UiSubsystem = LocalPlayer->GetSubsystem<UUiSubsystem>())
		{
			FPdUiModalInputConfig InputConfig;
			InputConfig.RestorePolicy = bRestoreGameInputOnClose
				? EPdUiInputRestorePolicy::Gameplay
				: EPdUiInputRestorePolicy::PreviousState;
			if (UiSubsystem->UpdateModalInput(
				this,
				MenuModalInputToken,
				this,
				InputConfig))
			{
				return;
			}

			MenuModalInputToken.Invalidate();
			MenuModalInputToken = UiSubsystem->AcquireModalInput(this, this, InputConfig);
			if (MenuModalInputToken.IsValid())
			{
				return;
			}
		}
	}

	UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(
		PlayerController,
		this,
		EMouseLockMode::DoNotLock,
		false,
		false);
	PlayerController->bShowMouseCursor = true;
	PlayerController->bEnableClickEvents = true;
	PlayerController->bEnableMouseOverEvents = true;
	SetIsFocusable(true);
	SetUserFocus(PlayerController);
	SetFocus();
}

void UMenuPopupWidget::ReleaseMenuInputMode()
{
	if (!MenuModalInputToken.IsValid())
	{
		return;
	}

	if (const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UUiSubsystem* UiSubsystem = LocalPlayer->GetSubsystem<UUiSubsystem>())
		{
			UiSubsystem->ReleaseModalInput(this, MenuModalInputToken);
		}
	}

	MenuModalInputToken.Invalidate();
}

void UMenuPopupWidget::RestoreGameInputMode() const
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{
		return;
	}

	UWidgetBlueprintLibrary::SetInputMode_GameOnly(PlayerController, false);
	PlayerController->bShowMouseCursor = false;
	PlayerController->bEnableClickEvents = false;
	PlayerController->bEnableMouseOverEvents = false;
}

void UMenuPopupWidget::SetRequestedPause(const bool bPaused)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!bAllowNetworkPause && World->GetNetMode() != NM_Standalone)
	{

		return;
	}

	UGameplayStatics::SetGamePaused(this, bPaused);
	bAppliedPause = bPaused;
}

FString UMenuPopupWidget::GetResolvedTitleTravelMapName() const
{
	const FString LongPackageName = TitleMap.ToSoftObjectPath().GetLongPackageName();
	return LongPackageName.IsEmpty() ? TitleTravelMapName : LongPackageName;
}

void UMenuPopupWidget::TravelToTitleMap()
{
	const FString TitleMapName = GetResolvedTitleTravelMapName();
	if (TitleMapName.IsEmpty())
	{

		return;
	}


	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::OpenLevel(World, FName(*TitleMapName));
		return;
	}

	UGameplayStatics::OpenLevel(this, FName(*TitleMapName));
}

void UMenuPopupWidget::ClearDestroySessionDelegate()
{
	if (!DestroySessionCompleteHandle.IsValid())
	{
		return;
	}

	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr)
	{
		OnlineSessionsSubsystem->OnDestroySessionComplete.Remove(DestroySessionCompleteHandle);
	}

	DestroySessionCompleteHandle.Reset();
}

void UMenuPopupWidget::HandleDestroySessionForExit(const bool bWasSuccessful)
{


	ClearDestroySessionDelegate();
	TravelToTitleMap();
}
