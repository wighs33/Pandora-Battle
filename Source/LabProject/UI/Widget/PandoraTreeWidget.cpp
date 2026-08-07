#include "UI/Widget/PandoraTreeWidget.h"

#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraComponent.h"
#include "Components/Button.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "Mode/PdHUD.h"
#include "Mode/PdPlayerState.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Settings/LocalPlayerSettingsSubsystem.h"
#include "UI/UiSubsystem.h"
#include "UI/WidgetLookup.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "UI/Widget/PandoraDescriptionWidget.h"
#include "UI/Widget/PandoraWidget.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"
#include "ViewModel/PandoraTreeViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraTreeWidget)

namespace
{
	UPandoraTreeComponent* ResolvePandoraTreeComponentFromTreeWidget(const UUserWidget* Widget)
	{
		if (!Widget)
		{
			return nullptr;
		}

		if (APlayerController* PlayerController = Widget->GetOwningPlayer())
		{
			if (APdPlayerState* PlayerState = PlayerController->GetPlayerState<APdPlayerState>())
			{
				return PlayerState->GetPandoraTreeComponent();
			}
		}

		if (APawn* OwningPawn = Widget->GetOwningPlayerPawn())
		{
			if (APdPlayerState* PlayerState = OwningPawn->GetPlayerState<APdPlayerState>())
			{
				return PlayerState->GetPandoraTreeComponent();
			}
		}

		return nullptr;
	}

	void DisablePreviewCameraLetterboxing(AActor* ViewTarget)
	{
		if (!IsValid(ViewTarget))
		{
			return;
		}

		TArray<UCameraComponent*> CameraComponents;
		ViewTarget->GetComponents<UCameraComponent>(CameraComponents);
		for (UCameraComponent* CameraComponent : CameraComponents)
		{
			if (!IsValid(CameraComponent))
			{
				continue;
			}

			CameraComponent->SetConstraintAspectRatio(false);
			CameraComponent->bOverrideAspectRatioAxisConstraint = false;
		}

	}
}

UPandoraTreeWidget::UPandoraTreeWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UPandoraTreeWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplyWidgetDefinitionSettings();
	ResolveControlWidgets();
	GetOrCreatePandoraTreeViewModel();
	ApplyPandoraTreeViewModelToMvvmView();
	RefreshPandoraWidgets();
}

void UPandoraTreeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	ApplyWidgetDefinitionSettings();
	ResolvePandoraTreeComponent();
	ResolveControlWidgets();
	GetOrCreatePandoraTreeViewModel();
	ApplyPandoraTreeViewModelToMvvmView();
	ApplyPandoraDefinitionToComponent();
	BindPandoraTreeEvents();
	BindButtonEvents();
	SetPandoraPointsText();
	RefreshPandoraWidgets();
}

void UPandoraTreeWidget::NativeDestruct()
{
	ClearHideTimer();
	UnbindPandoraWidgetEvents();
	PandoraDescriptionRequestStack.Reset();
	HidePandoraDescription();
	if (PandoraDescriptionWidget)
	{
		PandoraDescriptionWidget->RemoveFromParent();
		PandoraDescriptionWidget = nullptr;
	}
	UnbindButtonEvents();
	UnbindPandoraTreeEvents();
	if (bReturnCameraOnHide)
	{
		ReturnCameraToPawn(PreviewCameraHideBlendTime);
	}
	DestroyCharacterPreview();
	if (PandoraTreeViewModel && PandoraTreeViewModel->IsViewModelInitialized())
	{
		PandoraTreeViewModel->UninitializeViewModel();
	}

	ReleaseRoutedPandoraInput();
	Super::NativeDestruct();
}

void UPandoraTreeWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (PandoraDescriptionWidget
		&& PandoraDescriptionWidget->GetVisibility() != ESlateVisibility::Collapsed
		&& !ActivePandoraDescriptionAnchor.IsValid())
	{
		ShowTopRequestedPandoraDescription();
	}

	if (ActivePandoraDescriptionAnchor.IsValid()
		&& PandoraDescriptionWidget
		&& PandoraDescriptionWidget->GetVisibility() != ESlateVisibility::Collapsed)
	{
		PositionPandoraDescriptionWidget(ActivePandoraDescriptionAnchor.Get());
	}
}

