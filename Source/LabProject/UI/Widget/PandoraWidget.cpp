#include "UI/Widget/PandoraWidget.h"

#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Mode/PdGameInstance.h"
#include "Mode/PdPlayerState.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "UI/WidgetLookup.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "UI/Widget/PandoraWidgetViewData.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"
#include "ViewModel/PandoraWidgetViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraWidget)

namespace
{
	const FLinearColor PandoraButtonTransparentColor(1.0f, 1.0f, 1.0f, 0.0f);

	UPandoraTreeComponent* ResolvePandoraTreeComponentFromPandoraWidget(const UUserWidget* Widget)
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

	UPandoraComponent* ResolvePandoraComponentFromPandoraWidget(const UUserWidget* Widget)
	{
		if (!Widget)
		{
			return nullptr;
		}

		if (APlayerController* PlayerController = Widget->GetOwningPlayer())
		{
			if (APdPlayerState* PlayerState = PlayerController->GetPlayerState<APdPlayerState>())
			{
				return PlayerState->GetPandoraComponent();
			}
		}

		if (APawn* OwningPawn = Widget->GetOwningPlayerPawn())
		{
			if (APdPlayerState* PlayerState = OwningPawn->GetPlayerState<APdPlayerState>())
			{
				return PlayerState->GetPandoraComponent();
			}
		}

		return nullptr;
	}

}

void UPandoraWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplyWidgetDefinitionSettings();
	ResolveControlWidgets();
	ResolveEquipHintWidgets();
	GetOrCreatePandoraWidgetViewModel();
	ApplyPandoraWidgetViewModelToMvvmView();
	ApplyDesignerDefaults();
	ApplyEquipHintDefaults();
	SetPandoraInfo();
}

void UPandoraWidget::NativeConstruct()
{
	Super::NativeConstruct();

	bIsButtonHoverActive = false;
	bIsFocusWithinWidget = false;
	bIsPandoraDescriptionRequested = false;
	ApplyWidgetDefinitionSettings();
	ResolveControlWidgets();
	ResolveEquipHintWidgets();
	GetOrCreatePandoraWidgetViewModel();
	ApplyPandoraWidgetViewModelToMvvmView();
	ResolvePandoraTreeComponent();
	ResolvePandoraComponent();
	BindPandoraTreeEvents();
	BindPandoraComponentEvents();
	BindButtonEvents();
	ApplyDesignerDefaults();
	ApplyEquipHintDefaults();
	RefreshEquipHintState(false);
	SetPandoraInfo();
}

void UPandoraWidget::NativeDestruct()
{
	bIsButtonHoverActive = false;
	bIsFocusWithinWidget = false;
	if (bIsPandoraDescriptionRequested)
	{
		bIsPandoraDescriptionRequested = false;
		OnPandoraDescriptionDismissed.Broadcast(this);
	}

	SetEquipHintWidgetsVisible(false, false);
	ClearButtonPressTimer();
	UnbindButtonEvents();
	UnbindPandoraTreeEvents();
	UnbindPandoraComponentEvents();
	if (PandoraWidgetViewModel && PandoraWidgetViewModel->IsViewModelInitialized())
	{
		PandoraWidgetViewModel->UninitializeViewModel();
	}

	Super::NativeDestruct();
}

void UPandoraWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Pandora Tree pauses standalone training gameplay. UMG continues ticking
	// while paused, whereas UWorld timers do not, so hold progress belongs here.
	if (bIsButtonHoldActive)
	{
		IncrementButtonTimer();
	}
}

void UPandoraWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);

	bIsFocusWithinWidget = true;
	RefreshPandoraDescriptionRequest();
}

void UPandoraWidget::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);

	bIsFocusWithinWidget = false;
	RefreshPandoraDescriptionRequest();
}

void UPandoraWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	RefreshEquipHintState(true);
}

void UPandoraWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	RefreshEquipHintState(false);
}

