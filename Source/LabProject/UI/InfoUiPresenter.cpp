#include "UI/InfoUiPresenter.h"

#include "Character/PdPlayer.h"
#include "Common/ProjectTagConfig.h"
#include "Components/TileView.h"
#include "Engine/Texture2D.h"
#include "Item/InventoryComponent.h"
#include "Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Mode/PdHUD.h"
#include "PlayerComponent/EquipmentComponent.h"
#include "Pandora/PandoraComponent.h"
#include "Pandora/PandoraDefinition.h"
#include "Pandora/PandoraInstance.h"
#include "PlayerComponent/StatUpgradeComponent.h"
#include "Skin/SkinComponent.h"
#include "Skin/SkinInstance.h"
#include "UI/Widget/EquipSlotWidget.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/LeftEquipmentWidget.h"
#include "UI/Widget/LeftPandoraWidget.h"
#include "UI/Widget/LeftSkinWidget.h"
#include "UI/PandoraLoadoutUiModel.h"
#include "UI/Widget/PandoraEquipSlotWidget.h"
#include "UI/Widget/RightInventoryWidget.h"
#include "UI/Widget/RightPandoraWidget.h"
#include "UI/Widget/RightSkinWidget.h"
#include "UI/Widget/RightStatusWidget.h"
#include "UI/Widget/SelectPandoraWidget.h"
#include "UI/Widget/SkinEquipSlotWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InfoUiPresenter)

DEFINE_LOG_CATEGORY_STATIC(LogInfoUiPresenter, Log, All);

namespace
{
void AppendItemListAsObjects(const FItemList& ItemList, TArray<UObject*>& OutListItems)
{
	OutListItems.Reserve(OutListItems.Num() + ItemList.Items.Num());
	for (const TObjectPtr<UItemInstance>& Item : ItemList.Items)
	{
		if (UItemInstance* ItemInstance = Item.Get())
		{
			OutListItems.Add(ItemInstance);
		}
	}
}

void AppendSkinListAsObjects(const FSkinList& SkinList, TArray<UObject*>& OutListItems)
{
	OutListItems.Reserve(OutListItems.Num() + SkinList.Skins.Num());
	for (const TObjectPtr<USkinInstance>& Skin : SkinList.Skins)
	{
		if (USkinInstance* SkinInstance = Skin.Get())
		{
			OutListItems.Add(SkinInstance);
		}
	}
}

void AppendPandoraListAsObjects(const FPandoraList& PandoraList, TArray<UObject*>& OutListItems)
{
	OutListItems.Reserve(OutListItems.Num() + PandoraList.Pandoras.Num());
	for (const TObjectPtr<UPandoraInstance>& Pandora : PandoraList.Pandoras)
	{
		if (UPandoraInstance* PandoraInstance = Pandora.Get())
		{
			OutListItems.Add(PandoraInstance);
		}
	}
}

FString GetItemDisplayNameForLog(const UItemInstance* ItemInstance)
{
	const UItemDefinition* ItemDefinition = IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
	if (IsValid(ItemDefinition) && !ItemDefinition->DisplayName.IsEmpty())
	{
		return ItemDefinition->DisplayName.ToString();
	}

	return IsValid(ItemDefinition) ? GetNameSafe(ItemDefinition) : GetNameSafe(ItemInstance);
}

FString GetPandoraDisplayNameForLog(const UPandoraInstance* PandoraInstance)
{
	const UPandoraDefinition* PandoraDefinition = IsValid(PandoraInstance) ? PandoraInstance->PandoraDefinition.Get() : nullptr;
	if (IsValid(PandoraDefinition) && !PandoraDefinition->DisplayName.IsEmpty())
	{
		return PandoraDefinition->DisplayName.ToString();
	}

	return IsValid(PandoraDefinition) ? GetNameSafe(PandoraDefinition) : GetNameSafe(PandoraInstance);
}

}

UInfoUiPresenter::UInfoUiPresenter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UWorld* UInfoUiPresenter::GetWorld() const
{
	if (const APdPlayerController* ResolvedController = GetController())
	{
		return ResolvedController->GetWorld();
	}

	return Super::GetWorld();
}

void UInfoUiPresenter::Initialize(APdPlayerController* InController)
{
	if (OwningController == InController)
	{
		return;
	}

	Deinitialize();
	OwningController = InController;
	BindInventoryChangeNotification();
}

void UInfoUiPresenter::Deinitialize()
{
	UnbindInventoryChangeNotification();
	OwningController = nullptr;
	InfoWidget = nullptr;
	CachedSelectedEquipSlot = nullptr;
	CachedSelectedSkinEquipSlot = nullptr;
	CachedSelectedPandoraEquipSlot = nullptr;
	CachedFirstWeapon = nullptr;
	CachedSecondWeapon = nullptr;
	CachedThirdWeapon = nullptr;
	CurrentLeftUiTag = FGameplayTag();
	CurrentItemFilterTag = FGameplayTag();
	bUseItemTypeFilter = false;
}

void UInfoUiPresenter::BindInfoUi(UInfoWidget* InInfoWidget)
{
	InfoWidget = InInfoWidget;
	BindInventoryChangeNotification();
	BindStatusWidgetEvents();
}