FReply UPandoraTreeWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (APlayerController* PlayerController = GetOwningPlayer())
		{
			if (APdHUD* Hud = PlayerController->GetHUD<APdHUD>())
			{
				Hud->HandleEscapeInput();
				return FReply::Handled();
			}
		}
	}

	if (!bCloseOnToggleKey || !InKeyEvent.GetKey().IsValid() || !IsTogglePandoraTreeKey(InKeyEvent.GetKey()))
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (APdHUD* Hud = PlayerController->GetHUD<APdHUD>())
		{
			Hud->ClosePandoraTreeUi();
			return FReply::Handled();
		}
	}

	HidePandoraTree();
	return FReply::Handled();
}

void UPandoraTreeWidget::SetPandoraDefinition(UPandoraDefinition* InPandoraDefinition)
{
	if (PandoraDefinition.Get() == InPandoraDefinition)
	{
		return;
	}

	PandoraDefinition = InPandoraDefinition;
	ApplyPandoraDefinitionToComponent();
	RefreshPandoraWidgets();
}

void UPandoraTreeWidget::SetPandoraPointsText()
{
	UPandoraTreeViewModel* ViewModel = GetOrCreatePandoraTreeViewModel();
	if (!ViewModel)
	{
		return;
	}

	if (!PandoraTreeComponent)
	{

		ViewModel->SetPointsAvailable(0);
		ViewModel->SetPandoraPointsText(FText::GetEmpty());
		return;
	}

	const int32 PointsAvailable = PandoraTreeComponent->GetPointsAvailable();
	ViewModel->SetPointsAvailable(PointsAvailable);
	ViewModel->SetPandoraPointsText(FText::Format(PandoraPointsFormat, FText::AsNumber(PointsAvailable)));
}

void UPandoraTreeWidget::RefreshPandoraWidgets()
{
	if (!WidgetTree)
	{
		return;
	}

	WidgetTree->ForEachWidget(
		[this](UWidget* Widget)
		{
			RefreshPandoraWidget(Widget);
		});
}

void UPandoraTreeWidget::ShowPandoraTree()
{
	ClearHideTimer();
	SetVisibility(ESlateVisibility::Visible);
	SetFocus();

	if (SlideInLeft)
	{
		PlayAnimation(SlideInLeft, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
	}

	SpawnCharacterPreview();

	if (!ShouldManageInputModeInternally())
	{
		return;
	}

	if (ApplyRoutedPandoraInput())
	{
		return;
	}

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = true;
		PlayerController->bEnableClickEvents = true;
		PlayerController->bEnableMouseOverEvents = true;
		SetUserFocus(PlayerController);
		SetFocus();
	}
}

void UPandoraTreeWidget::SetInputModeManagedExternally(const bool bManagedExternally)
{
	if (bInputModeManagedExternally == bManagedExternally)
	{
		return;
	}

	bInputModeManagedExternally = bManagedExternally;
	if (bInputModeManagedExternally)
	{
		ReleaseRoutedPandoraInput();
	}
	else if (ShouldManageInputModeInternally()
		&& IsInViewport()
		&& GetVisibility() != ESlateVisibility::Collapsed)
	{
		ApplyRoutedPandoraInput();
	}
}

void UPandoraTreeWidget::HidePandoraTree()
{
	PrepareToHidePandoraTree();

	if (SlideInLeft)
	{
		PlayAnimation(SlideInLeft, 0.0f, 1, EUMGSequencePlayMode::Reverse, 1.0f, false);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				HideTimerHandle,
				this,
				&ThisClass::FinishHidePandoraTree,
				HideAnimationDelay,
				false);
			return;
		}
	}

	FinishHidePandoraTree();
}

void UPandoraTreeWidget::HidePandoraTreeImmediately()
{
	PrepareToHidePandoraTree();
	StopAllAnimations();
	FinishHidePandoraTree();
}

