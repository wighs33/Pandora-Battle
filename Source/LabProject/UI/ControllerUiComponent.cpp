#include "UI/ControllerUiComponent.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Widget.h"
#include "Engine/LocalPlayer.h"
#include "Mode/PdPlayerController.h"
#include "UI/InfoUiPresenter.h"
#include "UI/UiSubsystem.h"
#include "UI/WidgetClassDefinition.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/SelectPandoraWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControllerUiComponent)

UControllerUiComponent::UControllerUiComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UControllerUiComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CachedInfoUiPresenter)
	{
		CachedInfoUiPresenter->Deinitialize();
		CachedInfoUiPresenter = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UControllerUiComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateSelectPandoraDirectionFromMouse();
}

void UControllerUiComponent::CreateAllUi()
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller)
	{
		return;
	}

	RefreshUiBindings();

	const UWidgetClassDefinition* WidgetDefinition = GetWidgetClassDefinition();
	if (!WidgetDefinition)
	{
		UE_LOG(PdPlayerControllerLog, Warning, TEXT("CreateAllUi failed: '%s' has no WidgetClassDefinition."), *GetNameSafe(Controller));
		return;
	}

	const TSubclassOf<UInfoWidget> InfoWidgetClass = WidgetDefinition->GetInfoWidgetClass();
	const TSubclassOf<USelectPandoraWidget> SelectPandoraWidgetClass = WidgetDefinition->GetSelectPandoraWidgetClass();
	const TSubclassOf<UUserWidget> AimCrosshairWidgetClass = WidgetDefinition->GetAimCrosshairWidgetClass();

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

	if (CachedInfoUI)
	{
		CachedInfoUI->OnClickedInfoCenterButton.Clear();
		UInfoUiPresenter* InfoUiPresenter = GetInfoUiPresenter();
		if (InfoUiPresenter)
		{
			InfoUiPresenter->BindInfoUi(CachedInfoUI);
			CachedInfoUI->OnClickedInfoCenterButton.AddUniqueDynamic(InfoUiPresenter, &UInfoUiPresenter::HandleClickedInfoCenterButton);
		}
		else
		{
			UE_LOG(PdPlayerControllerLog, Warning, TEXT("CreateAllUi skipped Info UI presenter binding: '%s' has no configured InfoUiPresenter class."), *GetNameSafe(Controller));
		}

		if (URightStatusWidget* RightStatusWidget = CachedInfoUI->GetRightStatusWidget())
		{
			ApplyStatusViewModelToWidget(RightStatusWidget);
		}
	}

	if (CachedSelectPandoraUI)
	{
		CachedSelectPandoraUI->OnSelected.Clear();
		UInfoUiPresenter* InfoUiPresenter = GetInfoUiPresenter();
		if (InfoUiPresenter)
		{
			CachedSelectPandoraUI->OnSelected.AddUniqueDynamic(InfoUiPresenter, &UInfoUiPresenter::HandleSelectedPandoraDirection);
		}
		else
		{
			UE_LOG(PdPlayerControllerLog, Warning, TEXT("CreateAllUi skipped Select Pandora presenter binding: '%s' has no configured InfoUiPresenter class."), *GetNameSafe(Controller));
		}
	}
	else
	{
		UE_LOG(PdPlayerControllerLog, Warning, TEXT("CreateAllUi failed: '%s' has no Select Pandora UI."), *GetNameSafe(Controller));
	}
}

void UControllerUiComponent::OpenInfoUi()
{
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
		UE_LOG(PdPlayerControllerLog, Warning, TEXT("OpenInfoUi failed: '%s' has no Info UI widget class."), *GetNameSafe(Controller));
		return;
	}

	CachedInfoUI->OnClickedInfoCenterButton.Clear();
	UInfoUiPresenter* InfoUiPresenter = GetInfoUiPresenter();
	if (InfoUiPresenter)
	{
		InfoUiPresenter->BindInfoUi(CachedInfoUI);
		CachedInfoUI->OnClickedInfoCenterButton.AddUniqueDynamic(InfoUiPresenter, &UInfoUiPresenter::HandleClickedInfoCenterButton);
	}
	else
	{
		UE_LOG(PdPlayerControllerLog, Warning, TEXT("OpenInfoUi opened without InfoUiPresenter: set InfoWidgetSettings.PresenterClass in WidgetClassDefinition for '%s'."), *GetNameSafe(Controller));
	}

	if (URightStatusWidget* RightStatusWidget = CachedInfoUI->GetRightStatusWidget())
	{
		ApplyStatusViewModelToWidget(RightStatusWidget);
	}

	CachedInfoUI->AddToViewport();
	ToggleUiMode(true);

	if (InfoUiPresenter)
	{
		InfoUiPresenter->HandleOpenedInfoUi();
	}

	CachedInfoUI->SelectProfileTab();
}