UItemInstance* UInfoUiPresenter::GetSelectedWeapon(EEnum_Direction Direction) const
{
	switch (Direction)
	{
	case EEnum_Direction::Up:
		return CachedSecondWeapon;
	case EEnum_Direction::Right:
		return CachedThirdWeapon;
	case EEnum_Direction::Left:
		return CachedFirstWeapon;
	case EEnum_Direction::Center:
	case EEnum_Direction::Down:
	default:
		return nullptr;
	}
}

UPandoraInstance* UInfoUiPresenter::GetSelectedPandora(EEnum_Direction Direction) const
{
	const APdPlayerState* PlayerState = GetCachedPlayerState();
	const UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	return PandoraComponent ? PandoraComponent->GetPandoraLoadoutInstance(Direction) : nullptr;
}

void UInfoUiPresenter::HandleSelectedPandoraDirection(EEnum_Direction Direction)
{
	APdPlayerController* Controller = GetController();
	APdPlayer* PlayerCharacter = Controller ? Cast<APdPlayer>(Controller->GetPawn()) : nullptr;
	UEquipmentComponent* EquipmentComponent = PlayerCharacter ? PlayerCharacter->GetEquipmentComponent() : nullptr;
	APdPlayerState* PlayerState = GetCachedPlayerState();
	UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;

	if (Direction == EEnum_Direction::Down)
	{
		UE_LOG(LogInfoUiPresenter, Log, TEXT("Select UI confirmed: direction=%d pandora=None weapon=None"),
			static_cast<int32>(Direction));

		if (EquipmentComponent)
		{
			const bool bRequested = EquipmentComponent->RequestWeaponUnequip();
			UE_LOG(LogInfoUiPresenter, Log, TEXT("Weapon unequip requested from select UI: direction=%d result=%s"),
				static_cast<int32>(Direction),
				bRequested ? TEXT("true") : TEXT("false"));
		}

		if (PandoraComponent)
		{
			PandoraComponent->RequestPandoraSelection(nullptr);
		}
		return;
	}

	UPandoraInstance* CurrentPandora = GetSelectedPandora(Direction);
	const UPandoraDefinition* PandoraDefinition = CurrentPandora ? CurrentPandora->PandoraDefinition.Get() : nullptr;
	if (!PandoraDefinition && PandoraComponent)
	{
		PandoraDefinition = PandoraComponent->GetPandoraLoadoutDefinition(Direction);
	}
	UItemInstance* CurrentWeapon = GetSelectedWeapon(Direction);
	const UItemDefinition* ItemDefinition = CurrentWeapon ? CurrentWeapon->ItemDefinition.Get() : nullptr;

	UE_LOG(LogInfoUiPresenter, Log,
		TEXT("Select UI confirmed: direction=%d pandora=%s pandoraAsset=%s pandoraTag=%s weapon=%s weaponAsset=%s weaponTag=%s"),
		static_cast<int32>(Direction),
		*GetPandoraDisplayNameForLog(CurrentPandora),
		*GetNameSafe(PandoraDefinition),
		PandoraDefinition && PandoraDefinition->IdTag.IsValid() ? *PandoraDefinition->IdTag.ToString() : TEXT("None"),
		*GetItemDisplayNameForLog(CurrentWeapon),
		*GetNameSafe(ItemDefinition),
		ItemDefinition && ItemDefinition->IdTag.IsValid() ? *ItemDefinition->IdTag.ToString() : TEXT("None"));

	if (PandoraComponent && PandoraDefinition)
	{
		const bool bPandoraRequested = PandoraComponent->RequestPandoraSelection(PandoraDefinition);
		UE_LOG(LogInfoUiPresenter, Log, TEXT("Pandora selection requested from select UI: direction=%d pandora=%s tag=%s result=%s"),
			static_cast<int32>(Direction),
			*GetNameSafe(PandoraDefinition),
			PandoraDefinition->IdTag.IsValid() ? *PandoraDefinition->IdTag.ToString() : TEXT("None"),
			bPandoraRequested ? TEXT("true") : TEXT("false"));
	}
	else
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("Pandora selection skipped: direction=%d component=%s pandora=%s"),
			static_cast<int32>(Direction),
			*GetNameSafe(PandoraComponent),
			*GetNameSafe(PandoraDefinition));
	}

	if (!EquipmentComponent)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("Weapon selection skipped: direction=%d controller=%s pawn=%s equipment=null"),
			static_cast<int32>(Direction),
			*GetNameSafe(Controller),
			*GetNameSafe(PlayerCharacter));
		return;
	}

	if (!IsValid(CurrentWeapon))
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("Weapon selection skipped: direction=%d selected weapon is invalid"),
			static_cast<int32>(Direction));
		return;
	}

	const bool bRequested = EquipmentComponent->RequestWeaponSelection(CurrentWeapon);
	UE_LOG(LogInfoUiPresenter, Log, TEXT("Weapon selection requested from select UI: direction=%d item=%s definition=%s tag=%s result=%s"),
		static_cast<int32>(Direction),
		*GetNameSafe(CurrentWeapon),
		*GetNameSafe(ItemDefinition),
		ItemDefinition ? *ItemDefinition->IdTag.ToString() : TEXT("None"),
		bRequested ? TEXT("true") : TEXT("false"));
}