void UPandoraTreeWidget::PrepareToHidePandoraTree()
{
	ClearHideTimer();
	PandoraDescriptionRequestStack.Reset();
	HidePandoraDescription();
	if (bReturnCameraOnHide)
	{
		ReturnCameraToPawn(PreviewCameraHideBlendTime);
	}

	if (ShouldManageInputModeInternally() && !ReleaseRoutedPandoraInput())
	{
		const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
		UUiSubsystem* UiSubsystem = LocalPlayer
			? LocalPlayer->GetSubsystem<UUiSubsystem>()
			: nullptr;
		if ((!UiSubsystem || !UiSubsystem->HasActiveModalInput())
			&& GetOwningPlayer())
		{
			APlayerController* PlayerController = GetOwningPlayer();
			FInputModeGameOnly InputMode;
			PlayerController->SetInputMode(InputMode);
			PlayerController->bShowMouseCursor = false;
			PlayerController->bEnableClickEvents = false;
			PlayerController->bEnableMouseOverEvents = false;
		}
	}
}

void UPandoraTreeWidget::ResetPandora()
{
	ResolvePandoraTreeComponent();
	if (PandoraTreeComponent)
	{
		PandoraTreeComponent->ResetPandora();
	}
}

void UPandoraTreeWidget::ShowPandoraDescriptionAtWidget(
	UPandoraDefinition* InPandoraDefinition,
	UPandoraTreeComponent* InPandoraTreeComponent,
	const UWidget* AnchorWidget)
{
	const APlayerController* OwningPlayer = GetOwningPlayer();
	if (!OwningPlayer || !OwningPlayer->IsLocalController())
	{
		return;
	}

	if (!InPandoraDefinition || !AnchorWidget)
	{
		HidePandoraDescription(AnchorWidget);
		return;
	}

	UPandoraDescriptionWidget* DescriptionWidget = GetOrCreatePandoraDescriptionWidget();
	if (!DescriptionWidget)
	{
		return;
	}

	if (ActivePandoraDescriptionAnchor.Get() == AnchorWidget
		&& ActivePandoraDescriptionDefinition.Get() == InPandoraDefinition
		&& DescriptionWidget->GetVisibility() != ESlateVisibility::Collapsed)
	{
		return;
	}

	ActivePandoraDescriptionAnchor = const_cast<UWidget*>(AnchorWidget);
	ActivePandoraDescriptionDefinition = InPandoraDefinition;
	DescriptionWidget->SetPandoraDefinition(InPandoraDefinition);
	DescriptionWidget->SetPandoraTreeComponent(InPandoraTreeComponent);
	DescriptionWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	PositionPandoraDescriptionWidget(AnchorWidget);
	DescriptionWidget->PlayShowAnimation();
}

