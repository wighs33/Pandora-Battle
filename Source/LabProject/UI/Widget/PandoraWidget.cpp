#include "UI/Widget/PandoraWidget.h"

#include "AbilitySystem/PandoraTree/PandoraTreeComponent.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraDefinition.h"
#include "UI/Widget/PandoraDescriptionWidget.h"
#include "UI/Widget/PandoraTreeWidget.h"
#include "UI/Widget/PandoraWidgetViewData.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"
#include "ViewModel/PandoraWidgetViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraWidget)

DEFINE_LOG_CATEGORY_STATIC(LogPandoraWidget, Log, All);

namespace
{
	const FLinearColor PandoraButtonTransparentColor(1.0f, 1.0f, 1.0f, 0.0f);

	UPandoraTreeComponent* ResolvePandoraTreeComponentFromWidget(const UUserWidget* Widget)
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

}

void UPandoraWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ResolveControlWidgets();
	GetOrCreatePandoraWidgetViewModel();
	ApplyPandoraWidgetViewModelToMvvmView();
	ApplyDesignerDefaults();
	ResolvePandoraTreeWidget();
	SetPandoraInfo();
}

void UPandoraWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ResolveControlWidgets();
	GetOrCreatePandoraWidgetViewModel();
	ApplyPandoraWidgetViewModelToMvvmView();
	ResolvePandoraTreeComponent();
	BindPandoraTreeEvents();
	BindButtonEvents();
	ApplyDesignerDefaults();
	SetPandoraInfo();
	ResolvePandoraTreeWidget();
	SetupPandoraDescriptionPopupWidget();

	UE_LOG(LogPandoraWidget, Log,
		TEXT("[Construct] widget=%s treeComponent=%s treeWidget=%s pandora=%s button=%s progress=%s descClass=%s popup=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PandoraTreeComponent.Get()),
		*GetNameSafe(ResolvedPandoraTreeWidget.Get()),
		*GetNameSafe(PandoraDefinition.Get()),
		*GetNameSafe(Button.Get()),
		*GetNameSafe(ButtonProgressBar.Get()),
		*GetNameSafe(PandoraDescriptionPopupWidgetClass.Get()),
		*GetNameSafe(CreatedPandoraDescriptionPopup.Get()));
}

void UPandoraWidget::NativeDestruct()
{
	RemovePandoraDescriptionPopup();
	ClearFocusCheckTimer();
	ClearButtonPressTimer();
	UnbindButtonEvents();
	UnbindPandoraTreeEvents();
	if (PandoraWidgetViewModel && PandoraWidgetViewModel->IsViewModelInitialized())
	{
		PandoraWidgetViewModel->UninitializeViewModel();
	}

	Super::NativeDestruct();
}

void UPandoraWidget::SetPandoraDefinition(UPandoraDefinition* InPandoraDefinition)
{
	if (PandoraDefinition.Get() == InPandoraDefinition)
	{
		return;
	}

	PandoraDefinition = InPandoraDefinition;
	UE_LOG(LogPandoraWidget, Log,
		TEXT("[SetPandoraDefinition] widget=%s pandora=%s icon=%s activeIcon=%s max=%d skills=%d"),
		*GetNameSafe(this),
		*GetNameSafe(PandoraDefinition.Get()),
		PandoraDefinition ? *GetNameSafe(PandoraDefinition->GetIconResource()) : TEXT("None"),
		PandoraDefinition ? *GetNameSafe(PandoraDefinition->GetActiveIconResource()) : TEXT("None"),
		PandoraDefinition ? PandoraDefinition->GetMaxLevel() : 0,
		PandoraDefinition ? PandoraDefinition->Skill.Num() : 0);
	SetPandoraInfo();
	UpdatePandoraDescriptionDetails();
}

void UPandoraWidget::SetPandoraTreeComponent(UPandoraTreeComponent* InPandoraTreeComponent)
{
	if (PandoraTreeComponent.Get() == InPandoraTreeComponent)
	{
		return;
	}

	UnbindPandoraTreeEvents();
	PandoraTreeComponent = InPandoraTreeComponent;
	if (!PandoraDefinition && PandoraTreeComponent)
	{
		PandoraDefinition = PandoraTreeComponent->GetPandoraDefinition();
	}
	UE_LOG(LogPandoraWidget, Log,
		TEXT("[SetTreeComponent] widget=%s treeComponent=%s resolvedPandora=%s points=%d"),
		*GetNameSafe(this),
		*GetNameSafe(PandoraTreeComponent.Get()),
		*GetNameSafe(PandoraDefinition.Get()),
		PandoraTreeComponent ? PandoraTreeComponent->GetPointsAvailable() : INDEX_NONE);
	BindPandoraTreeEvents();
	SetPandoraInfo();
	UpdatePandoraDescriptionDetails();
}