void UInfoUiPresenter::HandleOpenedInfoUi()
{
	BindInventoryChangeNotification();
	BindStatusWidgetEvents();
	RefreshSelectPandoraLoadoutImages();
	RefreshSelectPandoraCompatibilityState();

	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	if (!CurrentInfoWidget)
	{
		return;
	}

	if (ULeftEquipmentWidget* LeftEquipmentWidget = CurrentInfoWidget->GetLeftEquipmentWidget())
	{
		LeftEquipmentWidget->InitialzeEquipSlots();
	}

	if (URightInventoryWidget* RightInventoryWidget = CurrentInfoWidget->GetRightInventoryWidget())
	{
		RightInventoryWidget->ToggleActiveFiliterButtons(true);
		RightInventoryWidget->ClearTileViewItemClicked();
	}
}

void UInfoUiPresenter::HandleClickedStatUpButton(FGameplayTag StatTag)
{
	APdPlayerState* PdPlayerState = GetCachedPlayerState();
	UStatUpgradeComponent* StatUpgradeComponent = PdPlayerState ? PdPlayerState->GetStatUpgradeComponent() : nullptr;
	UE_LOG(LogInfoUiPresenter, Log, TEXT("[StatUpgrade] Presenter received stat up: controller=%s playerState=%s component=%s tag=%s valid=%s"),
		*GetNameSafe(GetController()),
		*GetNameSafe(PdPlayerState),
		*GetNameSafe(StatUpgradeComponent),
		*StatTag.ToString(),
		StatTag.IsValid() ? TEXT("true") : TEXT("false"));

	if (StatUpgradeComponent)
	{
		const bool bRequested = StatUpgradeComponent->RequestStatUp(StatTag);
		UE_LOG(LogInfoUiPresenter, Log, TEXT("[StatUpgrade] Presenter request result: component=%s tag=%s result=%s"),
			*GetNameSafe(StatUpgradeComponent),
			*StatTag.ToString(),
			bRequested ? TEXT("true") : TEXT("false"));
	}
	else
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[StatUpgrade] Presenter request skipped: StatUpgradeComponent is null."));
	}
}

void UInfoUiPresenter::HandleClickedInfoCenterButton(FGameplayTag LeftUiTag, FGameplayTag RightUiTag)
{
	(void)RightUiTag;

	if (!LeftUiTag.IsValid())
	{
		return;
	}

	CurrentLeftUiTag = LeftUiTag;

	APdPlayerController* Controller = GetController();
	if (!Controller)
	{
		return;
	}

	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	if (!CurrentInfoWidget)
	{
		return;
	}

	if (LeftUiTag == GetProfileLeftUiTag())
	{
		BindStatusWidgetEvents();
		return;
	}

	if (LeftUiTag == GetEquipmentLeftUiTag())
	{
		bUseItemTypeFilter = false;
		CurrentItemFilterTag = FGameplayTag();

		if (URightInventoryWidget* RightInventoryWidget = CurrentInfoWidget->GetRightInventoryWidget())
		{
			RefreshInventoryTileView();

			RightInventoryWidget->OnClicked_FilterAllButton.Clear();
			RightInventoryWidget->OnClicked_FilterAllButton.AddUniqueDynamic(this, &ThisClass::HandleClickedItemFilterAllButton);

			RightInventoryWidget->OnClicked_FilterTypeButton.Clear();
			RightInventoryWidget->OnClicked_FilterTypeButton.AddUniqueDynamic(this, &ThisClass::HandleClickedItemFilterTypeButton);
		}

		if (ULeftEquipmentWidget* LeftEquipmentWidget = CurrentInfoWidget->GetLeftEquipmentWidget())
		{
			LeftEquipmentWidget->OnClicked_EquipTypeSlot.Clear();
			LeftEquipmentWidget->OnClicked_EquipTypeSlot.AddUniqueDynamic(this, &ThisClass::HandleClickedItemEquipTypeSlot);
		}
		return;
	}

	if (LeftUiTag == GetSkinEquipmentLeftUiTag())
	{
		TArray<UObject*> CurrentSkinList;

		const APdPlayerState* PdPlayerState = GetCachedPlayerState();
		const USkinComponent* SkinComponent = PdPlayerState ? PdPlayerState->GetSkinComponent() : nullptr;
		if (SkinComponent)
		{
			AppendSkinListAsObjects(SkinComponent->AllSkinList, CurrentSkinList);
		}

		if (URightSkinWidget* RightSkinWidget = CurrentInfoWidget->GetRightSkinWidget())
		{
			RightSkinWidget->SetTileView(CurrentSkinList);

			RightSkinWidget->OnClicked_SkinFilterAllButton.Clear();
			RightSkinWidget->OnClicked_SkinFilterAllButton.AddUniqueDynamic(this, &ThisClass::HandleClickedSkinFilterAllButton);

			RightSkinWidget->OnClicked_SkinFilterTypeButton.Clear();
			RightSkinWidget->OnClicked_SkinFilterTypeButton.AddUniqueDynamic(this, &ThisClass::HandleClickedSkinFilterTypeButton);
		}

		if (ULeftSkinWidget* LeftSkinWidget = CurrentInfoWidget->GetLeftSkinWidget())
		{
			LeftSkinWidget->OnClicked_SkinEquipTypeSlot.Clear();
			LeftSkinWidget->OnClicked_SkinEquipTypeSlot.AddUniqueDynamic(this, &ThisClass::HandleClickedSkinEquipTypeSlot);
		}
		return;
	}

	if (LeftUiTag == GetPandoraEquipmentLeftUiTag())
	{
		TArray<UObject*> CurrentPandoraList;

		const APdPlayerState* PdPlayerState = GetCachedPlayerState();
		const UPandoraComponent* PandoraComponent = PdPlayerState ? PdPlayerState->GetPandoraComponent() : nullptr;
		if (PandoraComponent)
		{
			AppendPandoraListAsObjects(PandoraComponent->AllPandoraList, CurrentPandoraList);
		}

		if (URightPandoraWidget* RightPandoraWidget = CurrentInfoWidget->GetRightPandoraWidget())
		{
			RightPandoraWidget->SetTileViewAndShowLockState(CurrentPandoraList);

			RightPandoraWidget->OnClicked_PandoraFilterAllButton.Clear();
			RightPandoraWidget->OnClicked_PandoraFilterAllButton.AddUniqueDynamic(this, &ThisClass::HandleClickedPandoraFilterAllButton);

			RightPandoraWidget->OnClicked_PandoraFilterTypeButton.Clear();
			RightPandoraWidget->OnClicked_PandoraFilterTypeButton.AddUniqueDynamic(this, &ThisClass::HandleClickedPandoraFilterTypeButton);
		}

		if (ULeftPandoraWidget* LeftPandoraWidget = CurrentInfoWidget->GetLeftPandoraWidget())
		{
			LeftPandoraWidget->OnClicked_PandoraEquipSlot.Clear();
			LeftPandoraWidget->OnClicked_PandoraEquipSlot.AddUniqueDynamic(this, &ThisClass::HandleClickedPandoraEquipSlot);
		}
	}
}