void UPandoraTreeWidget::HidePandoraDescription(const UWidget* RequestingAnchorWidget)
{
	if (RequestingAnchorWidget && ActivePandoraDescriptionAnchor.Get() != RequestingAnchorWidget)
	{
		return;
	}

	if (PandoraDescriptionWidget)
	{
		PandoraDescriptionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	ActivePandoraDescriptionAnchor.Reset();
	ActivePandoraDescriptionDefinition.Reset();
}

void UPandoraTreeWidget::RefreshActivePandoraDescription()
{
	if (!PandoraDescriptionWidget
		|| PandoraDescriptionWidget->GetVisibility() == ESlateVisibility::Collapsed
		|| !ActivePandoraDescriptionDefinition.IsValid()
		|| !ActivePandoraDescriptionAnchor.IsValid())
	{
		return;
	}

	PandoraDescriptionWidget->SetPandoraDefinition(ActivePandoraDescriptionDefinition.Get());
	PandoraDescriptionWidget->SetPandoraTreeComponent(PandoraTreeComponent.Get());
	PositionPandoraDescriptionWidget(ActivePandoraDescriptionAnchor.Get());
}

void UPandoraTreeWidget::PrunePandoraDescriptionRequests()
{
	PandoraDescriptionRequestStack.RemoveAll(
		[](const TWeakObjectPtr<UPandoraWidget>& RequestedWidget)
		{
			const UPandoraWidget* PandoraWidget = RequestedWidget.Get();
			return !IsValid(PandoraWidget) || !PandoraWidget->IsPandoraDescriptionRequested();
		});
}

void UPandoraTreeWidget::ShowTopRequestedPandoraDescription()
{
	PrunePandoraDescriptionRequests();
	if (PandoraDescriptionRequestStack.IsEmpty())
	{
		HidePandoraDescription();
		return;
	}

	UPandoraWidget* PandoraWidget = PandoraDescriptionRequestStack.Last().Get();
	if (!IsValid(PandoraWidget))
	{
		HidePandoraDescription();
		return;
	}

	ShowPandoraDescriptionAtWidget(
		PandoraWidget->GetPandoraDefinition(),
		PandoraTreeComponent.Get(),
		PandoraWidget->GetPandoraDescriptionAnchorWidget());
}

void UPandoraTreeWidget::HandlePandoraStateChanged()
{
	RefreshPandoraWidgets();
	RefreshActivePandoraDescription();
}

void UPandoraTreeWidget::HandlePandoraPointsChanged(int32 NewPointsAvailable)
{
	(void)NewPointsAvailable;
	SetPandoraPointsText();
	RefreshPandoraWidgets();
	RefreshActivePandoraDescription();
}

void UPandoraTreeWidget::HandleResetPandoraClicked()
{
	ResetPandora();
}

void UPandoraTreeWidget::HandleLoadoutClicked()
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (APdHUD* Hud = PlayerController->GetHUD<APdHUD>())
		{
			const TWeakObjectPtr<APdHUD> WeakHud = Hud;
			FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda(
					[WeakHud](float)
					{
						if (APdHUD* ValidHud = WeakHud.Get())
						{
							ValidHud->OpenInfoUiFocused(EInfoUiSection::Pandora);
						}

						return false;
					}));
		}
	}
}

void UPandoraTreeWidget::HandlePandoraDescriptionRequested(UPandoraWidget* PandoraWidget)
{
	if (!IsValid(PandoraWidget))
	{
		return;
	}

	PandoraDescriptionRequestStack.RemoveAll(
		[PandoraWidget](const TWeakObjectPtr<UPandoraWidget>& RequestedWidget)
		{
			return RequestedWidget.Get() == PandoraWidget;
		});
	PandoraDescriptionRequestStack.Add(PandoraWidget);
	ShowTopRequestedPandoraDescription();
}

void UPandoraTreeWidget::HandlePandoraDescriptionDismissed(UPandoraWidget* PandoraWidget)
{
	const UWidget* DismissedAnchor =
		IsValid(PandoraWidget) ? PandoraWidget->GetPandoraDescriptionAnchorWidget() : nullptr;
	PandoraDescriptionRequestStack.RemoveAll(
		[PandoraWidget](const TWeakObjectPtr<UPandoraWidget>& RequestedWidget)
		{
			return RequestedWidget.Get() == PandoraWidget;
		});

	if (!DismissedAnchor || ActivePandoraDescriptionAnchor.Get() == DismissedAnchor)
	{
		ShowTopRequestedPandoraDescription();
	}
}

void UPandoraTreeWidget::HandlePandoraTreeFocusRequested(UPandoraWidget* PandoraWidget)
{
	if (IsValid(PandoraWidget))
	{
		SetFocus();
	}
}

void UPandoraTreeWidget::ResolvePandoraTreeComponent()
{
	if (!PandoraTreeComponent)
	{
		PandoraTreeComponent = ResolvePandoraTreeComponentFromTreeWidget(this);
	}

	if (!PandoraDefinition && PandoraTreeComponent)
	{
		PandoraDefinition = PandoraTreeComponent->GetPandoraDefinition();
	}

}