void UPandoraWidget::SetPandoraInfo()
{
	UPandoraWidgetViewModel* ViewModel = GetOrCreatePandoraWidgetViewModel();

	if (ButtonProgressBar)
	{
		ButtonProgressBar->SetPercent(0.0f);
		ButtonProgressBar->SetRenderOpacity(1.0f);
	}

	FPandoraWidgetStyleConfig Style;
	Style.AvailableOverlayColor = AvailableOverlayColor;
	Style.UnavailableOverlayColor = UnavailableOverlayColor;
	Style.LockedOverlayColor = LockedOverlayColor;
	Style.NotEnoughPointsOverlayColor = NotEnoughPointsOverlayColor;
	Style.AvailableContentOpacity = AvailableContentOpacity;
	Style.UnavailableContentOpacity = UnavailableContentOpacity;
	Style.bShowMaxText = bShowMaxText;

	const FPandoraWidgetViewData ViewData = FPandoraWidgetViewDataBuilder::Build(
		PandoraDefinition.Get(),
		PandoraTreeComponent.Get(),
		Style);

	if (ViewModel)
	{
		ViewModel->SetDisplayName(ViewData.DisplayName);
		ViewModel->SetLevelText(ViewData.LevelText);
		ViewModel->SetIconResource(ViewData.IconResource);
		ViewModel->SetOverlayColor(ViewData.OverlayColor);
		ViewModel->SetContentOpacity(ViewData.ContentOpacity);
		ViewModel->SetStateIconVisibility(ViewData.StateIconVisibility);
		ViewModel->SetStateIconColor(ViewData.StateIconColor);
		ViewModel->SetCanSpend(ViewData.bCanSpend);
		ViewModel->SetIsLocked(ViewData.bLocked);
		ViewModel->SetNotEnoughPoints(ViewData.bNotEnoughPoints);
		ViewModel->SetAtMaxLevel(ViewData.bAtMaxLevel);
	}

	UE_LOG(LogPandoraWidget, Log,
		TEXT("[SetInfo] widget=%s pandora=%s tree=%s current=%d max=%d points=%d required=%d canSpend=%s unlockRulesMet=%s locked=%s notEnoughPoints=%s atMax=%s inactiveStyle=%s dimmedStyle=%s button=%s progress=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PandoraDefinition.Get()),
		*GetNameSafe(PandoraTreeComponent.Get()),
		ViewData.CurrentLevel,
		ViewData.MaxLevel,
		ViewData.PointsAvailable,
		ViewData.RequiredPoints,
		ViewData.bCanSpend ? TEXT("true") : TEXT("false"),
		ViewData.bUnlockRulesMet ? TEXT("true") : TEXT("false"),
		ViewData.bLocked ? TEXT("true") : TEXT("false"),
		ViewData.bNotEnoughPoints ? TEXT("true") : TEXT("false"),
		ViewData.bAtMaxLevel ? TEXT("true") : TEXT("false"),
		ViewData.bInactiveStyle ? TEXT("true") : TEXT("false"),
		ViewData.bDimmedStyle ? TEXT("true") : TEXT("false"),
		*GetNameSafe(Button.Get()),
		*GetNameSafe(ButtonProgressBar.Get()));
}

void UPandoraWidget::ConfirmSpendPointOnPandora()
{
	if (!PandoraTreeComponent)
	{
		UE_LOG(LogPandoraWidget, Warning,
			TEXT("[ConfirmSpend] blocked: no tree component. widget=%s pandora=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PandoraDefinition.Get()));
		return;
	}

	if (PandoraDefinition)
	{
		if (PandoraTreeComponent->CanSpendPointOnPandora(PandoraDefinition.Get()))
		{
			UE_LOG(LogPandoraWidget, Log,
				TEXT("[ConfirmSpend] spending pandora. widget=%s component=%s pandora=%s points=%d current=%d max=%d required=%d"),
				*GetNameSafe(this),
				*GetNameSafe(PandoraTreeComponent.Get()),
				*GetNameSafe(PandoraDefinition.Get()),
				PandoraTreeComponent->GetPointsAvailable(),
				PandoraTreeComponent->GetCurrentPandoraLevel(PandoraDefinition.Get()),
				PandoraTreeComponent->GetMaxPandoraLevel(PandoraDefinition.Get()),
				PandoraTreeComponent->GetRequiredPointsForPandora(PandoraDefinition.Get(), PandoraTreeComponent->HasGrantedPandora(PandoraDefinition.Get())));
			PandoraTreeComponent->SpendPointOnPandora(PandoraDefinition.Get());
		}
		else
		{
			UE_LOG(LogPandoraWidget, Warning,
				TEXT("[ConfirmSpend] blocked by CanSpendPointOnPandora=false. widget=%s component=%s pandora=%s points=%d current=%d max=%d required=%d"),
				*GetNameSafe(this),
				*GetNameSafe(PandoraTreeComponent.Get()),
				*GetNameSafe(PandoraDefinition.Get()),
				PandoraTreeComponent->GetPointsAvailable(),
				PandoraTreeComponent->GetCurrentPandoraLevel(PandoraDefinition.Get()),
				PandoraTreeComponent->GetMaxPandoraLevel(PandoraDefinition.Get()),
				PandoraTreeComponent->GetRequiredPointsForPandora(PandoraDefinition.Get(), PandoraTreeComponent->HasGrantedPandora(PandoraDefinition.Get())));
		}
		return;
	}

	UE_LOG(LogPandoraWidget, Warning,
		TEXT("[ConfirmSpend] blocked: PandoraDefinition is not set. widget=%s"),
		*GetNameSafe(this));
}

void UPandoraWidget::IncrementButtonTimer()
{
	if (ButtonHoldDuration <= 0.0)
	{
		ConfirmSpendPointOnPandora();
		ResetButtonPress();
		return;
	}

	ButtonHoldElapsedTime += ButtonHoldUpdateInterval;

	const float PressPercent = FMath::Clamp(
		static_cast<float>(ButtonHoldElapsedTime / ButtonHoldDuration),
		0.0f,
		1.0f);
	if (ButtonProgressBar)
	{
		ButtonProgressBar->SetPercent(PressPercent);
	}

	if (ButtonHoldElapsedTime >= ButtonHoldDuration)
	{
		ConfirmSpendPointOnPandora();
		ResetButtonPress();
	}
}

void UPandoraWidget::ResetButtonPress()
{
	ButtonHoldElapsedTime = 0.0;
	ClearButtonPressTimer();

	if (ButtonProgressBar)
	{
		ButtonProgressBar->SetPercent(0.0f);
	}

	if (UPandoraTreeWidget* PandoraTreeWidget = GetTypedOuter<UPandoraTreeWidget>())
	{
		PandoraTreeWidget->SetFocus();
	}
}

void UPandoraWidget::HandleButtonPressed()
{
	const bool bCanSpend = PandoraTreeComponent
		&& PandoraDefinition
		&& PandoraTreeComponent->CanSpendPointOnPandora(PandoraDefinition.Get());
	if (!bCanSpend)
	{
		UE_LOG(LogPandoraWidget, Warning,
			TEXT("[ButtonPressed] blocked. widget=%s tree=%s pandora=%s points=%d current=%d max=%d required=%d hasPandora=%s canSpend=%s button=%s progress=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PandoraTreeComponent.Get()),
			*GetNameSafe(PandoraDefinition.Get()),
			PandoraTreeComponent ? PandoraTreeComponent->GetPointsAvailable() : INDEX_NONE,
			PandoraTreeComponent && PandoraDefinition ? PandoraTreeComponent->GetCurrentPandoraLevel(PandoraDefinition.Get()) : INDEX_NONE,
			PandoraTreeComponent && PandoraDefinition ? PandoraTreeComponent->GetMaxPandoraLevel(PandoraDefinition.Get()) : INDEX_NONE,
			PandoraTreeComponent && PandoraDefinition ? PandoraTreeComponent->GetRequiredPointsForPandora(PandoraDefinition.Get(), PandoraTreeComponent->HasGrantedPandora(PandoraDefinition.Get())) : INDEX_NONE,
			PandoraTreeComponent && PandoraDefinition && PandoraTreeComponent->HasGrantedPandora(PandoraDefinition.Get()) ? TEXT("true") : TEXT("false"),
			bCanSpend ? TEXT("true") : TEXT("false"),
			*GetNameSafe(Button.Get()),
			*GetNameSafe(ButtonProgressBar.Get()));
		return;
	}

	UE_LOG(LogPandoraWidget, Log,
		TEXT("[ButtonPressed] accepted. widget=%s pandora=%s holdDuration=%.3f updateInterval=%.3f"),
		*GetNameSafe(this),
		*GetNameSafe(PandoraDefinition.Get()),
		ButtonHoldDuration,
		ButtonHoldUpdateInterval);

	if (ButtonHoldUpdateInterval <= 0.0f)
	{
		ConfirmSpendPointOnPandora();
		ResetButtonPress();
		return;
	}

	ClearButtonPressTimer();
	ButtonHoldElapsedTime = 0.0;
	if (ButtonProgressBar)
	{
		ButtonProgressBar->SetPercent(0.0f);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ButtonHoldTimerHandle,
			this,
			&ThisClass::IncrementButtonTimer,
			ButtonHoldUpdateInterval,
			true);
		UE_LOG(LogPandoraWidget, Log,
			TEXT("[ButtonPressed] hold timer started. widget=%s pandora=%s progressWidget=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PandoraDefinition.Get()),
			*GetNameSafe(ButtonProgressBar.Get()));
	}
}

void UPandoraWidget::HandleButtonReleased()
{
	ResetButtonPress();
}

void UPandoraWidget::HandlePandoraStateChanged()
{
	SetPandoraInfo();
	UpdatePandoraDescriptionDetails();
}

void UPandoraWidget::HandlePandoraPointsChanged(int32 NewPointsAvailable)
{
	(void)NewPointsAvailable;
	SetPandoraInfo();
	UpdatePandoraDescriptionDetails();
}

void UPandoraWidget::ResolvePandoraTreeComponent()
{
	if (!PandoraTreeComponent)
	{
		PandoraTreeComponent = ResolvePandoraTreeComponentFromWidget(this);
	}

	if (!PandoraDefinition && PandoraTreeComponent)
	{
		PandoraDefinition = PandoraTreeComponent->GetPandoraDefinition();
	}

	UE_LOG(LogPandoraWidget, Log,
		TEXT("[ResolveTreeComponent] widget=%s tree=%s pandora=%s owningPlayer=%s owningPawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PandoraTreeComponent.Get()),
		*GetNameSafe(PandoraDefinition.Get()),
		*GetNameSafe(GetOwningPlayer()),
		*GetNameSafe(GetOwningPlayerPawn()));
}

void UPandoraWidget::BindPandoraTreeEvents()
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

void UPandoraWidget::UnbindPandoraTreeEvents()
{
	if (!PandoraTreeComponent)
	{
		return;
	}

	PandoraTreeComponent->OnPandorasChanged.RemoveDynamic(this, &ThisClass::HandlePandoraStateChanged);
	PandoraTreeComponent->OnPointsChanged.RemoveDynamic(this, &ThisClass::HandlePandoraPointsChanged);
}

void UPandoraWidget::BindButtonEvents()
{
	ResolveControlWidgets();
	if (!Button)
	{
		return;
	}

	Button->OnPressed.RemoveDynamic(this, &ThisClass::HandleButtonPressed);
	Button->OnReleased.RemoveDynamic(this, &ThisClass::HandleButtonReleased);
	Button->OnPressed.AddUniqueDynamic(this, &ThisClass::HandleButtonPressed);
	Button->OnReleased.AddUniqueDynamic(this, &ThisClass::HandleButtonReleased);
}

void UPandoraWidget::UnbindButtonEvents()
{
	ResolveControlWidgets();
	if (!Button)
	{
		return;
	}

	Button->OnPressed.RemoveDynamic(this, &ThisClass::HandleButtonPressed);
	Button->OnReleased.RemoveDynamic(this, &ThisClass::HandleButtonReleased);
}

void UPandoraWidget::ResolveControlWidgets()
{
	if (!Button)
	{
		Button = Cast<UButton>(GetWidgetFromName(TEXT("PandoraButton")));
	}

	if (!ButtonProgressBar)
	{
		ButtonProgressBar = Cast<UProgressBar>(GetWidgetFromName(TEXT("PandoraButtonProgressBar")));
	}

	if (!ButtonProgressBar)
	{
		ButtonProgressBar = Cast<UProgressBar>(GetWidgetFromName(TEXT("PandoraProgressBar")));
	}

	UE_LOG(LogPandoraWidget, Log,
		TEXT("[ResolveControls] widget=%s button=%s progress=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Button.Get()),
		*GetNameSafe(ButtonProgressBar.Get()));
}

UPandoraWidgetViewModel* UPandoraWidget::GetOrCreatePandoraWidgetViewModel()
{
	if (!PandoraWidgetViewModel)
	{
		PandoraWidgetViewModel = NewObject<UPandoraWidgetViewModel>(this);
	}

	if (PandoraWidgetViewModel && !PandoraWidgetViewModel->IsViewModelInitialized())
	{
		PandoraWidgetViewModel->InitializeViewModel(this);
	}

	return PandoraWidgetViewModel.Get();
}

void UPandoraWidget::ApplyPandoraWidgetViewModelToMvvmView()
{
	if (!PandoraWidgetViewModel)
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
		if (SourceClass && PandoraWidgetViewModel->GetClass()->IsChildOf(SourceClass))
		{
			RuntimeViewModelName = Source.GetName();
			break;
		}
	}

	if (RuntimeViewModelName.IsNone())
	{
		UE_LOG(LogPandoraWidget, Warning,
			TEXT("[ApplyViewModel] skipped: widget has MVVM extension, but no settable PandoraWidgetViewModel source. widget=%s viewModel=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PandoraWidgetViewModel.Get()));
		return;
	}

	const bool bSuccess = ViewExtension->SetViewModel(RuntimeViewModelName, PandoraWidgetViewModel);
	if (!bSuccess)
	{
		UE_LOG(LogPandoraWidget, Warning,
			TEXT("[ApplyViewModel] failed. widget=%s viewModelName=%s viewModel=%s"),
			*GetNameSafe(this),
			*RuntimeViewModelName.ToString(),
			*GetNameSafe(PandoraWidgetViewModel.Get()));
	}
}

void UPandoraWidget::ApplyDesignerDefaults()
{
	ResolveControlWidgets();

	if (Button)
	{
		Button->SetBackgroundColor(PandoraButtonTransparentColor);
	}
}

void UPandoraWidget::ClearButtonPressTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ButtonHoldTimerHandle);
	}

	ButtonHoldTimerHandle.Invalidate();
}

void UPandoraWidget::SetupPandoraDescriptionPopupWidget()
{
	ResolvePandoraDescriptionPopupWidgetClass();
	if (!CreatedPandoraDescriptionPopup && PandoraDescriptionPopupWidgetClass)
	{
		if (APlayerController* OwningPlayer = GetOwningPlayer())
		{
			CreatedPandoraDescriptionPopup = CreateWidget<UUserWidget>(OwningPlayer, PandoraDescriptionPopupWidgetClass);
		}
		else if (UWorld* World = GetWorld())
		{
			CreatedPandoraDescriptionPopup = CreateWidget<UUserWidget>(World, PandoraDescriptionPopupWidgetClass);
		}
	}

	UE_LOG(LogPandoraWidget, Log,
		TEXT("[SetupDescription] widget=%s descClass=%s popup=%s treeWidget=%s popupPanel=%s pandora=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PandoraDescriptionPopupWidgetClass.Get()),
		*GetNameSafe(CreatedPandoraDescriptionPopup.Get()),
		*GetNameSafe(ResolvedPandoraTreeWidget.Get()),
		*GetNameSafe(GetPandoraDescriptionPopupPanel()),
		*GetNameSafe(PandoraDefinition.Get()));

	UpdatePandoraDescriptionDetails();
	StartFocusCheckTimer();
}

void UPandoraWidget::ResolvePandoraTreeWidget()
{
	if (ResolvedPandoraTreeWidget)
	{
		return;
	}

	if (UPandoraTreeWidget* PandoraTreeWidget = GetTypedOuter<UPandoraTreeWidget>())
	{
		ResolvedPandoraTreeWidget = PandoraTreeWidget;
		return;
	}

	TArray<UUserWidget*> PandoraTreeWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, PandoraTreeWidgets, UPandoraTreeWidget::StaticClass(), false);
	for (UUserWidget* Widget : PandoraTreeWidgets)
	{
		if (UPandoraTreeWidget* PandoraTreeWidget = Cast<UPandoraTreeWidget>(Widget))
		{
			ResolvedPandoraTreeWidget = PandoraTreeWidget;
			return;
		}
	}
}

void UPandoraWidget::ResolvePandoraDescriptionPopupWidgetClass()
{
	if (PandoraDescriptionPopupWidgetClass)
	{
		return;
	}

	PandoraDescriptionPopupWidgetClass = LoadClass<UUserWidget>(
		nullptr,
		TEXT("/Game/UI/Widget/WBP_PandoraDescription.WBP_PandoraDescription_C"));
}

void UPandoraWidget::StartFocusCheckTimer()
{
	ClearFocusCheckTimer();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			FocusCheckTimerHandle,
			this,
			&ThisClass::CheckFocusState,
			FocusCheckInterval,
			true);
	}
}

void UPandoraWidget::ClearFocusCheckTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FocusCheckTimerHandle);
	}

	FocusCheckTimerHandle.Invalidate();
}

