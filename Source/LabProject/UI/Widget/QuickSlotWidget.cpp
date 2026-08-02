#include "UI/Widget/QuickSlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Definition/Item/ItemDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Component/Item/InventoryComponent.h"
#include "Item/ItemInstance.h"
#include "Mode/PdPlayerState.h"
#include "Component/Skin/SkinEquipmentComponent.h"
#include "TimerManager.h"
#include "UI/Widget/QuickSlotEntryWidget.h"
#include "Definition/UI/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(QuickSlotWidget)

namespace
{
	constexpr int32 RequiredQuickSlotCount = 8;
	constexpr int32 ConsumableQuickSlotCount = 4;
	constexpr int32 QuickSlotGridColumns = 4;

	FGameplayTag ResolveGestureSlotTagFromQuickSlotIndex(const int32 QuickSlotIndex)
	{
		switch (QuickSlotIndex - ConsumableQuickSlotCount)
		{
		case 0:
			return LabGameplayTags::Skin_Gesture_Slot1;
		case 1:
			return LabGameplayTags::Skin_Gesture_Slot2;
		case 2:
			return LabGameplayTags::Skin_Gesture_Slot3;
		case 3:
			return LabGameplayTags::Skin_Gesture_Slot4;
		default:
			return FGameplayTag();
		}
	}
}

void UQuickSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		RebuildQuickSlotBar();
	}
}

void UQuickSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	InitializeInventoryBinding();
}

void UQuickSlotWidget::NativeDestruct()
{
	ReleaseQuickSlotIconPreload();
	UnbindInventoryChangedEvent();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetryInitializeTimerHandle);
		World->GetTimerManager().ClearTimer(RebuildBarTimerHandle);
	}

	Super::NativeDestruct();
}

void UQuickSlotWidget::FillQuickSlotBar()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RebuildBarTimerHandle);
		RebuildBarTimerHandle = World->GetTimerManager().SetTimerForNextTick(
			this,
			&ThisClass::RebuildQuickSlotBar);
		return;
	}

	RebuildQuickSlotBar();
}

void UQuickSlotWidget::InitializeInventoryBinding()
{
	UnbindInventoryChangedEvent();

	UInventoryComponent* InventoryComponent = ResolveOwningInventoryComponent();
	USkinEquipmentComponent* SkinEquipmentComponent = ResolveOwningSkinEquipmentComponent();
	if (!InventoryComponent || !SkinEquipmentComponent)
	{
		RebuildQuickSlotBar();
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				RetryInitializeTimerHandle,
				this,
				&ThisClass::InitializeInventoryBinding,
				0.1f,
				false);
		}
	}

	BoundInventoryComponent = InventoryComponent;
	BoundSkinEquipmentComponent = SkinEquipmentComponent;
	BindInventoryChangedEvent();
	RefreshQuickSlotIconPreload();
}

void UQuickSlotWidget::BindInventoryChangedEvent()
{
	if (UInventoryComponent* InventoryComponent = BoundInventoryComponent.Get())
	{
		InventoryComponent->OnInventoryChanged.AddUObject(this, &ThisClass::HandleInventoryChanged);
	}

	if (USkinEquipmentComponent* SkinEquipmentComponent = BoundSkinEquipmentComponent.Get())
	{
		SkinEquipmentComponent->OnEquippedSkinsChanged.AddUniqueDynamic(this, &ThisClass::HandleSkinEquipmentChanged);
	}
}

void UQuickSlotWidget::UnbindInventoryChangedEvent()
{
	if (UInventoryComponent* InventoryComponent = BoundInventoryComponent.Get())
	{
		InventoryComponent->OnInventoryChanged.RemoveAll(this);
	}

	if (USkinEquipmentComponent* SkinEquipmentComponent = BoundSkinEquipmentComponent.Get())
	{
		SkinEquipmentComponent->OnEquippedSkinsChanged.RemoveAll(this);
	}

	BoundInventoryComponent.Reset();
	BoundSkinEquipmentComponent.Reset();
}

void UQuickSlotWidget::HandleInventoryChanged()
{
	RefreshQuickSlotIconPreload();
}