void UPandoraTreeWidget::ApplyWidgetDefinitionSettings()
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const FPandoraTreeWidgetSettings& Settings = WidgetDefinition->GetPandoraTreeWidgetSettings();
		PandoraPointsFormat = Settings.PandoraPointsFormat;
		bCloseOnToggleKey = Settings.bCloseOnToggleKey;
		bSetInputModeOnShowHide = Settings.bSetInputModeOnShowHide;
		HideAnimationDelay = Settings.HideAnimationDelay;
		bUseCharacterPreviewCamera = Settings.bUseCharacterPreviewCamera;
		PreviewCameraShowBlendTime = Settings.PreviewCameraShowBlendTime;
		PreviewCameraHideBlendTime = Settings.PreviewCameraHideBlendTime;
		bReturnCameraOnHide = Settings.bReturnCameraOnHide;

		// Preserve the PandoraTree widget's own preview class (for example,
		// BP_CharacterPreviewRight). The shared definition is only a fallback.
		if (!CharacterPreviewClass)
		{
			CharacterPreviewClass = WidgetDefinition->GetCharacterPreviewClass();
		}
	}
}

void UPandoraTreeWidget::ResolveControlWidgets()
{
	if (!ResetPandoraButton)
	{
		ResetPandoraButton = PdWidgetLookup::FindWidgetByNames<UButton>(this, {
			TEXT("ResetPandoraButton"),
			TEXT("PandoraResetButton")
		});
	}

	if (!Btn_Loadout)
	{
		Btn_Loadout = PdWidgetLookup::FindWidgetByNames<UButton>(this, {
			TEXT("Btn_Loadout"),
			TEXT("LoadoutButton")
		});
	}

}

UPandoraTreeViewModel* UPandoraTreeWidget::GetOrCreatePandoraTreeViewModel()
{
	if (!PandoraTreeViewModel)
	{
		PandoraTreeViewModel = NewObject<UPandoraTreeViewModel>(this);
	}

	if (PandoraTreeViewModel && !PandoraTreeViewModel->IsViewModelInitialized())
	{
		PandoraTreeViewModel->InitializeViewModel(this);
	}

	return PandoraTreeViewModel.Get();
}

void UPandoraTreeWidget::ApplyPandoraTreeViewModelToMvvmView()
{
	if (!PandoraTreeViewModel)
	{
		return;
	}

	UMVVMView* ViewExtension = GetExtension<UMVVMView>();
	if (!ViewExtension)
	{
		return;
	}

	const UMVVMViewClass* ViewClass = ViewExtension->GetViewClass();
	if (!ViewClass)
	{
		return;
	}

	FName RuntimeViewModelName = NAME_None;
	for (const FMVVMViewClass_Source& Source : ViewClass->GetSources())
	{
		if (!Source.IsViewModel() || !Source.CanBeSet())
		{
			continue;
		}

		const UClass* SourceClass = Source.GetSourceClass();
		if (SourceClass && PandoraTreeViewModel->GetClass()->IsChildOf(SourceClass))
		{
			RuntimeViewModelName = Source.GetName();
			break;
		}
	}

	if (RuntimeViewModelName.IsNone())
	{

		return;
	}

	ViewExtension->SetViewModel(RuntimeViewModelName, PandoraTreeViewModel);
}

void UPandoraTreeWidget::BindPandoraTreeEvents()
{
	if (!PandoraTreeComponent)
	{
		return;
	}

	PandoraTreeComponent->OnPandorasChanged.RemoveDynamic(this, &ThisClass::HandlePandoraStateChanged);
	PandoraTreeComponent->OnPandorasChanged.AddUniqueDynamic(this, &ThisClass::HandlePandoraStateChanged);
	PandoraTreeComponent->OnPointsChanged.RemoveDynamic(this, &ThisClass::HandlePandoraPointsChanged);
	PandoraTreeComponent->OnPointsChanged.AddUniqueDynamic(this, &ThisClass::HandlePandoraPointsChanged);
}

