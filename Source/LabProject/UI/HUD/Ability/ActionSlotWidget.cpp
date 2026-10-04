#include "UI/HUD/Ability/ActionSlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Mode/PdPlayerController.h"
#include "Definition/Player/ControllerInputDefinition.h"
#include "TimerManager.h"
#include "UI/HUD/Ability/ActionSlotEntryWidget.h"
#include "UI/Core/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActionSlotWidget)

namespace
{
	constexpr int32 ActionSlotCount = 2;
}

void UActionSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		RebuildActionSlotBar();
	}
}

void UActionSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BeginActionContentPreload();
}

void UActionSlotWidget::NativeDestruct()
{
	ActionPresentationLease.Reset();
	ActionDefinitionLease.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RebuildActionSlotTimerHandle);
	}
	Super::NativeDestruct();
}

void UActionSlotWidget::BeginActionContentPreload()
{
	ActionPresentationLease.Reset();
	ActionDefinitionLease.Reset();

	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	const TSoftObjectPtr<UCharacterActionDefinition> ActionDefinitionReference =
		ResolveActionDefinitionReference();

	if (!ContentSubsystem || ActionDefinitionReference.IsNull())
	{
		FillActionSlotBar();
		return;
	}

	if (ActionDefinitionReference.Get())
	{
		BeginActionPresentationPreload();
		return;
	}

	ActionDefinitionLease = ContentSubsystem->AcquireContent(
		{ActionDefinitionReference.ToSoftObjectPath()},
		FSimpleDelegate::CreateUObject(this, &ThisClass::BeginActionPresentationPreload));
}

void UActionSlotWidget::BeginActionPresentationPreload()
{
	TArray<FSoftObjectPath> PresentationPaths;
	if (const UCharacterActionDefinition* ActionDefinition =
		ResolveActionDefinition())
	{
		ActionDefinition->GetRuntimePreloadAssetPaths(PresentationPaths);
	}

	if (const APdPlayerController* PlayerController =
		Cast<APdPlayerController>(GetOwningPlayer()))
	{
		if (const UControllerInputDefinition* InputDefinition =
			PlayerController->GetLoadedInputDefinition())
		{
			for (const FInputActionIconMapping& Mapping :
				InputDefinition->GetInputActionIconMappings())
			{
				if (!Mapping.InputAction.IsNull())
				{
					PresentationPaths.Add(
						Mapping.InputAction.ToSoftObjectPath());
				}
				if (!Mapping.Icon.IsNull())
				{
					PresentationPaths.Add(Mapping.Icon.ToSoftObjectPath());
				}
			}
		}
	}

	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem || PresentationPaths.IsEmpty())
	{
		FillActionSlotBar();
		return;
	}

	ActionPresentationLease = ContentSubsystem->AcquireContent(
		PresentationPaths,
		FSimpleDelegate::CreateUObject(this, &ThisClass::FillActionSlotBar));
}

void UActionSlotWidget::FillActionSlotBar()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RebuildActionSlotTimerHandle);
		RebuildActionSlotTimerHandle =
			World->GetTimerManager().SetTimerForNextTick(
				this,
				&ThisClass::RebuildActionSlotBar);
		return;
	}

	RebuildActionSlotBar();
}

void UActionSlotWidget::RebuildActionSlotBar()
{
	RebuildActionSlotTimerHandle.Invalidate();

	if (!ContainerHorizontalBox)
	{
		return;
	}

	ContainerHorizontalBox->ClearChildren();
	EntryWidgets.Reset();

	for (int32 SlotIndex = 0; SlotIndex < ActionSlotCount; ++SlotIndex)
	{
		AddActionSlotEntry(SlotIndex);
	}
}

void UActionSlotWidget::AddActionSlotEntry(const int32 SlotIndex)
{
	UActionSlotEntryWidget* EntryWidget = CreateActionSlotEntryWidget();
	if (!EntryWidget)
	{
		return;
	}

	UCharacterActionDefinition* ActionDefinition = ResolveActionDefinition();
	EntryWidget->SetActionSlotData(SlotIndex, ResolveActionType(SlotIndex), ActionDefinition);

	EntryWidgets.Add(EntryWidget);
	AddWidgetToBar(EntryWidget);
}

UActionSlotEntryWidget* UActionSlotWidget::CreateActionSlotEntryWidget() const
{
	TSubclassOf<UActionSlotEntryWidget> ResolvedEntryWidgetClass = ResolveEntryWidgetClass();
	if (!ResolvedEntryWidgetClass)
	{
		return nullptr;
	}

	if (IsDesignTime() && WidgetTree)
	{
		return WidgetTree->ConstructWidget<UActionSlotEntryWidget>(ResolvedEntryWidgetClass);
	}

	if (APlayerController* OwningPlayer = GetOwningPlayer())
	{
		return CreateWidget<UActionSlotEntryWidget>(OwningPlayer, ResolvedEntryWidgetClass);
	}

	return CreateWidget<UActionSlotEntryWidget>(const_cast<UActionSlotWidget*>(this), ResolvedEntryWidgetClass);
}

void UActionSlotWidget::AddWidgetToBar(UWidget* Widget) const
{
	if (!ContainerHorizontalBox || !Widget)
	{
		return;
	}

	UHorizontalBoxSlot* HorizontalBoxSlot = ContainerHorizontalBox->AddChildToHorizontalBox(Widget);
	if (HorizontalBoxSlot)
	{
		HorizontalBoxSlot->SetPadding(ResolveSlotPadding());
	}
}

TSubclassOf<UActionSlotEntryWidget> UActionSlotWidget::ResolveEntryWidgetClass() const
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const TSoftClassPtr<UActionSlotEntryWidget> EntryWidgetClassDefinition =
			WidgetDefinition->GetActionSlotEntryWidgetClass();
		if (!EntryWidgetClassDefinition.IsNull())
		{
			if (UClass* LoadedClass = EntryWidgetClassDefinition.Get())
			{
				return LoadedClass;
			}
		}
	}

	return EntryWidgetClass;
}

UCharacterActionDefinition* UActionSlotWidget::ResolveActionDefinition() const
{
	return ResolveActionDefinitionReference().Get();
}

TSoftObjectPtr<UCharacterActionDefinition>
UActionSlotWidget::ResolveActionDefinitionReference() const
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const FActionSlotWidgetSettings& Settings = WidgetDefinition->GetActionSlotWidgetSettings();
		if (!Settings.CharacterActionDefinition.IsNull())
		{
			return Settings.CharacterActionDefinition;
		}
	}

	if (!CharacterActionDefinition.IsNull())
	{
		return CharacterActionDefinition;
	}

	const APdPlayerController* PlayerController = Cast<APdPlayerController>(GetOwningPlayer());
	const UControllerInputDefinition* InputDefinition = PlayerController ? PlayerController->GetLoadedInputDefinition() : nullptr;
	if (!InputDefinition)
	{
		return {};
	}

	return InputDefinition->GetEffectiveCharacterActionDefinition();
}

ECharacterActionType UActionSlotWidget::ResolveActionType(const int32 SlotIndex) const
{
	return SlotIndex == 0
		? ECharacterActionType::PandoraWeaponSwap
		: ECharacterActionType::GrappleHook;
}

FMargin UActionSlotWidget::ResolveSlotPadding() const
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		return WidgetDefinition->GetActionSlotWidgetSettings().SlotPadding;
	}

	return SlotPadding;
}