void UInfoUiPresenter::HandleClickedItemSlot(UObject* Item)
{
	APdPlayerController* Controller = GetController();
	UItemInstance* ItemInstance = Cast<UItemInstance>(Item);
	if (!Controller || !ItemInstance)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("Item slot click skipped: controller=%s item=%s itemInstance=%s"),
			*GetNameSafe(Controller),
			*GetNameSafe(Item),
			*GetNameSafe(ItemInstance));
		return;
	}

	if (CachedSelectedEquipSlot)
	{
		CachedSelectedEquipSlot->SetData(ItemInstance);
	}

	if (UInfoWidget* CurrentInfoWidget = GetInfoWidget())
	{
		if (URightInventoryWidget* RightInventoryWidget = CurrentInfoWidget->GetRightInventoryWidget())
		{
			RightInventoryWidget->ClearTileViewItemClicked();
		}
	}

	const UItemDefinition* ItemDefinition = ItemInstance->ItemDefinition;
	if (!ItemDefinition || !ItemDefinition->IdTag.IsValid())
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("Item slot click skipped: item=%s definition=%s idTag=%s"),
			*GetNameSafe(ItemInstance),
			*GetNameSafe(ItemDefinition),
			ItemDefinition ? *ItemDefinition->IdTag.ToString() : TEXT("None"));
		return;
	}

	const FGameplayTag WeaponItemTypeTag = GetWeaponItemTypeTag();
	if (!WeaponItemTypeTag.IsValid() || !ItemDefinition->IdTag.MatchesTag(WeaponItemTypeTag))
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("Item slot click ignored by weapon filter: item=%s definition=%s idTag=%s weaponTypeTag=%s"),
			*GetNameSafe(ItemInstance),
			*GetNameSafe(ItemDefinition),
			*ItemDefinition->IdTag.ToString(),
			*WeaponItemTypeTag.ToString());
		return;
	}

	UTexture2D* WeaponTexture = ItemDefinition->IconTexture.LoadSynchronous();
	if (!WeaponTexture)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("Weapon slot selection skipped: missing icon texture item=%s definition=%s"),
			*GetNameSafe(ItemInstance),
			*GetNameSafe(ItemDefinition));
		return;
	}

	const int32 Nth = CachedSelectedEquipSlot ? CachedSelectedEquipSlot->GetNth() : 0;
	if (USelectPandoraWidget* SelectPandoraWidget = GetSelectPandoraWidget())
	{
		SelectPandoraWidget->SetWeaponImage(Nth, WeaponTexture);
	}

	switch (Nth)
	{
	case 1:
		CachedFirstWeapon = ItemInstance;
		break;
	case 2:
		CachedSecondWeapon = ItemInstance;
		break;
	case 3:
		CachedThirdWeapon = ItemInstance;
		break;
	default:
		break;
	}

	UE_LOG(LogInfoUiPresenter, Log, TEXT("Weapon cached in select slot: slot=%d item=%s definition=%s idTag=%s"),
		Nth,
		*GetNameSafe(ItemInstance),
		*GetNameSafe(ItemDefinition),
		*ItemDefinition->IdTag.ToString());
	RefreshSelectPandoraCompatibilityState();
}