FReply UPandoraWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const bool bHandled = HandlePandoraWidgetMouseButtonDown(InMouseEvent);

	if (bHandled)
	{
		return FReply::Handled();
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UPandoraWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const bool bHandled = HandlePandoraWidgetMouseButtonDown(InMouseEvent);

	if (bHandled)
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UPandoraWidget::SetPandoraDefinition(UPandoraDefinition* InPandoraDefinition)
{
	if (PandoraDefinition.Get() == InPandoraDefinition)
	{
		return;
	}

	PandoraDefinition = InPandoraDefinition;

	SetPandoraInfo();
	RefreshEquipHintState(IsHovered());
	RefreshPandoraDescriptionRequest(true);
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

	BindPandoraTreeEvents();
	SetPandoraInfo();
	RefreshEquipHintState(IsHovered());
	RefreshPandoraDescriptionRequest(true);
}

const UWidget* UPandoraWidget::GetPandoraDescriptionAnchorWidget() const
{
	return Button ? static_cast<const UWidget*>(Button.Get()) : static_cast<const UWidget*>(this);
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

	FPandoraWidgetViewData ViewData = FPandoraWidgetViewDataBuilder::Build(
		PandoraDefinition.Get(),
		PandoraTreeComponent.Get(),
		Style);
	const bool bUnlockedInSave = IsPandoraUnlockedInSave();
	if (!bUnlockedInSave && PandoraDefinition)
	{
		ViewData.bCanSpend = false;
		ViewData.bActive = false;
		ViewData.bLocked = true;
		ViewData.bDimmedStyle = true;
		ViewData.OverlayColor = LockedOverlayColor;
		ViewData.ContentOpacity = UnavailableContentOpacity;
		ViewData.StateIconVisibility = ESlateVisibility::HitTestInvisible;
	}
	else if (bUnlockedInSave && PandoraDefinition)
	{
		ViewData.StateIconVisibility = ESlateVisibility::Collapsed;
	}

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

RefreshEquipHintState(IsHovered());
}

void UPandoraWidget::ConfirmSpendPointOnPandora()
{
	if (!IsPandoraUnlockedInSave() || !PandoraTreeComponent || !PandoraDefinition)
	{
		return;
	}

	if (PandoraTreeComponent->CanSpendPointOnPandora(PandoraDefinition.Get()))
	{
		PandoraTreeComponent->SpendPointOnPandora(PandoraDefinition.Get());
	}
}

void UPandoraWidget::IncrementButtonTimer()
{
	if (!bIsButtonHoldActive)
	{
		return;
	}

	if (ButtonHoldDuration <= 0.0)
	{
		ConfirmSpendPointOnPandora();
		ResetButtonPress();
		return;
	}

	ButtonHoldElapsedTime = FMath::Max(0.0, FPlatformTime::Seconds() - ButtonHoldStartRealTime);

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
	bIsButtonHoldActive = false;
	ButtonHoldStartRealTime = 0.0;
	ButtonHoldElapsedTime = 0.0;
	ClearButtonPressTimer();

	if (ButtonProgressBar)
	{
		ButtonProgressBar->SetPercent(0.0f);
	}

	OnPandoraTreeFocusRequested.Broadcast(this);
}

void UPandoraWidget::HandleButtonPressed()
{
	if (!IsPandoraUnlockedInSave())
	{
		return;
	}

	const bool bCanSpend = PandoraTreeComponent
		&& PandoraDefinition
		&& PandoraTreeComponent->CanSpendPointOnPandora(PandoraDefinition.Get());
	if (!bCanSpend)
	{
		return;
	}

	if (ButtonHoldDuration <= 0.0)
	{
		ConfirmSpendPointOnPandora();
		ResetButtonPress();
		return;
	}

	ClearButtonPressTimer();
	bIsButtonHoldActive = true;
	ButtonHoldStartRealTime = FPlatformTime::Seconds();
	ButtonHoldElapsedTime = 0.0;
	if (ButtonProgressBar)
	{
		ButtonProgressBar->SetPercent(0.0f);
	}
}

void UPandoraWidget::HandleButtonReleased()
{
	ResetButtonPress();
}

void UPandoraWidget::HandleButtonHovered()
{
	if (bIsButtonHoverActive)
	{
		return;
	}

	const APlayerController* OwningPlayer = GetOwningPlayer();
	if (!OwningPlayer || !OwningPlayer->IsLocalController())
	{
		return;
	}

	bIsButtonHoverActive = true;
	RefreshEquipHintState(true);
	RefreshPandoraDescriptionRequest();
}

void UPandoraWidget::HandleButtonUnhovered()
{
	if (!bIsButtonHoverActive)
	{
		return;
	}

	bIsButtonHoverActive = false;
	RefreshEquipHintState(false);
	RefreshPandoraDescriptionRequest();
}

void UPandoraWidget::HandlePandoraStateChanged()
{
	SetPandoraInfo();
}

void UPandoraWidget::HandlePandoraPointsChanged(int32 NewPointsAvailable)
{
	(void)NewPointsAvailable;
	SetPandoraInfo();
}

void UPandoraWidget::HandlePandoraLoadoutChanged()
{
	RefreshEquipHintState(IsHovered());
}

void UPandoraWidget::ResolvePandoraTreeComponent()
{
	if (!PandoraTreeComponent)
	{
		PandoraTreeComponent = ResolvePandoraTreeComponentFromPandoraWidget(this);
	}

	if (!PandoraDefinition && PandoraTreeComponent)
	{
		PandoraDefinition = PandoraTreeComponent->GetPandoraDefinition();
	}

}

void UPandoraWidget::ResolvePandoraComponent()
{
	if (!PandoraComponent)
	{
		PandoraComponent = ResolvePandoraComponentFromPandoraWidget(this);
	}

}

void UPandoraWidget::ApplyWidgetDefinitionSettings()
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const FPandoraWidgetSettings& Settings = WidgetDefinition->GetPandoraWidgetSettings();
		bShowMaxText = Settings.bShowMaxText;
		ButtonHoldDuration = Settings.ButtonHoldDuration;
		ButtonHoldUpdateInterval = Settings.ButtonHoldUpdateInterval;
	}
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