void UQuickSlotWidget::RefreshQuickSlotIconPreload()
{
	TArray<FSoftObjectPath> IconPaths;
	UInventoryComponent* InventoryComponent = BoundInventoryComponent.Get();
	if (!InventoryComponent)
	{
		InventoryComponent = ResolveOwningInventoryComponent();
	}

	if (InventoryComponent)
	{
		for (int32 SlotIndex = 0;
			SlotIndex < ConsumableQuickSlotCount;
			++SlotIndex)
		{
			const UItemInstance* ItemInstance =
				InventoryComponent->GetConsumableQuickSlotItem(SlotIndex);
			const UItemDefinition* ItemDefinition =
				IsValid(ItemInstance)
					? ItemInstance->ItemDefinition.Get()
					: nullptr;
			if (!IsValid(ItemDefinition))
			{
				continue;
			}

			const FSoftObjectPath IconPath =
				ItemDefinition->IconTexture.ToSoftObjectPath();
			if (IconPath.IsValid() && !IconPath.IsNull())
			{
				IconPaths.AddUnique(IconPath);
			}
		}
	}

	IconPaths.Sort(
		[](const FSoftObjectPath& Left, const FSoftObjectPath& Right)
		{
			return Left.ToString() < Right.ToString();
		});

	if (PreloadedQuickSlotIconPaths == IconPaths
		&& (IconPaths.IsEmpty() || QuickSlotIconPreloadHandle.IsValid()))
	{
		FillQuickSlotBar();
		return;
	}

	ReleaseQuickSlotIconPreload();
	PreloadedQuickSlotIconPaths = IconPaths;
	FillQuickSlotBar();
	if (IconPaths.IsEmpty())
	{
		return;
	}

	const uint32 RequestGeneration = QuickSlotIconPreloadGeneration;
	TSharedPtr<FStreamableHandle> NewHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			IconPaths,
			FStreamableDelegate::CreateWeakLambda(
				this,
				[this, RequestGeneration]()
				{
					if (RequestGeneration != QuickSlotIconPreloadGeneration)
					{
						return;
					}

					FillQuickSlotBar();
				}));

	if (RequestGeneration != QuickSlotIconPreloadGeneration)
	{
		if (NewHandle.IsValid())
		{
			NewHandle->ReleaseHandle();
		}
		return;
	}

	QuickSlotIconPreloadHandle = MoveTemp(NewHandle);
}

void UQuickSlotWidget::ReleaseQuickSlotIconPreload()
{
	++QuickSlotIconPreloadGeneration;
	if (QuickSlotIconPreloadHandle.IsValid())
	{
		if (!QuickSlotIconPreloadHandle->HasLoadCompleted())
		{
			QuickSlotIconPreloadHandle->CancelHandle();
		}
		QuickSlotIconPreloadHandle->ReleaseHandle();
	}

	QuickSlotIconPreloadHandle.Reset();
	PreloadedQuickSlotIconPaths.Reset();
}

void UQuickSlotWidget::HandleSkinEquipmentChanged()
{
	FillQuickSlotBar();
}

void UQuickSlotWidget::RebuildQuickSlotBar()
{
	RebuildBarTimerHandle.Invalidate();

	if (!UniformGridPanel)
	{
		return;
	}

	UniformGridPanel->ClearChildren();
	UniformGridPanel->SetSlotPadding(ResolveSlotPadding());
	EntryWidgets.Reset();

	UInventoryComponent* InventoryComponent = nullptr;
	if (!IsDesignTime())
	{
		InventoryComponent = BoundInventoryComponent.Get();
		if (!InventoryComponent)
		{
			InventoryComponent = ResolveOwningInventoryComponent();
		}
	}

	USkinEquipmentComponent* SkinEquipmentComponent = nullptr;
	if (!IsDesignTime())
	{
		SkinEquipmentComponent = BoundSkinEquipmentComponent.Get();
		if (!SkinEquipmentComponent)
		{
			SkinEquipmentComponent = ResolveOwningSkinEquipmentComponent();
		}
	}

	const int32 NumSlots = IsDesignTime() ? RequiredQuickSlotCount : ResolveSlotCount();
	for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
	{
		AddQuickSlotEntry(SlotIndex, InventoryComponent, SkinEquipmentComponent);
	}
}