void UInfoUiPresenter::HandleClickedItemEquipTypeSlot(FGameplayTag EquipTypeTag, UEquipSlotWidget* SelectedEquipSlot, bool bIsSelectedAnyButton)
{
	APdPlayerController* Controller = GetController();
	if (!Controller)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[InventoryFilter] EquipSlot click skipped: controller is null. tag=%s slot=%s selectedAny=%s"),
			*EquipTypeTag.ToString(),
			*GetNameSafe(SelectedEquipSlot),
			bIsSelectedAnyButton ? TEXT("true") : TEXT("false"));
		return;
	}

	CachedSelectedEquipSlot = SelectedEquipSlot;
	UE_LOG(LogInfoUiPresenter, Log, TEXT("[InventoryFilter] EquipSlot clicked: controller=%s tag=%s slot=%s slotNth=%d selectedAny=%s"),
		*GetNameSafe(Controller),
		*EquipTypeTag.ToString(),
		*GetNameSafe(SelectedEquipSlot),
		SelectedEquipSlot ? SelectedEquipSlot->GetNth() : INDEX_NONE,
		bIsSelectedAnyButton ? TEXT("true") : TEXT("false"));

	HandleClickedItemFilterTypeButton(EquipTypeTag);

	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	URightInventoryWidget* RightInventoryWidget = CurrentInfoWidget ? CurrentInfoWidget->GetRightInventoryWidget() : nullptr;
	if (!RightInventoryWidget)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[InventoryFilter] EquipSlot click could not update right inventory: info=%s rightInventory=%s"),
			*GetNameSafe(CurrentInfoWidget),
			*GetNameSafe(RightInventoryWidget));
		return;
	}

	RightInventoryWidget->ToggleActiveFiliterButtons(bIsSelectedAnyButton);
	if (bIsSelectedAnyButton)
	{
		UE_LOG(LogInfoUiPresenter, Log, TEXT("[InventoryFilter] EquipSlot click ended in deselect state: filter buttons active=%s"),
			bIsSelectedAnyButton ? TEXT("true") : TEXT("false"));
		return;
	}

	if (UTileView* TileView = RightInventoryWidget->GetTileView())
	{
		TileView->OnItemClicked().RemoveAll(this);
		TileView->OnItemClicked().AddUObject(this, &ThisClass::HandleClickedItemSlot);
		UE_LOG(LogInfoUiPresenter, Log, TEXT("[InventoryFilter] Tile item click rebound for equipment selection: tileView=%s"),
			*GetNameSafe(TileView));
	}
	else
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[InventoryFilter] EquipSlot click could not bind tile click: tileView is null."));
	}
}

void UInfoUiPresenter::HandleClickedItemFilterTypeButton(FGameplayTag TypeTag)
{
	if (!TypeTag.IsValid())
	{
		bUseItemTypeFilter = false;
		CurrentItemFilterTag = FGameplayTag();
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[InventoryFilter] Type filter clicked with invalid tag. Falling back to all items."));
		RefreshInventoryTileView();
		return;
	}

	bUseItemTypeFilter = true;
	CurrentItemFilterTag = TypeTag;
	UE_LOG(LogInfoUiPresenter, Log, TEXT("[InventoryFilter] Type filter selected: tag=%s"),
		*CurrentItemFilterTag.ToString());

	APdPlayerController* Controller = GetController();
	if (!Controller)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[InventoryFilter] Type filter refresh skipped: controller is null. tag=%s"),
			*CurrentItemFilterTag.ToString());
		return;
	}

	RefreshInventoryTileView();
}

void UInfoUiPresenter::HandleClickedItemFilterAllButton()
{
	bUseItemTypeFilter = false;
	CurrentItemFilterTag = FGameplayTag();
	UE_LOG(LogInfoUiPresenter, Log, TEXT("[InventoryFilter] All filter selected."));

	APdPlayerController* Controller = GetController();
	if (!Controller)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[InventoryFilter] All filter refresh skipped: controller is null."));
		return;
	}

	RefreshInventoryTileView();
}

void UInfoUiPresenter::HandleClickedSkinSlot(UObject* Item)
{
	APdPlayerController* Controller = GetController();
	USkinInstance* SkinInstance = Cast<USkinInstance>(Item);
	if (!Controller || !SkinInstance)
	{
		return;
	}

	if (CachedSelectedSkinEquipSlot)
	{
		CachedSelectedSkinEquipSlot->SetData(SkinInstance);
	}

	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	if (URightSkinWidget* RightSkinWidget = CurrentInfoWidget ? CurrentInfoWidget->GetRightSkinWidget() : nullptr)
	{
		RightSkinWidget->ClearTileViewItemClicked();
	}
}

void UInfoUiPresenter::HandleClickedSkinEquipTypeSlot(FGameplayTag EquipTypeTag, USkinEquipSlotWidget* SelectedEquipSlot, bool bIsSelectedAnyButton)
{
	APdPlayerController* Controller = GetController();
	if (!Controller)
	{
		return;
	}

	CachedSelectedSkinEquipSlot = SelectedEquipSlot;

	HandleClickedSkinFilterTypeButton(EquipTypeTag);

	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	URightSkinWidget* RightSkinWidget = CurrentInfoWidget ? CurrentInfoWidget->GetRightSkinWidget() : nullptr;
	if (!RightSkinWidget)
	{
		return;
	}

	RightSkinWidget->ToggleActiveFiliterButtons(bIsSelectedAnyButton);
	if (bIsSelectedAnyButton)
	{
		return;
	}

	if (UTileView* TileView = RightSkinWidget->GetTileView())
	{
		TileView->OnItemClicked().RemoveAll(this);
		TileView->OnItemClicked().AddUObject(this, &ThisClass::HandleClickedSkinSlot);
	}
}