void UPandoraWidget::BindPandoraComponentEvents()
{
	ResolvePandoraComponent();
	if (!PandoraComponent)
	{
		return;
	}

	PandoraComponent->OnPandoraLoadoutChanged.RemoveDynamic(this, &ThisClass::HandlePandoraLoadoutChanged);
	PandoraComponent->OnPandoraLoadoutChanged.AddUniqueDynamic(this, &ThisClass::HandlePandoraLoadoutChanged);
}

void UPandoraWidget::UnbindPandoraComponentEvents()
{
	if (!PandoraComponent)
	{
		return;
	}

	PandoraComponent->OnPandoraLoadoutChanged.RemoveDynamic(this, &ThisClass::HandlePandoraLoadoutChanged);
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
	Button->OnHovered.RemoveDynamic(this, &ThisClass::HandleButtonHovered);
	Button->OnUnhovered.RemoveDynamic(this, &ThisClass::HandleButtonUnhovered);
	Button->OnPressed.AddUniqueDynamic(this, &ThisClass::HandleButtonPressed);
	Button->OnReleased.AddUniqueDynamic(this, &ThisClass::HandleButtonReleased);
	Button->OnHovered.AddUniqueDynamic(this, &ThisClass::HandleButtonHovered);
	Button->OnUnhovered.AddUniqueDynamic(this, &ThisClass::HandleButtonUnhovered);
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
	Button->OnHovered.RemoveDynamic(this, &ThisClass::HandleButtonHovered);
	Button->OnUnhovered.RemoveDynamic(this, &ThisClass::HandleButtonUnhovered);
}

void UPandoraWidget::ResolveControlWidgets()
{
	if (!Button)
	{
		Button = PdWidgetLookup::FindWidgetByNames<UButton>(this, {
			TEXT("PandoraButton")
		});
	}
	if (!Button && WidgetTree)
	{
		Button = PdWidgetLookup::FindFirstWidgetOfType<UButton>(WidgetTree);
	}

	if (!ButtonProgressBar)
	{
		ButtonProgressBar = PdWidgetLookup::FindWidgetByNames<UProgressBar>(this, {
			TEXT("PandoraButtonProgressBar"),
			TEXT("PandoraProgressBar")
		});
	}
	if (!ButtonProgressBar && WidgetTree)
	{
		ButtonProgressBar = PdWidgetLookup::FindFirstWidgetOfType<UProgressBar>(WidgetTree);
	}

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

		return;
	}

	ViewExtension->SetViewModel(RuntimeViewModelName, PandoraWidgetViewModel);
}

