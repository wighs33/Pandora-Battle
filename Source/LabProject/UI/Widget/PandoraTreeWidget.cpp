#include "UI/Widget/PandoraTreeWidget.h"

#include "AbilitySystem/PandoraTree/PandoraTreeComponent.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraDefinition.h"
#include "UI/Widget/PandoraWidget.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"
#include "ViewModel/PandoraTreeViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraTreeWidget)

DEFINE_LOG_CATEGORY_STATIC(LogPandoraTreeWidget, Log, All);

namespace
{
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

	void CallNoParamFunction(UObject* Object, const FName FunctionName)
	{
		if (!IsValid(Object))
		{
			return;
		}

		if (UFunction* Function = Object->FindFunction(FunctionName))
		{
			Object->ProcessEvent(Function, nullptr);
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

	ResolveControlWidgets();
	GetOrCreatePandoraTreeViewModel();
	ApplyPandoraTreeViewModelToMvvmView();
	RefreshPandoraWidgets();
}

void UPandoraTreeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ResolvePandoraTreeComponent();
	ResolveControlWidgets();
	GetOrCreatePandoraTreeViewModel();
	ApplyPandoraTreeViewModelToMvvmView();
	ApplyPandoraDefinitionToComponent();
	BindPandoraTreeEvents();
	BindButtonEvents();
	SetPandoraPointsText();
	RefreshPandoraWidgets();

	UE_LOG(LogPandoraTreeWidget, Log,
		TEXT("[Construct] widget=%s treeComponent=%s pandora=%s viewModel=%s resetButton=%s popupPanel=%s owningPlayer=%s owningPawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PandoraTreeComponent.Get()),
		*GetNameSafe(PandoraDefinition.Get()),
		*GetNameSafe(PandoraTreeViewModel.Get()),
		*GetNameSafe(ResetPandoraButton.Get()),
		*GetNameSafe(GetPandoraDescriptionPopupPanel()),
		*GetNameSafe(GetOwningPlayer()),
		*GetNameSafe(GetOwningPlayerPawn()));
}

void UPandoraTreeWidget::NativeDestruct()
{
	ClearHideTimer();
	UnbindButtonEvents();
	UnbindPandoraTreeEvents();
	ReturnCameraToPawn(PreviewCameraHideBlendTime);
	DestroyCharacterPreview();
	if (PandoraTreeViewModel && PandoraTreeViewModel->IsViewModelInitialized())
	{
		PandoraTreeViewModel->UninitializeViewModel();
	}

	Super::NativeDestruct();
}

FReply UPandoraTreeWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!bCloseOnToggleKey || !InKeyEvent.GetKey().IsValid() || !IsTogglePandoraTreeKey(InKeyEvent.GetKey()))
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
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
		UE_LOG(LogPandoraTreeWidget, Warning,
			TEXT("[SetPointsText] skipped. widget=%s treeComponent=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PandoraTreeComponent.Get()));
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
	UE_LOG(LogPandoraTreeWidget, Log,
		TEXT("[Show] widget=%s treeComponent=%s pandora=%s points=%d"),
		*GetNameSafe(this),
		*GetNameSafe(PandoraTreeComponent.Get()),
		*GetNameSafe(PandoraDefinition.Get()),
		PandoraTreeComponent ? PandoraTreeComponent->GetPointsAvailable() : INDEX_NONE);

	if (SlideInLeft)
	{
		PlayAnimation(SlideInLeft, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
	}

	SpawnCharacterPreview();

	if (!bSetInputModeOnShowHide)
	{
		return;
	}

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = true;
	}
}

void UPandoraTreeWidget::HidePandoraTree()
{
	ClearHideTimer();
	ReturnCameraToPawn(PreviewCameraHideBlendTime);

	if (bSetInputModeOnShowHide)
	{
		if (APlayerController* PlayerController = GetOwningPlayer())
		{
			FInputModeGameOnly InputMode;
			PlayerController->SetInputMode(InputMode);
			PlayerController->bShowMouseCursor = false;
		}
	}

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

void UPandoraTreeWidget::ResetPandora()
{
	ResolvePandoraTreeComponent();
	if (PandoraTreeComponent)
	{
		PandoraTreeComponent->ResetPandora();
	}
}

UPanelWidget* UPandoraTreeWidget::GetPandoraDescriptionPopupPanel() const
{
	if (PandoraDescriptionPopup)
	{
		return PandoraDescriptionPopup.Get();
	}

	UPandoraTreeWidget* MutableThis = const_cast<UPandoraTreeWidget*>(this);
	if (UPanelWidget* Panel = Cast<UPanelWidget>(MutableThis->GetWidgetFromName(TEXT("PandoraDescriptionPopup"))))
	{
		return Panel;
	}

	if (UPanelWidget* Panel = Cast<UPanelWidget>(MutableThis->GetWidgetFromName(TEXT("PandoraDescriptionPopupPanel"))))
	{
		return Panel;
	}

	return nullptr;
}

void UPandoraTreeWidget::HandlePandoraStateChanged()
{
	RefreshPandoraWidgets();
}

void UPandoraTreeWidget::HandlePandoraPointsChanged(int32 NewPointsAvailable)
{
	(void)NewPointsAvailable;
	SetPandoraPointsText();
	RefreshPandoraWidgets();
}

void UPandoraTreeWidget::HandleResetPandoraClicked()
{
	ResetPandora();
}

void UPandoraTreeWidget::ResolvePandoraTreeComponent()
{
	if (!PandoraTreeComponent)
	{
		PandoraTreeComponent = ResolvePandoraTreeComponentFromWidget(this);
	}

	if (!PandoraDefinition && PandoraTreeComponent)
	{
		PandoraDefinition = PandoraTreeComponent->GetPandoraDefinition();
	}

	UE_LOG(LogPandoraTreeWidget, Log,
		TEXT("[ResolveTreeComponent] widget=%s treeComponent=%s pandora=%s owningPlayer=%s owningPawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PandoraTreeComponent.Get()),
		*GetNameSafe(PandoraDefinition.Get()),
		*GetNameSafe(GetOwningPlayer()),
		*GetNameSafe(GetOwningPlayerPawn()));
}

void UPandoraTreeWidget::ResolveControlWidgets()
{
	if (!ResetPandoraButton)
	{
		ResetPandoraButton = Cast<UButton>(GetWidgetFromName(TEXT("ResetPandoraButton")));
	}
	if (!ResetPandoraButton)
	{
		ResetPandoraButton = Cast<UButton>(GetWidgetFromName(TEXT("PandoraResetButton")));
	}

	if (!PandoraDescriptionPopup)
	{
		PandoraDescriptionPopup = Cast<UPanelWidget>(GetWidgetFromName(TEXT("PandoraDescriptionPopup")));
	}
	if (!PandoraDescriptionPopup)
	{
		PandoraDescriptionPopup = Cast<UPanelWidget>(GetWidgetFromName(TEXT("PandoraDescriptionPopupPanel")));
	}

	UE_LOG(LogPandoraTreeWidget, Log,
		TEXT("[ResolveControls] widget=%s resetButton=%s popupPanel=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ResetPandoraButton.Get()),
		*GetNameSafe(PandoraDescriptionPopup.Get()));
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
		UE_LOG(LogPandoraTreeWidget, Warning,
			TEXT("[ApplyViewModel] skipped: widget has MVVM extension, but no settable PandoraTreeViewModel source. widget=%s viewModel=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PandoraTreeViewModel.Get()));
		return;
	}

	const bool bSuccess = ViewExtension->SetViewModel(RuntimeViewModelName, PandoraTreeViewModel);
	if (!bSuccess)
	{
		UE_LOG(LogPandoraTreeWidget, Warning,
			TEXT("[ApplyViewModel] failed. widget=%s viewModelName=%s viewModel=%s"),
			*GetNameSafe(this),
			*RuntimeViewModelName.ToString(),
			*GetNameSafe(PandoraTreeViewModel.Get()));
	}
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

	if (!ResetPandoraButton)
	{
		return;
	}

	ResetPandoraButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleResetPandoraClicked);
	ResetPandoraButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleResetPandoraClicked);
}

void UPandoraTreeWidget::UnbindButtonEvents()
{
	if (!ResetPandoraButton)
	{
		return;
	}

	ResetPandoraButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleResetPandoraClicked);
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
		UE_LOG(LogPandoraTreeWidget, Log,
			TEXT("[ApplyPandoraDefinition] widget=%s treeComponent=%s pandora=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PandoraTreeComponent.Get()),
			*GetNameSafe(PandoraDefinition.Get()));
		PandoraTreeComponent->SetPandoraDefinition(PandoraDefinition.Get());
	}
	else
	{
		UE_LOG(LogPandoraTreeWidget, Log,
			TEXT("[ApplyPandoraDefinition] skipped. widget=%s treeComponent=%s pandora=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PandoraTreeComponent.Get()),
			*GetNameSafe(PandoraDefinition.Get()));
	}
}

void UPandoraTreeWidget::RefreshPandoraWidget(UWidget* Widget) const
{
	if (UPandoraWidget* PandoraWidget = Cast<UPandoraWidget>(Widget))
	{
		const UPandoraDefinition* BeforePandora = PandoraWidget->GetPandoraDefinition();
		PandoraWidget->SetPandoraTreeComponent(PandoraTreeComponent.Get());
		if (!PandoraWidget->GetPandoraDefinition() && PandoraDefinition)
		{
			PandoraWidget->SetPandoraDefinition(PandoraDefinition.Get());
		}

		PandoraWidget->SetPandoraInfo();
		UE_LOG(LogPandoraTreeWidget, Log,
			TEXT("[RefreshPandoraWidget] child=%s beforePandora=%s afterPandora=%s treeComponent=%s treePandora=%s"),
			*GetNameSafe(PandoraWidget),
			*GetNameSafe(BeforePandora),
			*GetNameSafe(PandoraWidget->GetPandoraDefinition()),
			*GetNameSafe(PandoraTreeComponent.Get()),
			*GetNameSafe(PandoraDefinition.Get()));
		return;
	}

	CallNoParamFunction(Widget, TEXT("SetPandoraInfo"));
}

void UPandoraTreeWidget::ResolveTogglePandoraTreeAction()
{
	if (TogglePandoraTreeAction)
	{
		return;
	}

	TogglePandoraTreeAction = LoadObject<UInputAction>(
		nullptr,
		TEXT("/Game/Input/Action/IA_PandoraTree.IA_PandoraTree"));
}

bool UPandoraTreeWidget::IsTogglePandoraTreeKey(const FKey& Key) const
{
	const_cast<UPandoraTreeWidget*>(this)->ResolveTogglePandoraTreeAction();
	if (!TogglePandoraTreeAction)
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
		: nullptr;
	if (!InputSubsystem)
	{
		return false;
	}

	const TArray<FKey> MappedKeys = InputSubsystem->QueryKeysMappedToAction(TogglePandoraTreeAction);
	return MappedKeys.Contains(Key);
}

void UPandoraTreeWidget::ResolveCharacterPreviewClass()
{
	if (CharacterPreviewClass)
	{
		return;
	}

	CharacterPreviewClass = LoadClass<AActor>(
		nullptr,
		TEXT("/Game/CharacterPreview/BP_CharacterPreview.BP_CharacterPreview_C"));
}

void UPandoraTreeWidget::SpawnCharacterPreview()
{
	if (!bUseCharacterPreviewCamera)
	{
		return;
	}

	if (IsValid(SpawnedCharacterPreview))
	{
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

void UPandoraTreeWidget::FinishHidePandoraTree()
{
	HideTimerHandle.Invalidate();
	DestroyCharacterPreview();
	RemoveFromParent();
}

void UPandoraTreeWidget::ClearHideTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}

	HideTimerHandle.Invalidate();
}