void UInfoUiPresenter::HandleClickedSkinFilterTypeButton(FGameplayTag TypeTag)
{
	APdPlayerController* Controller = GetController();
	if (!Controller)
	{
		return;
	}

	TArray<UObject*> CurrentSkinList;

	const APdPlayerState* PdPlayerState = GetCachedPlayerState();
	const USkinComponent* SkinComponent = PdPlayerState ? PdPlayerState->GetSkinComponent() : nullptr;
	if (SkinComponent)
	{
		if (const FSkinList* FoundSkinList = SkinComponent->Map_Type_SkinList.Find(TypeTag))
		{
			AppendSkinListAsObjects(*FoundSkinList, CurrentSkinList);
		}
	}

	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	if (URightSkinWidget* RightSkinWidget = CurrentInfoWidget ? CurrentInfoWidget->GetRightSkinWidget() : nullptr)
	{
		RightSkinWidget->SetTileView(CurrentSkinList);
	}
}

void UInfoUiPresenter::HandleClickedSkinFilterAllButton()
{
	APdPlayerController* Controller = GetController();
	if (!Controller)
	{
		return;
	}

	TArray<UObject*> CurrentSkinList;

	const APdPlayerState* PdPlayerState = GetCachedPlayerState();
	const USkinComponent* SkinComponent = PdPlayerState ? PdPlayerState->GetSkinComponent() : nullptr;
	if (SkinComponent)
	{
		AppendSkinListAsObjects(SkinComponent->AllSkinList, CurrentSkinList);
	}

	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	if (URightSkinWidget* RightSkinWidget = CurrentInfoWidget ? CurrentInfoWidget->GetRightSkinWidget() : nullptr)
	{
		RightSkinWidget->SetTileView(CurrentSkinList);
	}
}

void UInfoUiPresenter::HandleClickedPandoraSlot(UObject* Item)
{
	APdPlayerController* Controller = GetController();
	UPandoraInstance* PandoraInstance = Cast<UPandoraInstance>(Item);
	if (!Controller || !PandoraInstance)
	{
		return;
	}

	if (CachedSelectedPandoraEquipSlot)
	{
		CachedSelectedPandoraEquipSlot->SetData(PandoraInstance);
	}

	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	if (URightPandoraWidget* RightPandoraWidget = CurrentInfoWidget ? CurrentInfoWidget->GetRightPandoraWidget() : nullptr)
	{
		RightPandoraWidget->ClearTileViewItemClicked();
	}

	const UPandoraDefinition* PandoraDefinition = PandoraInstance->PandoraDefinition;
	UTexture2D* PandoraTexture = PandoraDefinition ? PandoraDefinition->IconTexture.Get() : nullptr;
	USelectPandoraWidget* SelectPandoraWidget = GetSelectPandoraWidget();
	if (!PandoraTexture || !SelectPandoraWidget)
	{
		return;
	}

	const int32 Nth = CachedSelectedPandoraEquipSlot ? CachedSelectedPandoraEquipSlot->GetNth() : 0;
	SelectPandoraWidget->SetPandoraImage(Nth, PandoraTexture);

	APdPlayerState* PlayerState = GetCachedPlayerState();
	UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	if (PandoraComponent)
	{
		PandoraComponent->RequestSetPandoraLoadoutSlot(FPandoraLoadoutUiModel::GetDirectionFromSelectSlotNumber(Nth), PandoraDefinition);
	}

	RefreshSelectPandoraCompatibilityState();
}

void UInfoUiPresenter::HandleClickedPandoraEquipSlot(UPandoraEquipSlotWidget* SelectedPandoraEquipSlot, bool bIsSelectedAnyButton)
{
	APdPlayerController* Controller = GetController();
	if (!Controller)
	{
		return;
	}

	CachedSelectedPandoraEquipSlot = SelectedPandoraEquipSlot;

	if (CachedSelectedPandoraEquipSlot)
	{
		CachedSelectedPandoraEquipSlot->ToggleText_Apply(!bIsSelectedAnyButton);
	}

	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	URightPandoraWidget* RightPandoraWidget = CurrentInfoWidget ? CurrentInfoWidget->GetRightPandoraWidget() : nullptr;
	if (!RightPandoraWidget)
	{
		return;
	}

	if (bIsSelectedAnyButton)
	{
		return;
	}

	if (UTileView* TileView = RightPandoraWidget->GetTileView())
	{
		TileView->OnItemClicked().RemoveAll(this);
		TileView->OnItemClicked().AddUObject(this, &ThisClass::HandleClickedPandoraSlot);
	}
}

void UInfoUiPresenter::HandleClickedPandoraFilterTypeButton(FGameplayTag TypeTag)
{
	APdPlayerController* Controller = GetController();
	if (!Controller)
	{
		return;
	}

	TArray<UObject*> CurrentPandoraList;

	const APdPlayerState* PdPlayerState = GetCachedPlayerState();
	const UPandoraComponent* PandoraComponent = PdPlayerState ? PdPlayerState->GetPandoraComponent() : nullptr;
	if (PandoraComponent)
	{
		if (const FPandoraList* FoundPandoraList = PandoraComponent->Map_Type_PandoraList.Find(TypeTag))
		{
			AppendPandoraListAsObjects(*FoundPandoraList, CurrentPandoraList);
		}
	}

	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	if (URightPandoraWidget* RightPandoraWidget = CurrentInfoWidget ? CurrentInfoWidget->GetRightPandoraWidget() : nullptr)
	{
		RightPandoraWidget->SetTileViewAndShowLockState(CurrentPandoraList);
	}
}