void UPandoraWidget::ApplyDesignerDefaults()
{
	ResolveControlWidgets();
	ResolveEquipHintWidgets();

	if (Button)
	{
		Button->SetBackgroundColor(PandoraButtonTransparentColor);
	}
}

void UPandoraWidget::ResolveEquipHintWidgets()
{
	if (!InputKeyOverlay)
	{
		InputKeyOverlay = PdWidgetLookup::FindWidgetByNames<UWidget>(this, {
			TEXT("InputKeyOverlay")
		});
	}

	if (!InputKeyBackground)
	{
		InputKeyBackground = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("InputKeyBackground")
		});
	}

	if (!KeyIcon)
	{
		KeyIcon = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("KeyIcon")
		});
	}

	if (!Txt_Equip)
	{
		Txt_Equip = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("Txt_Equip")
		});
	}
}

void UPandoraWidget::ApplyEquipHintDefaults()
{
	ResolveEquipHintWidgets();

	if (Txt_Equip && !bHasCachedDefaultEquipText)
	{
		DefaultEquipText = Txt_Equip->GetText();
		bHasCachedDefaultEquipText = true;
	}

	if (IsDesignTime())
	{
		SetEquipHintWidgetsVisible(true, true);
	}
	else
	{
		RefreshEquipHintState(false);
	}
}

void UPandoraWidget::SetEquipHintWidgetsVisible(const bool bShowText, const bool bShowInputKey)
{
	ResolveEquipHintWidgets();

	const ESlateVisibility TextVisibility = bShowText
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed;
	const ESlateVisibility InputKeyVisibility = bShowInputKey
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed;

	if (InputKeyOverlay)
	{
		InputKeyOverlay->SetVisibility(InputKeyVisibility);
	}

	if (InputKeyBackground)
	{
		InputKeyBackground->SetVisibility(InputKeyVisibility);
	}

	if (KeyIcon)
	{
		KeyIcon->SetVisibility(InputKeyVisibility);
	}

	if (Txt_Equip)
	{
		Txt_Equip->SetVisibility(TextVisibility);
	}
}

void UPandoraWidget::RefreshEquipHintState(const bool bHovered)
{
	ResolveEquipHintWidgets();
	ResolvePandoraComponent();

	if (Txt_Equip && !bHasCachedDefaultEquipText)
	{
		DefaultEquipText = Txt_Equip->GetText();
		bHasCachedDefaultEquipText = true;
	}

	if (IsDesignTime())
	{
		SetEquipHintWidgetsVisible(true, true);
		return;
	}

	const bool bEquipped = IsPandoraEquipped();
	if (bEquipped)
	{
		if (Txt_Equip)
		{
			Txt_Equip->SetText(bHovered
				? NSLOCTEXT("PandoraWidget", "UnequipText", "Unequip")
				: NSLOCTEXT("PandoraWidget", "EquippedText", "Equipped"));
		}
		SetEquipHintWidgetsVisible(true, bHovered);
		return;
	}

	if (Txt_Equip && bHasCachedDefaultEquipText)
	{
		Txt_Equip->SetText(DefaultEquipText);
	}

	SetEquipHintWidgetsVisible(bHovered && CanShowEquipHint(), bHovered && CanShowEquipHint());
}

bool UPandoraWidget::CanShowEquipHint() const
{
	if (!PandoraDefinition)
	{
		return false;
	}

	if (!IsPandoraUnlockedInSave())
	{
		return false;
	}

	if (!PandoraTreeComponent)
	{
		return false;
	}

	return PandoraTreeComponent->GetCurrentPandoraLevel(PandoraDefinition.Get()) >= 1;
}

bool UPandoraWidget::CanAutoEquipPandora() const
{
	return CanShowEquipHint();
}

bool UPandoraWidget::IsPandoraEquipped() const
{
	EEnum_Direction EquippedDirection = EEnum_Direction::Center;
	return TryGetEquippedPandoraDirection(EquippedDirection);
}