void UPandoraTreeWidget::BindButtonEvents()
{
	ResolveControlWidgets();

	if (ResetPandoraButton)
	{
		ResetPandoraButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleResetPandoraClicked);
		ResetPandoraButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleResetPandoraClicked);
	}

	if (Btn_Loadout)
	{
		Btn_Loadout->OnClicked.RemoveDynamic(this, &ThisClass::HandleLoadoutClicked);
		Btn_Loadout->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLoadoutClicked);
	}
}

void UPandoraTreeWidget::UnbindButtonEvents()
{
	if (ResetPandoraButton)
	{
		ResetPandoraButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleResetPandoraClicked);
	}

	if (Btn_Loadout)
	{
		Btn_Loadout->OnClicked.RemoveDynamic(this, &ThisClass::HandleLoadoutClicked);
	}
}

void UPandoraTreeWidget::UnbindPandoraTreeEvents()
{
	if (!PandoraTreeComponent)
	{
		return;
	}

	PandoraTreeComponent->OnPandorasChanged.RemoveDynamic(this, &ThisClass::HandlePandoraStateChanged);
	PandoraTreeComponent->OnPointsChanged.RemoveDynamic(this, &ThisClass::HandlePandoraPointsChanged);
}

void UPandoraTreeWidget::ApplyPandoraDefinitionToComponent()
{
	if (PandoraTreeComponent && PandoraDefinition)
	{
		PandoraTreeComponent->SetPandoraDefinition(PandoraDefinition.Get());
	}
}

void UPandoraTreeWidget::RefreshPandoraWidget(UWidget* Widget)
{
	if (UPandoraWidget* PandoraWidget = Cast<UPandoraWidget>(Widget))
	{
		BindPandoraWidgetEvents(PandoraWidget);
		PandoraWidget->SetPandoraTreeComponent(PandoraTreeComponent.Get());
		if (!PandoraWidget->GetPandoraDefinition() && PandoraDefinition)
		{
			PandoraWidget->SetPandoraDefinition(PandoraDefinition.Get());
		}

		PandoraWidget->SetPandoraInfo();

		return;
	}

}

void UPandoraTreeWidget::BindPandoraWidgetEvents(UPandoraWidget* PandoraWidget)
{
	if (!IsValid(PandoraWidget))
	{
		return;
	}

	PandoraWidget->OnPandoraDescriptionRequested.RemoveDynamic(
		this,
		&ThisClass::HandlePandoraDescriptionRequested);
	PandoraWidget->OnPandoraDescriptionDismissed.RemoveDynamic(
		this,
		&ThisClass::HandlePandoraDescriptionDismissed);
	PandoraWidget->OnPandoraTreeFocusRequested.RemoveDynamic(
		this,
		&ThisClass::HandlePandoraTreeFocusRequested);

	PandoraWidget->OnPandoraDescriptionRequested.AddUniqueDynamic(
		this,
		&ThisClass::HandlePandoraDescriptionRequested);
	PandoraWidget->OnPandoraDescriptionDismissed.AddUniqueDynamic(
		this,
		&ThisClass::HandlePandoraDescriptionDismissed);
	PandoraWidget->OnPandoraTreeFocusRequested.AddUniqueDynamic(
		this,
		&ThisClass::HandlePandoraTreeFocusRequested);
}

void UPandoraTreeWidget::UnbindPandoraWidgetEvents()
{
	if (!WidgetTree)
	{
		return;
	}

	WidgetTree->ForEachWidget(
		[this](UWidget* Widget)
		{
			UPandoraWidget* PandoraWidget = Cast<UPandoraWidget>(Widget);
			if (!IsValid(PandoraWidget))
			{
				return;
			}

			PandoraWidget->OnPandoraDescriptionRequested.RemoveDynamic(
				this,
				&ThisClass::HandlePandoraDescriptionRequested);
			PandoraWidget->OnPandoraDescriptionDismissed.RemoveDynamic(
				this,
				&ThisClass::HandlePandoraDescriptionDismissed);
			PandoraWidget->OnPandoraTreeFocusRequested.RemoveDynamic(
				this,
				&ThisClass::HandlePandoraTreeFocusRequested);
		});
}