void UInfoUiPresenter::HandleClickedPandoraFilterAllButton()
{
	APdPlayerController* Controller = GetController();
	if (!Controller)
	{
		return;
	}

	TArray<UObject*> CurrentPandoraList;

	const APdPlayerState* PdPlayerState = GetCachedPlayerState();
	const UPandoraComponent* PandoraComponent = PdPlayerState ? PdPlayerState->GetPandoraComponent() : nullptr;
	if (PandoraComponent)
	{
		AppendPandoraListAsObjects(PandoraComponent->AllPandoraList, CurrentPandoraList);
	}

	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	if (URightPandoraWidget* RightPandoraWidget = CurrentInfoWidget ? CurrentInfoWidget->GetRightPandoraWidget() : nullptr)
	{
		RightPandoraWidget->SetTileViewAndShowLockState(CurrentPandoraList);
	}
}

UInfoWidget* UInfoUiPresenter::GetInfoWidget() const
{
	return InfoWidget;
}

USelectPandoraWidget* UInfoUiPresenter::GetSelectPandoraWidget() const
{
	APdPlayerController* Controller = GetController();
	const APdHUD* HUD = Controller ? Cast<APdHUD>(Controller->GetHUD()) : nullptr;
	return HUD ? HUD->GetSelectPandoraWidget() : nullptr;
}

APdPlayerController* UInfoUiPresenter::GetController() const
{
	return OwningController.Get();
}

APdPlayerState* UInfoUiPresenter::GetCachedPlayerState() const
{
	const APdPlayerController* Controller = GetController();
	return Controller ? Controller->GetPlayerState<APdPlayerState>() : nullptr;
}

FGameplayTag UInfoUiPresenter::GetProfileLeftUiTag() const
{
	return UProjectTagConfig::Get(this)->GetUiProfileLeftTag();
}

FGameplayTag UInfoUiPresenter::GetEquipmentLeftUiTag() const
{
	return UProjectTagConfig::Get(this)->GetUiEquipmentLeftTag();
}

FGameplayTag UInfoUiPresenter::GetSkinEquipmentLeftUiTag() const
{
	return UProjectTagConfig::Get(this)->GetUiSkinEquipmentLeftTag();
}

FGameplayTag UInfoUiPresenter::GetPandoraEquipmentLeftUiTag() const
{
	return UProjectTagConfig::Get(this)->GetUiPandoraEquipmentLeftTag();
}

FGameplayTag UInfoUiPresenter::GetWeaponItemTypeTag() const
{
	return UProjectTagConfig::Get(this)->GetItemWeaponTypeTag();
}

void UInfoUiPresenter::RefreshSelectPandoraCompatibilityState() const
{
	USelectPandoraWidget* SelectPandoraWidget = GetSelectPandoraWidget();
	if (!SelectPandoraWidget)
	{
		return;
	}

	const APdPlayerState* PlayerState = GetCachedPlayerState();
	const UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	const TArray<FPandoraSelectSlotUiData> Slots = FPandoraLoadoutUiModel::BuildSelectSlots(
		PandoraComponent,
		CachedFirstWeapon,
		CachedSecondWeapon,
		CachedThirdWeapon);

	for (const FPandoraSelectSlotUiData& Slot : Slots)
	{
		SelectPandoraWidget->SetPandoraEnabled(Slot.SlotNumber, Slot.bCompatibleWithWeapon);
	}
}

void UInfoUiPresenter::RefreshSelectPandoraLoadoutImages() const
{
	USelectPandoraWidget* SelectPandoraWidget = GetSelectPandoraWidget();
	if (!SelectPandoraWidget)
	{
		return;
	}

	const APdPlayerState* PlayerState = GetCachedPlayerState();
	const UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	const TArray<FPandoraSelectSlotUiData> Slots = FPandoraLoadoutUiModel::BuildSelectSlots(
		PandoraComponent,
		CachedFirstWeapon,
		CachedSecondWeapon,
		CachedThirdWeapon);

	for (const FPandoraSelectSlotUiData& Slot : Slots)
	{
		SelectPandoraWidget->SetPandoraImage(Slot.SlotNumber, Slot.IconTexture);
	}
}

void UInfoUiPresenter::BindInventoryChangeNotification()
{
	APdPlayerState* PdPlayerState = GetCachedPlayerState();
	UInventoryComponent* InventoryComponent = PdPlayerState ? PdPlayerState->GetInventoryComponent() : nullptr;
	if (BoundInventoryComponent == InventoryComponent)
	{
		return;
	}

	UnbindInventoryChangeNotification();

	BoundInventoryComponent = InventoryComponent;
	if (BoundInventoryComponent)
	{
		BoundInventoryComponent->OnInventoryChanged.AddUObject(this, &ThisClass::HandleInventoryChanged);
	}
}

void UInfoUiPresenter::UnbindInventoryChangeNotification()
{
	if (BoundInventoryComponent)
	{
		BoundInventoryComponent->OnInventoryChanged.RemoveAll(this);
		BoundInventoryComponent = nullptr;
	}
}