bool UPandoraWidget::TryGetEquippedPandoraDirection(EEnum_Direction& OutDirection) const
{
	if (!PandoraComponent || !PandoraDefinition)
	{
		OutDirection = EEnum_Direction::Center;
		return false;
	}

	for (const EEnum_Direction Direction : { EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right })
	{
		if (PandoraComponent->GetPandoraLoadoutDefinition(Direction) == PandoraDefinition.Get())
		{
			OutDirection = Direction;
			return true;
		}
	}

	OutDirection = EEnum_Direction::Center;
	return false;
}

bool UPandoraWidget::HandlePandoraWidgetMouseButtonDown(const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::RightMouseButton)
	{
		return false;
	}

	ResolvePandoraComponent();
	if (IsPandoraEquipped())
	{
		RequestUnequipPandora();
	}
	else
	{
		RequestAutoEquipPandora();
	}
	return true;
}

bool UPandoraWidget::RequestAutoEquipPandora()
{
	if (!PandoraDefinition)
	{

		return false;
	}

	if (!CanAutoEquipPandora())
	{
		const int32 CurrentLevel = PandoraTreeComponent && PandoraDefinition
			? PandoraTreeComponent->GetCurrentPandoraLevel(PandoraDefinition.Get())
			: INDEX_NONE;

		return false;
	}

	APdPlayerState* PlayerState = nullptr;
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerState = PlayerController->GetPlayerState<APdPlayerState>();
	}

	if (!PlayerState)
	{
		if (APawn* OwningPawn = GetOwningPlayerPawn())
		{
			PlayerState = OwningPawn->GetPlayerState<APdPlayerState>();
		}
	}

	UPandoraComponent* ResolvedPandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	if (!ResolvedPandoraComponent)
	{

		return false;
	}

	return ResolvedPandoraComponent->RequestAutoSetPandoraLoadoutSlot(PandoraDefinition.Get());
}

bool UPandoraWidget::RequestUnequipPandora()
{
	if (!PandoraDefinition)
	{

		return false;
	}

	ResolvePandoraComponent();
	if (!PandoraComponent)
	{

		return false;
	}

	EEnum_Direction EquippedDirection = EEnum_Direction::Center;
	if (!TryGetEquippedPandoraDirection(EquippedDirection))
	{

		return false;
	}

	return PandoraComponent->RequestSetPandoraLoadoutSlot(EquippedDirection, nullptr);
}

void UPandoraWidget::RefreshPandoraDescriptionRequest(const bool bForceRefresh)
{
	const APlayerController* OwningPlayer = GetOwningPlayer();
	const bool bShouldRequestDescription =
		(bIsButtonHoverActive || bIsFocusWithinWidget)
		&& IsValid(PandoraDefinition)
		&& OwningPlayer
		&& OwningPlayer->IsLocalController();

	if (bShouldRequestDescription == bIsPandoraDescriptionRequested)
	{
		if (bForceRefresh && bShouldRequestDescription)
		{
			OnPandoraDescriptionRequested.Broadcast(this);
		}
		return;
	}

	bIsPandoraDescriptionRequested = bShouldRequestDescription;
	if (bIsPandoraDescriptionRequested)
	{
		OnPandoraDescriptionRequested.Broadcast(this);
	}
	else
	{
		OnPandoraDescriptionDismissed.Broadcast(this);
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

bool UPandoraWidget::IsPandoraUnlockedInSave() const
{
	if (!PandoraDefinition)
	{
		return false;
	}

	const bool bUnlockedInTree = PandoraTreeComponent
		&& PandoraTreeComponent->IsPandoraUnlockedForTree(PandoraDefinition.Get());
	if (bUnlockedInTree)
	{

		return true;
	}

	UPdGameInstance* PdGameInstance = GetGameInstance<UPdGameInstance>();
	if (!PdGameInstance)
	{
		return false;
	}

	const APlayerController* PlayerController = GetOwningPlayer();
	const APlayerState* PlayerState = PlayerController ? PlayerController->PlayerState : nullptr;
	FString PlayerId = PdGameInstance->ResolveSavePlayerId(PlayerController, PlayerState);
	if (PlayerId.IsEmpty())
	{
		PlayerId = PdGameInstance->GetPreferredSavePlayerId();
	}

	// Default-unlocked definitions are resolved before IsPandoraGranted requires a player id.
	return PdGameInstance->IsPandoraGranted(PlayerId, PandoraDefinition.Get());
}