void UControllerUiComponent::CloseInfoUi()
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller)
	{
		return;
	}

	if (CachedInfoUI)
	{
		CachedInfoUI->RemoveFromParent();
	}

	ToggleUiMode(false);
}

void UControllerUiComponent::ToggleInfoUi()
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller)
	{
		return;
	}

	if (CachedInfoUI && CachedInfoUI->IsInViewport())
	{
		CloseInfoUi();
		return;
	}

	OpenInfoUi();
}

void UControllerUiComponent::ToggleUiMode(bool bOn)
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

bool UControllerUiComponent::IsGameplayInputBlockedByUi() const
{
	const APdPlayerController* Controller = GetPdController();
	return Controller
		&& ((CachedInfoUI && CachedInfoUI->IsInViewport())
			|| (CachedSelectPandoraUI && CachedSelectPandoraUI->IsInViewport()));
}

void UControllerUiComponent::OpenSelectPandoraUi()
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller)
	{
		return;
	}

	if (!CachedSelectPandoraUI)
	{
		CreateAllUi();
	}

	if (!CachedSelectPandoraUI)
	{
		return;
	}

	CachedSelectPandoraUI->AddToViewport();
	SetComponentTickEnabled(true);
	ToggleUiMode(true);
}

void UControllerUiComponent::CloseSelectPandoraUi()
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller)
	{
		return;
	}

	if (CachedSelectPandoraUI)
	{
		CachedSelectPandoraUI->SetDirection(CachedDirIndex);
		CachedSelectPandoraUI->RemoveFromParent();
	}

	SetComponentTickEnabled(false);
	ToggleUiMode(false);
}

void UControllerUiComponent::UpdateSelectPandoraDirectionFromMouse()
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller || !CachedSelectPandoraUI || !CachedSelectPandoraUI->IsInViewport())
	{
		return;
	}

	const UWidgetClassDefinition* WidgetDefinition = GetWidgetClassDefinition();
	if (!WidgetDefinition)
	{
		return;
	}

	const FSelectPandoraWidgetSettings& SelectPandoraSettings = WidgetDefinition->GetSelectPandoraWidgetSettings();

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

void UControllerUiComponent::ShowAimCrosshair(FGameplayTag DesiredCrosshairWidgetTag)
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller)
	{
		return;
	}

	const UWidgetClassDefinition* WidgetDefinition = GetWidgetClassDefinition();
	if (!WidgetDefinition)
	{
		return;
	}

	TSubclassOf<UUserWidget> DesiredCrosshairWidgetClass = WidgetDefinition->FindWidgetClassByTag(DesiredCrosshairWidgetTag);
	if (!DesiredCrosshairWidgetClass && DesiredCrosshairWidgetTag.IsValid())
	{
		UE_LOG(PdPlayerControllerLog, Warning, TEXT("ShowAimCrosshair fallback: no widget class is mapped to tag '%s'."), *DesiredCrosshairWidgetTag.ToString());
	}

	if (!DesiredCrosshairWidgetClass)
	{
		DesiredCrosshairWidgetClass = WidgetDefinition->GetAimCrosshairWidgetClass();
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

void UControllerUiComponent::HideAimCrosshair()
{
	if (AimCrosshairWidget)
	{
		AimCrosshairWidget->RemoveFromParent();
	}
}

void UControllerUiComponent::RefreshUiBindings()
{
	if (UUiSubsystem* UiSubsystem = GetUiSubsystem())
	{
		UiSubsystem->RefreshStatusViewModel();
	}
}

APdPlayerController* UControllerUiComponent::GetPdController() const
{
	return Cast<APdPlayerController>(GetOwner());
}

UInfoUiPresenter* UControllerUiComponent::GetInfoUiPresenter()
{
	if (CachedInfoUiPresenter)
	{
		return CachedInfoUiPresenter;
	}

	const UWidgetClassDefinition* WidgetDefinition = GetWidgetClassDefinition();
	const TSubclassOf<UInfoUiPresenter> PresenterClass = WidgetDefinition ? WidgetDefinition->GetInfoWidgetSettings().PresenterClass : nullptr;
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

UUiSubsystem* UControllerUiComponent::GetUiSubsystem() const
{
	const APdPlayerController* Controller = GetPdController();
	ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
}

const UWidgetClassDefinition* UControllerUiComponent::GetWidgetClassDefinition() const
{
	return WidgetClassDefinition.Get();
}

void UControllerUiComponent::ApplyStatusViewModelToWidget(UUserWidget* InWidget)
{
	if (UUiSubsystem* UiSubsystem = GetUiSubsystem())
	{
		UiSubsystem->ApplyStatusViewModelToWidget(InWidget);
	}
}

void UControllerUiComponent::OnOpenInfoUiInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	ToggleInfoUi();
}

void UControllerUiComponent::OnSelectPandoraInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	OpenSelectPandoraUi();
}

void UControllerUiComponent::OnSelectPandoraInputEnded(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	CloseSelectPandoraUi();
}