void UInfoUiPresenter::BindStatusWidgetEvents()
{
	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	URightStatusWidget* RightStatusWidget = CurrentInfoWidget ? CurrentInfoWidget->GetRightStatusWidget() : nullptr;
	if (!RightStatusWidget)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[StatUpgrade] Failed to bind stat up delegate: info=%s rightStatus=%s"),
			*GetNameSafe(CurrentInfoWidget),
			*GetNameSafe(RightStatusWidget));
		return;
	}

	RightStatusWidget->OnClicked_StatUpButton.RemoveDynamic(this, &ThisClass::HandleClickedStatUpButton);
	RightStatusWidget->OnClicked_StatUpButton.AddUniqueDynamic(this, &ThisClass::HandleClickedStatUpButton);
	UE_LOG(LogInfoUiPresenter, Log, TEXT("[StatUpgrade] Bound stat up delegate: info=%s rightStatus=%s"),
		*GetNameSafe(CurrentInfoWidget),
		*GetNameSafe(RightStatusWidget));
}

void UInfoUiPresenter::HandleInventoryChanged()
{
	UE_LOG(LogInfoUiPresenter, Log, TEXT("[InventoryFilter] Inventory changed: refreshing tile view. left=%s filterEnabled=%s filterTag=%s"),
		*CurrentLeftUiTag.ToString(),
		bUseItemTypeFilter ? TEXT("true") : TEXT("false"),
		*CurrentItemFilterTag.ToString());
	RefreshInventoryTileView();
}

void UInfoUiPresenter::RefreshInventoryTileView()
{
	const FGameplayTag EquipmentLeftUiTag = GetEquipmentLeftUiTag();
	if (CurrentLeftUiTag != EquipmentLeftUiTag)
	{
		UE_LOG(LogInfoUiPresenter, Log, TEXT("[InventoryFilter] Refresh skipped: currentLeft=%s equipmentLeft=%s"),
			*CurrentLeftUiTag.ToString(),
			*EquipmentLeftUiTag.ToString());
		return;
	}

	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	if (!CurrentInfoWidget)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[InventoryFilter] Refresh skipped: InfoWidget is null."));
		return;
	}

	URightInventoryWidget* RightInventoryWidget = CurrentInfoWidget->GetRightInventoryWidget();
	if (!RightInventoryWidget)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[InventoryFilter] Refresh skipped: RightInventoryWidget is null. info=%s"),
			*GetNameSafe(CurrentInfoWidget));
		return;
	}

	if (!CurrentInfoWidget->IsInViewport())
	{
		UE_LOG(LogInfoUiPresenter, Log, TEXT("[InventoryFilter] Refresh skipped: InfoWidget is not in viewport. info=%s"),
			*GetNameSafe(CurrentInfoWidget));
		return;
	}

	TArray<UObject*> CurrentItemList;

	const APdPlayerState* PdPlayerState = GetCachedPlayerState();
	const UInventoryComponent* InventoryComponent = PdPlayerState ? PdPlayerState->GetInventoryComponent() : nullptr;
	UE_LOG(LogInfoUiPresenter, Log, TEXT("[InventoryFilter] Refresh started: playerState=%s inventory=%s allCount=%d mapTypes=%d filterEnabled=%s filterTag=%s"),
		*GetNameSafe(PdPlayerState),
		*GetNameSafe(InventoryComponent),
		InventoryComponent ? InventoryComponent->AllItemList.Items.Num() : 0,
		InventoryComponent ? InventoryComponent->Map_Type_ItemList.Num() : 0,
		bUseItemTypeFilter ? TEXT("true") : TEXT("false"),
		*CurrentItemFilterTag.ToString());
	if (InventoryComponent)
	{
		const bool bCanUseTypeFilter = bUseItemTypeFilter
			&& CurrentItemFilterTag.IsValid()
			&& !InventoryComponent->Map_Type_ItemList.IsEmpty();

		if (bCanUseTypeFilter)
		{
			if (const FItemList* FoundItemList = InventoryComponent->Map_Type_ItemList.Find(CurrentItemFilterTag))
			{
				AppendItemListAsObjects(*FoundItemList, CurrentItemList);
				UE_LOG(LogInfoUiPresenter, Log, TEXT("[InventoryFilter] Type filter matched: tag=%s sourceCount=%d objectCount=%d"),
					*CurrentItemFilterTag.ToString(),
					FoundItemList->Items.Num(),
					CurrentItemList.Num());
			}
			else
			{
				UE_LOG(LogInfoUiPresenter, Warning, TEXT("[InventoryFilter] Type filter found no list: tag=%s mapTypes=%d"),
					*CurrentItemFilterTag.ToString(),
					InventoryComponent->Map_Type_ItemList.Num());
			}
		}
		else
		{
			AppendItemListAsObjects(InventoryComponent->AllItemList, CurrentItemList);
			UE_LOG(LogInfoUiPresenter, Log, TEXT("[InventoryFilter] Using all items: canUseTypeFilter=%s allSourceCount=%d objectCount=%d"),
				bCanUseTypeFilter ? TEXT("true") : TEXT("false"),
				InventoryComponent->AllItemList.Items.Num(),
				CurrentItemList.Num());
		}
	}
	else
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[InventoryFilter] Refresh has no inventory component."));
	}

	RightInventoryWidget->SetTileView(CurrentItemList);
	UE_LOG(LogInfoUiPresenter, Log, TEXT("[InventoryFilter] Tile view updated: itemCount=%d"),
		CurrentItemList.Num());
}