void UPandoraTreeWidget::ResolveTogglePandoraTreeAction()
{
	if (TogglePandoraTreeAction)
	{
		return;
	}

	if (const UWidgetClassDefinition* WidgetDefinition =
		UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		TogglePandoraTreeAction =
			WidgetDefinition->GetTogglePandoraTreeInputAction().Get();
	}
}

bool UPandoraTreeWidget::IsTogglePandoraTreeKey(const FKey& Key) const
{
	const_cast<UPandoraTreeWidget*>(this)->ResolveTogglePandoraTreeAction();
	if (!TogglePandoraTreeAction)
	{
		return false;
	}

	const ULocalPlayerSettingsSubsystem* LocalPlayerSettings = ULocalPlayerSettingsSubsystem::Get(GetOwningPlayer());
	if (!LocalPlayerSettings)
	{
		return false;
	}

	const TArray<FKey> MappedKeys = LocalPlayerSettings->QueryKeysMappedToAction(TogglePandoraTreeAction);
	return MappedKeys.Contains(Key);
}

void UPandoraTreeWidget::ResolveCharacterPreviewClass()
{
	if (CharacterPreviewClass)
	{
		return;
	}

	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		CharacterPreviewClass = WidgetDefinition->GetCharacterPreviewClass();
	}
}

void UPandoraTreeWidget::SpawnCharacterPreview()
{
	if (!bUseCharacterPreviewCamera)
	{
		return;
	}

	if (IsValid(SpawnedCharacterPreview))
	{
		DisablePreviewCameraLetterboxing(SpawnedCharacterPreview.Get());

		if (APlayerController* PlayerController = GetOwningPlayer())
		{
			PlayerController->SetViewTargetWithBlend(
				SpawnedCharacterPreview.Get(),
				PreviewCameraShowBlendTime,
				VTBlend_Cubic);
		}
		return;
	}

	ResolveCharacterPreviewClass();

	APawn* OwningPawn = GetOwningPlayerPawn();
	USkeletalMeshComponent* MeshComponent = OwningPawn
		? OwningPawn->FindComponentByClass<USkeletalMeshComponent>()
		: nullptr;
	UWorld* World = GetWorld();
	if (!World || !CharacterPreviewClass || !MeshComponent)
	{
		return;
	}

	SpawnedCharacterPreview = World->SpawnActor<AActor>(CharacterPreviewClass, FTransform::Identity);
	if (!SpawnedCharacterPreview)
	{
		return;
	}

	SpawnedCharacterPreview->AttachToComponent(
		MeshComponent,
		FAttachmentTransformRules(
			EAttachmentRule::KeepRelative,
			EAttachmentRule::KeepRelative,
			EAttachmentRule::KeepRelative,
			true));

	DisablePreviewCameraLetterboxing(SpawnedCharacterPreview.Get());

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetViewTargetWithBlend(
			SpawnedCharacterPreview.Get(),
			PreviewCameraShowBlendTime,
			VTBlend_Cubic);
	}
}

void UPandoraTreeWidget::ReturnCameraToPawn(float BlendTime) const
{
	APlayerController* PlayerController = GetOwningPlayer();
	APawn* OwningPawn = GetOwningPlayerPawn();
	if (PlayerController && OwningPawn)
	{
		PlayerController->SetViewTargetWithBlend(OwningPawn, BlendTime, VTBlend_Cubic);
	}
}

void UPandoraTreeWidget::DestroyCharacterPreview()
{
	if (IsValid(SpawnedCharacterPreview))
	{
		SpawnedCharacterPreview->Destroy();
	}

	SpawnedCharacterPreview = nullptr;
}