void UPandoraWidget::UpdatePandoraDescriptionDetails()
{
	if (!CreatedPandoraDescriptionPopup)
	{
		UE_LOG(LogPandoraWidget, Warning,
			TEXT("[UpdateDescription] skipped: no popup. widget=%s pandora=%s descClass=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PandoraDefinition.Get()),
			*GetNameSafe(PandoraDescriptionPopupWidgetClass.Get()));
		return;
	}

	if (UPandoraDescriptionWidget* PandoraDescriptionWidget = Cast<UPandoraDescriptionWidget>(CreatedPandoraDescriptionPopup))
	{
		PandoraDescriptionWidget->SetPandoraDefinition(PandoraDefinition.Get());
		PandoraDescriptionWidget->SetPandoraTreeComponent(PandoraTreeComponent.Get());
		PandoraDescriptionWidget->SetDetails();
		return;
	}

	if (UFunction* SetDetailsFunction = CreatedPandoraDescriptionPopup->FindFunction(TEXT("SetDetails")))
	{
		CreatedPandoraDescriptionPopup->ProcessEvent(SetDetailsFunction, nullptr);
	}
}

UPanelWidget* UPandoraWidget::GetPandoraDescriptionPopupPanel() const
{
	return ResolvedPandoraTreeWidget ? ResolvedPandoraTreeWidget->GetPandoraDescriptionPopupPanel() : nullptr;
}

void UPandoraWidget::CheckFocusState()
{
	const bool bShouldShowPopup = Button && (Button->IsHovered() || Button->HasKeyboardFocus());
	if (bShouldShowPopup)
	{
		ShowPandoraDescriptionPopup();
	}
	else
	{
		RemovePandoraDescriptionPopup();
	}
}

void UPandoraWidget::ShowPandoraDescriptionPopup()
{
	if (!CreatedPandoraDescriptionPopup)
	{
		SetupPandoraDescriptionPopupWidget();
	}

	if (!CreatedPandoraDescriptionPopup)
	{
		UE_LOG(LogPandoraWidget, Warning,
			TEXT("[ShowDescription] failed: no popup. widget=%s pandora=%s descClass=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PandoraDefinition.Get()),
			*GetNameSafe(PandoraDescriptionPopupWidgetClass.Get()));
		return;
	}

	ResolvePandoraTreeWidget();
	UPanelWidget* PopupPanel = GetPandoraDescriptionPopupPanel();
	if (!PopupPanel)
	{
		UE_LOG(LogPandoraWidget, Warning,
			TEXT("[ShowDescription] failed: no popup panel. widget=%s treeWidget=%s pandora=%s"),
			*GetNameSafe(this),
			*GetNameSafe(ResolvedPandoraTreeWidget.Get()),
			*GetNameSafe(PandoraDefinition.Get()));
		return;
	}

	UpdatePandoraDescriptionDetails();

	if (CreatedPandoraDescriptionPopup->GetParent() != PopupPanel)
	{
		CreatedPandoraDescriptionPopup->RemoveFromParent();
		PopupPanel->AddChild(CreatedPandoraDescriptionPopup);
		UE_LOG(LogPandoraWidget, Log,
			TEXT("[ShowDescription] added popup. widget=%s popup=%s panel=%s pandora=%s"),
			*GetNameSafe(this),
			*GetNameSafe(CreatedPandoraDescriptionPopup.Get()),
			*GetNameSafe(PopupPanel),
			*GetNameSafe(PandoraDefinition.Get()));
	}

	if (UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(PopupPanel))
	{
		FVector2D PixelPosition;
		FVector2D ViewportPosition;
		USlateBlueprintLibrary::LocalToViewport(
			this,
			GetCachedGeometry(),
			DescriptionPopupLocalOffset,
			PixelPosition,
			ViewportPosition);

		CanvasSlot->SetPosition(ViewportPosition);
	}
}

void UPandoraWidget::RemovePandoraDescriptionPopup()
{
	if (CreatedPandoraDescriptionPopup)
	{
		CreatedPandoraDescriptionPopup->RemoveFromParent();
	}
}