void UQuickSlotWidget::AddQuickSlotEntry(
	const int32 SlotIndex,
	UInventoryComponent* InventoryComponent,
	USkinEquipmentComponent* SkinEquipmentComponent)
{
	UQuickSlotEntryWidget* EntryWidget = CreateQuickSlotEntryWidget();
	if (!EntryWidget)
	{
		return;
	}

	if (SlotIndex < ConsumableQuickSlotCount)
	{
		EntryWidget->SetQuickSlotData(
			SlotIndex,
			InventoryComponent ? InventoryComponent->GetConsumableQuickSlotItem(SlotIndex) : nullptr);
	}
	else
	{
		EntryWidget->SetGestureSlotData(
			SlotIndex,
			ResolveGestureSlotSkinDefinition(SkinEquipmentComponent, SlotIndex));
	}

	EntryWidgets.Add(EntryWidget);
	AddWidgetToBar(EntryWidget, SlotIndex);
}

UQuickSlotEntryWidget* UQuickSlotWidget::CreateQuickSlotEntryWidget() const
{
	TSubclassOf<UQuickSlotEntryWidget> ResolvedEntryWidgetClass = ResolveEntryWidgetClass();
	if (!ResolvedEntryWidgetClass)
	{
		return nullptr;
	}

	if (IsDesignTime() && WidgetTree)
	{
		return WidgetTree->ConstructWidget<UQuickSlotEntryWidget>(ResolvedEntryWidgetClass);
	}

	if (APlayerController* OwningPlayer = GetOwningPlayer())
	{
		return CreateWidget<UQuickSlotEntryWidget>(OwningPlayer, ResolvedEntryWidgetClass);
	}

	return CreateWidget<UQuickSlotEntryWidget>(const_cast<UQuickSlotWidget*>(this), ResolvedEntryWidgetClass);
}

void UQuickSlotWidget::AddWidgetToBar(UWidget* Widget, const int32 SlotIndex) const
{
	if (!UniformGridPanel || !Widget)
	{
		return;
	}

	const int32 Row = SlotIndex / QuickSlotGridColumns;
	const int32 Column = SlotIndex % QuickSlotGridColumns;
	UUniformGridSlot* GridSlot = UniformGridPanel->AddChildToUniformGrid(Widget, Row, Column);
	if (GridSlot)
	{
		GridSlot->SetHorizontalAlignment(HAlign_Fill);
		GridSlot->SetVerticalAlignment(VAlign_Fill);
	}
}

UInventoryComponent* UQuickSlotWidget::ResolveOwningInventoryComponent() const
{
	const APlayerController* PlayerController = GetOwningPlayer();
	const APdPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<APdPlayerState>() : nullptr;
	if (!PlayerState)
	{
		if (const APawn* OwningPawn = GetOwningPlayerPawn())
		{
			PlayerState = OwningPawn->GetPlayerState<APdPlayerState>();
		}
	}

	return PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
}

USkinEquipmentComponent* UQuickSlotWidget::ResolveOwningSkinEquipmentComponent() const
{
	ACharacterBase* Character = Cast<ACharacterBase>(GetOwningPlayerPawn());
	if (!Character)
	{
		const APlayerController* PlayerController = GetOwningPlayer();
		Character = PlayerController ? Cast<ACharacterBase>(PlayerController->GetPawn()) : nullptr;
	}

	return Character ? Character->GetSkinEquipmentComponent() : nullptr;
}

const USkinDefinition* UQuickSlotWidget::ResolveGestureSlotSkinDefinition(
	const USkinEquipmentComponent* SkinEquipmentComponent,
	const int32 QuickSlotIndex) const
{
	const FGameplayTag SlotTag = ResolveGestureSlotTagFromQuickSlotIndex(QuickSlotIndex);
	return SkinEquipmentComponent && SlotTag.IsValid()
		? SkinEquipmentComponent->GetEquippedSkinDefinition(SlotTag)
		: nullptr;
}

TSubclassOf<UQuickSlotEntryWidget> UQuickSlotWidget::ResolveEntryWidgetClass() const
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const TSoftClassPtr<UQuickSlotEntryWidget> EntryWidgetClassDefinition =
			WidgetDefinition->GetQuickSlotEntryWidgetClass();
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

int32 UQuickSlotWidget::ResolveSlotCount() const
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		return FMath::Max(RequiredQuickSlotCount, WidgetDefinition->GetQuickSlotWidgetSettings().SlotCount);
	}

	return FMath::Max(RequiredQuickSlotCount, SlotCount);
}

FMargin UQuickSlotWidget::ResolveSlotPadding() const
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		return WidgetDefinition->GetQuickSlotWidgetSettings().SlotPadding;
	}

	return SlotPadding;
}