UPandoraDescriptionWidget* UPandoraTreeWidget::GetOrCreatePandoraDescriptionWidget()
{
	if (PandoraDescriptionWidget)
	{
		return PandoraDescriptionWidget.Get();
	}

	const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this);
	const TSubclassOf<UPandoraDescriptionWidget> DescriptionWidgetClass =
		WidgetDefinition ? WidgetDefinition->GetPandoraDescriptionWidgetClass() : nullptr;
	if (!DescriptionWidgetClass)
	{
		return nullptr;
	}

	PandoraDescriptionWidget = CreateWidget<UPandoraDescriptionWidget>(GetOwningPlayer(), DescriptionWidgetClass);
	if (PandoraDescriptionWidget)
	{
		PandoraDescriptionWidget->AddToViewport(100);
		PandoraDescriptionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	return PandoraDescriptionWidget.Get();
}

void UPandoraTreeWidget::PositionPandoraDescriptionWidget(const UWidget* AnchorWidget) const
{
	if (!PandoraDescriptionWidget || !AnchorWidget)
	{
		return;
	}

	const FGeometry& AnchorGeometry = AnchorWidget->GetCachedGeometry();
	FVector2D PixelPosition;
	FVector2D ViewportPosition;
	USlateBlueprintLibrary::LocalToViewport(
		this,
		AnchorGeometry,
		FVector2D(AnchorGeometry.GetLocalSize().X, 0.0f),
		PixelPosition,
		ViewportPosition);

	PandoraDescriptionWidget->ForceLayoutPrepass();
	const FVector2D DesiredSize = PandoraDescriptionWidget->GetDesiredSize();
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
	FVector2D PopupPosition = ViewportPosition + FVector2D(16.0f, 0.0f);

	if (ViewportSize.X > 0.0f && DesiredSize.X > 0.0f)
	{
		PopupPosition.X = FMath::Clamp(PopupPosition.X, 0.0f, FMath::Max(ViewportSize.X - DesiredSize.X, 0.0f));
	}
	if (ViewportSize.Y > 0.0f && DesiredSize.Y > 0.0f)
	{
		PopupPosition.Y = FMath::Clamp(PopupPosition.Y, 0.0f, FMath::Max(ViewportSize.Y - DesiredSize.Y, 0.0f));
	}

	PandoraDescriptionWidget->SetPositionInViewport(PopupPosition, false);
}

bool UPandoraTreeWidget::ShouldManageInputModeInternally() const
{
	return bSetInputModeOnShowHide && !bInputModeManagedExternally;
}

bool UPandoraTreeWidget::ApplyRoutedPandoraInput()
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	UUiSubsystem* UiSubsystem = LocalPlayer
		? LocalPlayer->GetSubsystem<UUiSubsystem>()
		: nullptr;
	if (!UiSubsystem)
	{
		return false;
	}

	FUiModalInputConfig InputConfig;
	InputConfig.InputMode = EUiInputMode::GameAndUI;
	InputConfig.bHideCursorDuringCapture = false;
	InputConfig.bShowMouseCursor = true;
	InputConfig.bEnableClickEvents = true;
	InputConfig.bEnableMouseOverEvents = true;
	InputConfig.RestorePolicy = EUiInputRestorePolicy::Gameplay;

	if (UiSubsystem->UpdateModalInput(
		this,
		PandoraModalInputToken,
		this,
		InputConfig))
	{
		return true;
	}

	PandoraModalInputToken.Invalidate();
	PandoraModalInputToken = UiSubsystem->AcquireModalInput(this, this, InputConfig);
	return PandoraModalInputToken.IsValid();
}

bool UPandoraTreeWidget::ReleaseRoutedPandoraInput()
{
	if (!PandoraModalInputToken.IsValid())
	{
		return false;
	}

	bool bReleased = false;
	if (const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UUiSubsystem* UiSubsystem = LocalPlayer->GetSubsystem<UUiSubsystem>())
		{
			bReleased = UiSubsystem->ReleaseModalInput(this, PandoraModalInputToken);
		}
	}

	PandoraModalInputToken.Invalidate();
	return bReleased;
}

void UPandoraTreeWidget::FinishHidePandoraTree()
{
	HideTimerHandle.Invalidate();
	DestroyCharacterPreview();
	RemoveFromParent();
	OnPandoraTreeClosed.Broadcast(this);
}

void UPandoraTreeWidget::ClearHideTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}

	HideTimerHandle.Invalidate();
}
