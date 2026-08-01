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
#include "Component/Player/StatUpgradeComponent.h"
#include "Skin/SkinComponent.h"
#include "Skin/SkinDefinition.h"
#include "Skin/SkinEquipmentComponent.h"
#include "Skin/SkinInstance.h"
#include "UI/Widget/EquipSlotWidget.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/InventorySlotViewData.h"
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
#include "UI/Widget/SkinSlotViewData.h"

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
	UnbindPandoraLoadoutChangeNotification();
	OwningController = nullptr;
	InfoWidget = nullptr;
	CachedSelectedEquipSlot = nullptr;
	CachedSelectedEquipTypeTag = FGameplayTag();
	CachedSelectedSkinEquipSlot = nullptr;
	CachedSelectedSkinEquipTypeTag = FGameplayTag();
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
	if (InfoWidget)
	{
		InfoWidget->OnDroppedItemToCharacterPanel.Clear();
		InfoWidget->OnDroppedItemToCharacterPanel.AddUniqueDynamic(this, &ThisClass::HandleDroppedItemToCharacterPanel);
		InfoWidget->OnDroppedSkinToCharacterPanel.Clear();
		InfoWidget->OnDroppedSkinToCharacterPanel.AddUniqueDynamic(this, &ThisClass::HandleDroppedSkinToCharacterPanel);
	}
	BindInventoryChangeNotification();
	BindPandoraLoadoutChangeNotification();
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
	BindPandoraLoadoutChangeNotification();
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
	if (ULeftSkinWidget* LeftSkinWidget = CurrentInfoWidget->GetLeftSkinWidget())
	{
		LeftSkinWidget->InitialzeEquipSlots();
	}
	RefreshLeftEquipmentSlots();
	RefreshLeftSkinSlots();
	RefreshLeftPandoraSlots();

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
			LeftEquipmentWidget->InitialzeEquipSlots();
			LeftEquipmentWidget->OnClicked_EquipTypeSlot.Clear();
			LeftEquipmentWidget->OnClicked_EquipTypeSlot.AddUniqueDynamic(this, &ThisClass::HandleClickedItemEquipTypeSlot);
			LeftEquipmentWidget->OnDroppedItem_EquipTypeSlot.Clear();
			LeftEquipmentWidget->OnDroppedItem_EquipTypeSlot.AddUniqueDynamic(this, &ThisClass::HandleDroppedItemEquipTypeSlot);
		}
		RefreshLeftEquipmentSlots();
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
			RightSkinWidget->ToggleActiveFiliterButtons(true);
			RightSkinWidget->SetTileView(CurrentSkinList);

			RightSkinWidget->OnClicked_SkinFilterAllButton.Clear();
			RightSkinWidget->OnClicked_SkinFilterAllButton.AddUniqueDynamic(this, &ThisClass::HandleClickedSkinFilterAllButton);

			RightSkinWidget->OnClicked_SkinFilterTypeButton.Clear();
			RightSkinWidget->OnClicked_SkinFilterTypeButton.AddUniqueDynamic(this, &ThisClass::HandleClickedSkinFilterTypeButton);
		}

		if (ULeftSkinWidget* LeftSkinWidget = CurrentInfoWidget->GetLeftSkinWidget())
		{
			LeftSkinWidget->InitialzeEquipSlots();
			LeftSkinWidget->OnClicked_SkinEquipTypeSlot.Clear();
			LeftSkinWidget->OnClicked_SkinEquipTypeSlot.AddUniqueDynamic(this, &ThisClass::HandleClickedSkinEquipTypeSlot);
			LeftSkinWidget->OnDroppedSkin_SkinEquipTypeSlot.Clear();
			LeftSkinWidget->OnDroppedSkin_SkinEquipTypeSlot.AddUniqueDynamic(this, &ThisClass::HandleDroppedSkinEquipTypeSlot);
		}
		RefreshLeftSkinSlots();
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
		RefreshLeftPandoraSlots();
	}
}

void UInfoUiPresenter::HandleClickedItemSlot(UObject* Item)
{
	APdPlayerController* Controller = GetController();
	UInventorySlotViewData* SlotViewData = Cast<UInventorySlotViewData>(Item);
	UItemInstance* ItemInstance = Cast<UItemInstance>(Item);
	if (!ItemInstance && SlotViewData)
	{
		ItemInstance = SlotViewData->GetItemInstance();
	}
	UE_LOG(LogInfoUiPresenter, Log,
		TEXT("[EquipSlotFlow] Inventory item clicked: controller=%s rawItem=%s slotIndex=%d itemInstance=%s cachedSlot=%s cachedSlotNth=%d cachedSlotType=%s currentFilter=%s filterEnabled=%s"),
		*GetNameSafe(Controller),
		*GetNameSafe(Item),
		SlotViewData ? SlotViewData->GetSlotIndex() : INDEX_NONE,
		*GetNameSafe(ItemInstance),
		*GetNameSafe(CachedSelectedEquipSlot),
		CachedSelectedEquipSlot ? CachedSelectedEquipSlot->GetNth() : INDEX_NONE,
		CachedSelectedEquipTypeTag.IsValid() ? *CachedSelectedEquipTypeTag.ToString() : TEXT("None"),
		CurrentItemFilterTag.IsValid() ? *CurrentItemFilterTag.ToString() : TEXT("None"),
		bUseItemTypeFilter ? TEXT("true") : TEXT("false"));
	if (!Controller || !ItemInstance)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("Item slot click skipped: controller=%s item=%s itemInstance=%s"),
			*GetNameSafe(Controller),
			*GetNameSafe(Item),
			*GetNameSafe(ItemInstance));
		return;
	}

	const UItemDefinition* ItemDefinition = ItemInstance->ItemDefinition;
	UE_LOG(LogInfoUiPresenter, Log,
		TEXT("[EquipSlotFlow] Item definition resolved: item=%s definition=%s idTag=%s displayName=%s iconPath=%s"),
		*GetNameSafe(ItemInstance),
		*GetNameSafe(ItemDefinition),
		ItemDefinition && ItemDefinition->IdTag.IsValid() ? *ItemDefinition->IdTag.ToString() : TEXT("None"),
		ItemDefinition ? *ItemDefinition->DisplayName.ToString() : TEXT("None"),
		ItemDefinition ? *ItemDefinition->IconTexture.ToSoftObjectPath().ToString() : TEXT("None"));
	if (!ItemDefinition || !ItemDefinition->IdTag.IsValid())
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("Item slot click skipped: item=%s definition=%s idTag=%s"),
			*GetNameSafe(ItemInstance),
			*GetNameSafe(ItemDefinition),
			ItemDefinition ? *ItemDefinition->IdTag.ToString() : TEXT("None"));
		return;
	}

	if (!CachedSelectedEquipSlot || !CachedSelectedEquipTypeTag.IsValid())
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("Item slot click skipped: no selected equipment slot. item=%s definition=%s idTag=%s selectedSlot=%s selectedType=%s"),
			*GetNameSafe(ItemInstance),
			*GetNameSafe(ItemDefinition),
			*ItemDefinition->IdTag.ToString(),
			*GetNameSafe(CachedSelectedEquipSlot),
			CachedSelectedEquipTypeTag.IsValid() ? *CachedSelectedEquipTypeTag.ToString() : TEXT("None"));
		return;
	}

	const bool bMatchesSelectedSlotType = ItemDefinition->IdTag.MatchesTag(CachedSelectedEquipTypeTag);
	UE_LOG(LogInfoUiPresenter, Log,
		TEXT("[EquipSlotFlow] Type check: itemTag=%s selectedSlotType=%s matches=%s slot=%s nth=%d"),
		*ItemDefinition->IdTag.ToString(),
		*CachedSelectedEquipTypeTag.ToString(),
		bMatchesSelectedSlotType ? TEXT("true") : TEXT("false"),
		*GetNameSafe(CachedSelectedEquipSlot),
		CachedSelectedEquipSlot->GetNth());
	if (!bMatchesSelectedSlotType)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("Item slot click ignored by selected slot type: item=%s definition=%s idTag=%s selectedType=%s slot=%s nth=%d"),
			*GetNameSafe(ItemInstance),
			*GetNameSafe(ItemDefinition),
			*ItemDefinition->IdTag.ToString(),
			*CachedSelectedEquipTypeTag.ToString(),
			*GetNameSafe(CachedSelectedEquipSlot),
			CachedSelectedEquipSlot->GetNth());
		return;
	}

	UE_LOG(LogInfoUiPresenter, Log,
		TEXT("[EquipSlotFlow] Applying item to selected UI slot: slot=%s nth=%d item=%s definition=%s"),
		*GetNameSafe(CachedSelectedEquipSlot),
		CachedSelectedEquipSlot->GetNth(),
		*GetNameSafe(ItemInstance),
		*GetNameSafe(ItemDefinition));
	CachedSelectedEquipSlot->SetData(ItemInstance);

	if (UInfoWidget* CurrentInfoWidget = GetInfoWidget())
	{
		if (URightInventoryWidget* RightInventoryWidget = CurrentInfoWidget->GetRightInventoryWidget())
		{
			RightInventoryWidget->ClearTileViewItemClicked();
		}
	}

	UE_LOG(LogInfoUiPresenter, Log, TEXT("Item equipped to UI slot: slot=%s nth=%d selectedType=%s item=%s definition=%s idTag=%s"),
		*GetNameSafe(CachedSelectedEquipSlot),
		CachedSelectedEquipSlot->GetNth(),
		*CachedSelectedEquipTypeTag.ToString(),
		*GetNameSafe(ItemInstance),
		*GetNameSafe(ItemDefinition),
		*ItemDefinition->IdTag.ToString());

	const FGameplayTag WeaponItemTypeTag = GetWeaponItemTypeTag();
	const bool bSelectedWeaponSlot = WeaponItemTypeTag.IsValid() && CachedSelectedEquipTypeTag.MatchesTag(WeaponItemTypeTag);
	UE_LOG(LogInfoUiPresenter, Log,
		TEXT("[EquipSlotFlow] Weapon branch check: selectedType=%s weaponRoot=%s isWeaponSlot=%s"),
		*CachedSelectedEquipTypeTag.ToString(),
		WeaponItemTypeTag.IsValid() ? *WeaponItemTypeTag.ToString() : TEXT("None"),
		bSelectedWeaponSlot ? TEXT("true") : TEXT("false"));
	if (!bSelectedWeaponSlot)
	{
		return;
	}

	UTexture2D* WeaponTexture = ItemDefinition->IconTexture.LoadSynchronous();
	UE_LOG(LogInfoUiPresenter, Log,
		TEXT("[EquipSlotFlow] Weapon icon loaded for SelectPandora: definition=%s icon=%s path=%s"),
		*GetNameSafe(ItemDefinition),
		*GetNameSafe(WeaponTexture),
		*ItemDefinition->IconTexture.ToSoftObjectPath().ToString());
	if (!WeaponTexture)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("Weapon slot selection skipped: missing icon texture item=%s definition=%s"),
			*GetNameSafe(ItemInstance),
			*GetNameSafe(ItemDefinition));
		return;
	}

	const int32 Nth = CachedSelectedEquipSlot->GetNth();
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
	RefreshLeftEquipmentSlots();
	RefreshSelectPandoraCompatibilityState();
}

void UInfoUiPresenter::HandleClickedItemEquipTypeSlot(FGameplayTag EquipTypeTag, UEquipSlotWidget* SelectedEquipSlot, bool bIsSelectedAnyButton)
{
	(void)bIsSelectedAnyButton;

	APdPlayerController* Controller = GetController();
	if (!Controller)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[InventoryFilter] EquipSlot click skipped: controller is null. tag=%s slot=%s selectedAny=%s"),
			*EquipTypeTag.ToString(),
			*GetNameSafe(SelectedEquipSlot),
			bIsSelectedAnyButton ? TEXT("true") : TEXT("false"));
		return;
	}

	ClearInventoryClickEquipBinding();
	CachedSelectedEquipSlot = nullptr;
	CachedSelectedEquipTypeTag = FGameplayTag();

	UE_LOG(LogInfoUiPresenter, Log, TEXT("[EquipSlotFlow] EquipSlot clicked: controller=%s tag=%s slot=%s slotNth=%d occupied=%s slotOwnTag=%s"),
		*GetNameSafe(Controller),
		*EquipTypeTag.ToString(),
		*GetNameSafe(SelectedEquipSlot),
		SelectedEquipSlot ? SelectedEquipSlot->GetNth() : INDEX_NONE,
		SelectedEquipSlot && SelectedEquipSlot->HasEquippedItem() ? TEXT("true") : TEXT("false"),
		SelectedEquipSlot && SelectedEquipSlot->GetEquipTypeTag().IsValid() ? *SelectedEquipSlot->GetEquipTypeTag().ToString() : TEXT("None"));

	if (!SelectedEquipSlot || !EquipTypeTag.IsValid())
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[InventoryFilter] EquipSlot click skipped: invalid slot or tag. slot=%s tag=%s"),
			*GetNameSafe(SelectedEquipSlot),
			EquipTypeTag.IsValid() ? *EquipTypeTag.ToString() : TEXT("None"));
		return;
	}

	if (SelectedEquipSlot->HasEquippedItem())
	{
		ClearEquipmentSlot(SelectedEquipSlot, EquipTypeTag);
		return;
	}

	HandleClickedItemFilterTypeButton(EquipTypeTag);
	UE_LOG(LogInfoUiPresenter, Log, TEXT("[InventoryFilter] Empty EquipSlot click applied filter only: slot=%s nth=%d tag=%s"),
		*GetNameSafe(SelectedEquipSlot),
		SelectedEquipSlot->GetNth(),
		*EquipTypeTag.ToString());
}

void UInfoUiPresenter::HandleDroppedItemEquipTypeSlot(FGameplayTag EquipTypeTag, UEquipSlotWidget* TargetEquipSlot, UItemInstance* ItemInstance)
{
	UE_LOG(LogInfoUiPresenter, Log, TEXT("[EquipSlotDragDrop] Presenter received item drop: tag=%s targetSlot=%s nth=%d item=%s"),
		EquipTypeTag.IsValid() ? *EquipTypeTag.ToString() : TEXT("None"),
		*GetNameSafe(TargetEquipSlot),
		TargetEquipSlot ? TargetEquipSlot->GetNth() : INDEX_NONE,
		*GetNameSafe(ItemInstance));

	CachedSelectedEquipSlot = TargetEquipSlot;
	CachedSelectedEquipTypeTag = EquipTypeTag;
	HandleClickedItemSlot(ItemInstance);
}

void UInfoUiPresenter::HandleDroppedItemToCharacterPanel(UItemInstance* ItemInstance)
{
	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	ULeftEquipmentWidget* LeftEquipmentWidget = CurrentInfoWidget ? CurrentInfoWidget->GetLeftEquipmentWidget() : nullptr;
	UEquipSlotWidget* TargetEquipSlot = LeftEquipmentWidget ? LeftEquipmentWidget->FindFirstCompatibleEquipSlot(ItemInstance) : nullptr;
	if (!TargetEquipSlot)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[CharacterPanelDrop] Item drop skipped: no compatible equipment slot. info=%s leftEquipment=%s item=%s definition=%s"),
			*GetNameSafe(CurrentInfoWidget),
			*GetNameSafe(LeftEquipmentWidget),
			*GetNameSafe(ItemInstance),
			*GetNameSafe(ItemInstance ? ItemInstance->ItemDefinition.Get() : nullptr));
		return;
	}

	const FGameplayTag TargetTag = TargetEquipSlot->GetAcceptedEquipTypeTag();
	UE_LOG(LogInfoUiPresenter, Log, TEXT("[CharacterPanelDrop] Item auto-routed to equipment slot: item=%s slot=%s nth=%d tag=%s"),
		*GetNameSafe(ItemInstance),
		*GetNameSafe(TargetEquipSlot),
		TargetEquipSlot->GetNth(),
		TargetTag.IsValid() ? *TargetTag.ToString() : TEXT("None"));

	CachedSelectedEquipSlot = TargetEquipSlot;
	CachedSelectedEquipTypeTag = TargetTag;
	HandleClickedItemSlot(ItemInstance);
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
	USkinSlotViewData* SlotViewData = Cast<USkinSlotViewData>(Item);
	USkinInstance* SkinInstance = Cast<USkinInstance>(Item);
	if (!SkinInstance && SlotViewData)
	{
		SkinInstance = SlotViewData->GetSkinInstance();
	}
	UE_LOG(LogInfoUiPresenter, Log,
		TEXT("[SkinSlotFlow] Skin inventory item clicked: controller=%s rawItem=%s slotIndex=%d skinInstance=%s cachedSlot=%s cachedSlotType=%s"),
		*GetNameSafe(Controller),
		*GetNameSafe(Item),
		SlotViewData ? SlotViewData->GetSlotIndex() : INDEX_NONE,
		*GetNameSafe(SkinInstance),
		*GetNameSafe(CachedSelectedSkinEquipSlot),
		CachedSelectedSkinEquipTypeTag.IsValid() ? *CachedSelectedSkinEquipTypeTag.ToString() : TEXT("None"));
	if (!Controller || !SkinInstance)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[SkinSlotFlow] Skin slot click skipped: controller=%s item=%s skinInstance=%s"),
			*GetNameSafe(Controller),
			*GetNameSafe(Item),
			*GetNameSafe(SkinInstance));
		return;
	}

	const USkinDefinition* SkinDefinition = SkinInstance->SkinDefinition.Get();
	UE_LOG(LogInfoUiPresenter, Log,
		TEXT("[SkinSlotFlow] Skin definition resolved: skin=%s definition=%s idTag=%s displayName=%s icon=%s"),
		*GetNameSafe(SkinInstance),
		*GetNameSafe(SkinDefinition),
		SkinDefinition && SkinDefinition->IdTag.IsValid() ? *SkinDefinition->IdTag.ToString() : TEXT("None"),
		SkinDefinition ? *SkinDefinition->DisplayName.ToString() : TEXT("None"),
		SkinDefinition ? *GetNameSafe(SkinDefinition->IconTexture) : TEXT("None"));
	if (!SkinDefinition || !SkinDefinition->IdTag.IsValid())
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[SkinSlotFlow] Skin slot click skipped: invalid definition. skin=%s definition=%s idTag=%s"),
			*GetNameSafe(SkinInstance),
			*GetNameSafe(SkinDefinition),
			SkinDefinition ? *SkinDefinition->IdTag.ToString() : TEXT("None"));
		return;
	}

	if (!CachedSelectedSkinEquipSlot || !CachedSelectedSkinEquipTypeTag.IsValid())
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[SkinSlotFlow] Skin slot click skipped: no selected skin equipment slot. skin=%s definition=%s selectedSlot=%s selectedType=%s"),
			*GetNameSafe(SkinInstance),
			*GetNameSafe(SkinDefinition),
			*GetNameSafe(CachedSelectedSkinEquipSlot),
			CachedSelectedSkinEquipTypeTag.IsValid() ? *CachedSelectedSkinEquipTypeTag.ToString() : TEXT("None"));
		return;
	}

	const bool bMatchesSelectedSlotType = SkinDefinition->IdTag.MatchesTag(CachedSelectedSkinEquipTypeTag);
	UE_LOG(LogInfoUiPresenter, Log,
		TEXT("[SkinSlotFlow] Type check: skinTag=%s selectedSlotType=%s matches=%s slot=%s"),
		*SkinDefinition->IdTag.ToString(),
		*CachedSelectedSkinEquipTypeTag.ToString(),
		bMatchesSelectedSlotType ? TEXT("true") : TEXT("false"),
		*GetNameSafe(CachedSelectedSkinEquipSlot));
	if (!bMatchesSelectedSlotType)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[SkinSlotFlow] Skin slot click ignored by selected slot type: skin=%s definition=%s idTag=%s selectedType=%s slot=%s"),
			*GetNameSafe(SkinInstance),
			*GetNameSafe(SkinDefinition),
			*SkinDefinition->IdTag.ToString(),
			*CachedSelectedSkinEquipTypeTag.ToString(),
			*GetNameSafe(CachedSelectedSkinEquipSlot));
		return;
	}

	APdPlayer* PlayerCharacter = Cast<APdPlayer>(Controller->GetPawn());
	USkinEquipmentComponent* SkinEquipmentComponent = PlayerCharacter ? PlayerCharacter->GetSkinEquipmentComponent() : nullptr;
	UE_LOG(LogInfoUiPresenter, Log, TEXT("[SkinSlotFlow] Applying skin to selected UI slot before equip request: slot=%s slotType=%s skin=%s definition=%s component=%s ownerHasAuthority=%s"),
		*GetNameSafe(CachedSelectedSkinEquipSlot),
		*CachedSelectedSkinEquipTypeTag.ToString(),
		*GetNameSafe(SkinInstance),
		*GetNameSafe(SkinDefinition),
		*GetNameSafe(SkinEquipmentComponent),
		SkinEquipmentComponent && SkinEquipmentComponent->GetOwner() && SkinEquipmentComponent->GetOwner()->HasAuthority() ? TEXT("true") : TEXT("false"));
	CachedSelectedSkinEquipSlot->SetData(SkinInstance);

	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	if (URightSkinWidget* RightSkinWidget = CurrentInfoWidget ? CurrentInfoWidget->GetRightSkinWidget() : nullptr)
	{
		RightSkinWidget->ClearTileViewItemClicked();
	}

	if (SkinEquipmentComponent)
	{
		const bool bRequested = SkinEquipmentComponent->RequestEquipSkin(SkinInstance, CachedSelectedSkinEquipTypeTag);
		UE_LOG(LogInfoUiPresenter, Log, TEXT("[SkinSlotFlow] Skin equip requested: slot=%s skin=%s definition=%s result=%s ownerHasAuthority=%s"),
			*CachedSelectedSkinEquipTypeTag.ToString(),
			*GetNameSafe(SkinInstance),
			*GetNameSafe(SkinDefinition),
			bRequested ? TEXT("true") : TEXT("false"),
			SkinEquipmentComponent->GetOwner() && SkinEquipmentComponent->GetOwner()->HasAuthority() ? TEXT("true") : TEXT("false"));

		if (!bRequested || (SkinEquipmentComponent->GetOwner() && SkinEquipmentComponent->GetOwner()->HasAuthority()))
		{
			UE_LOG(LogInfoUiPresenter, Log, TEXT("[SkinSlotFlow] Refreshing left skin slots after request: requested=%s authority=%s"),
				bRequested ? TEXT("true") : TEXT("false"),
				SkinEquipmentComponent->GetOwner() && SkinEquipmentComponent->GetOwner()->HasAuthority() ? TEXT("true") : TEXT("false"));
			RefreshLeftSkinSlots();
		}
		else
		{
			UE_LOG(LogInfoUiPresenter, Log, TEXT("[SkinSlotFlow] Keeping immediate client skin slot icon until server replication updates equipped skins."));
		}
	}
	else
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[SkinSlotFlow] Skin equip request skipped: SkinEquipmentComponent is null. pawn=%s"),
			*GetNameSafe(PlayerCharacter));
	}
}

void UInfoUiPresenter::HandleClickedSkinEquipTypeSlot(FGameplayTag EquipTypeTag, USkinEquipSlotWidget* SelectedEquipSlot, bool bIsSelectedAnyButton)
{
	(void)bIsSelectedAnyButton;

	APdPlayerController* Controller = GetController();
	if (!Controller)
	{
		return;
	}

	ClearSkinClickEquipBinding();
	CachedSelectedSkinEquipSlot = nullptr;
	CachedSelectedSkinEquipTypeTag = FGameplayTag();

	UE_LOG(LogInfoUiPresenter, Log, TEXT("[SkinSlotFlow] SkinEquipSlot clicked: controller=%s tag=%s slot=%s occupied=%s slotOwnTag=%s"),
		*GetNameSafe(Controller),
		EquipTypeTag.IsValid() ? *EquipTypeTag.ToString() : TEXT("None"),
		*GetNameSafe(SelectedEquipSlot),
		SelectedEquipSlot && SelectedEquipSlot->HasEquippedSkin() ? TEXT("true") : TEXT("false"),
		SelectedEquipSlot && SelectedEquipSlot->GetEquipTypeTag().IsValid() ? *SelectedEquipSlot->GetEquipTypeTag().ToString() : TEXT("None"));

	if (!SelectedEquipSlot || !EquipTypeTag.IsValid())
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[SkinSlotFlow] SkinEquipSlot click skipped: invalid slot or tag. slot=%s tag=%s"),
			*GetNameSafe(SelectedEquipSlot),
			EquipTypeTag.IsValid() ? *EquipTypeTag.ToString() : TEXT("None"));
		return;
	}

	if (SelectedEquipSlot->HasEquippedSkin())
	{
		ClearSkinEquipSlot(SelectedEquipSlot, EquipTypeTag);
		return;
	}

	HandleClickedSkinFilterTypeButton(EquipTypeTag);
	UE_LOG(LogInfoUiPresenter, Log, TEXT("[SkinSlotFlow] Empty SkinEquipSlot click applied filter only: slot=%s tag=%s"),
		*GetNameSafe(SelectedEquipSlot),
		*EquipTypeTag.ToString());
}

void UInfoUiPresenter::HandleDroppedSkinEquipTypeSlot(FGameplayTag EquipTypeTag, USkinEquipSlotWidget* TargetSkinEquipSlot, USkinInstance* SkinInstance)
{
	UE_LOG(LogInfoUiPresenter, Log, TEXT("[SkinSlotDragDrop] Presenter received skin drop: tag=%s targetSlot=%s skin=%s definition=%s"),
		EquipTypeTag.IsValid() ? *EquipTypeTag.ToString() : TEXT("None"),
		*GetNameSafe(TargetSkinEquipSlot),
		*GetNameSafe(SkinInstance),
		*GetNameSafe(SkinInstance ? SkinInstance->SkinDefinition.Get() : nullptr));

	CachedSelectedSkinEquipSlot = TargetSkinEquipSlot;
	CachedSelectedSkinEquipTypeTag = EquipTypeTag;
	HandleClickedSkinSlot(SkinInstance);
}

void UInfoUiPresenter::HandleDroppedSkinToCharacterPanel(USkinInstance* SkinInstance)
{
	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	ULeftSkinWidget* LeftSkinWidget = CurrentInfoWidget ? CurrentInfoWidget->GetLeftSkinWidget() : nullptr;
	USkinEquipSlotWidget* TargetSkinEquipSlot = LeftSkinWidget ? LeftSkinWidget->FindFirstCompatibleSkinEquipSlot(SkinInstance) : nullptr;
	if (!TargetSkinEquipSlot)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[CharacterPanelDrop] Skin drop skipped: no compatible skin slot. info=%s leftSkin=%s skin=%s definition=%s"),
			*GetNameSafe(CurrentInfoWidget),
			*GetNameSafe(LeftSkinWidget),
			*GetNameSafe(SkinInstance),
			*GetNameSafe(SkinInstance ? SkinInstance->SkinDefinition.Get() : nullptr));
		return;
	}

	const FGameplayTag TargetTag = TargetSkinEquipSlot->GetAcceptedEquipTypeTag();
	UE_LOG(LogInfoUiPresenter, Log, TEXT("[CharacterPanelDrop] Skin auto-routed to skin slot: skin=%s slot=%s tag=%s"),
		*GetNameSafe(SkinInstance),
		*GetNameSafe(TargetSkinEquipSlot),
		TargetTag.IsValid() ? *TargetTag.ToString() : TEXT("None"));

	CachedSelectedSkinEquipSlot = TargetSkinEquipSlot;
	CachedSelectedSkinEquipTypeTag = TargetTag;
	HandleClickedSkinSlot(SkinInstance);
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
		else if (TypeTag.IsValid())
		{
			for (USkinInstance* SkinInstance : SkinComponent->AllSkinList.Skins)
			{
				const USkinDefinition* SkinDefinition = IsValid(SkinInstance) ? SkinInstance->SkinDefinition.Get() : nullptr;
				if (SkinDefinition && SkinDefinition->IdTag.MatchesTag(TypeTag))
				{
					CurrentSkinList.Add(SkinInstance);
				}
			}
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
	UE_LOG(LogInfoUiPresenter, Log,
		TEXT("[PandoraLoadoutFlow] Pandora inventory item clicked: controller=%s rawItem=%s pandoraInstance=%s owned=%s cachedSlot=%s cachedSlotNth=%d"),
		*GetNameSafe(Controller),
		*GetNameSafe(Item),
		*GetNameSafe(PandoraInstance),
		PandoraInstance && PandoraInstance->IsOwned ? TEXT("true") : TEXT("false"),
		*GetNameSafe(CachedSelectedPandoraEquipSlot),
		CachedSelectedPandoraEquipSlot ? CachedSelectedPandoraEquipSlot->GetNth() : INDEX_NONE);
	if (!Controller || !PandoraInstance)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[PandoraLoadoutFlow] Pandora slot click skipped: controller=%s item=%s pandoraInstance=%s"),
			*GetNameSafe(Controller),
			*GetNameSafe(Item),
			*GetNameSafe(PandoraInstance));
		return;
	}

	if (!CachedSelectedPandoraEquipSlot)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[PandoraLoadoutFlow] Pandora slot click skipped: no selected pandora equip slot. pandora=%s"),
			*GetNameSafe(PandoraInstance));
		return;
	}

	if (!PandoraInstance->IsOwned)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[PandoraLoadoutFlow] Pandora slot click skipped: pandora is not owned. pandora=%s definition=%s"),
			*GetNameSafe(PandoraInstance),
			*GetNameSafe(PandoraInstance->PandoraDefinition.Get()));
		return;
	}

	const UPandoraDefinition* PandoraDefinition = PandoraInstance->PandoraDefinition;
	if (!PandoraDefinition)
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[PandoraLoadoutFlow] Pandora slot click skipped: missing definition. pandora=%s"),
			*GetNameSafe(PandoraInstance));
		return;
	}

	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	if (URightPandoraWidget* RightPandoraWidget = CurrentInfoWidget ? CurrentInfoWidget->GetRightPandoraWidget() : nullptr)
	{
		RightPandoraWidget->ClearTileViewItemClicked();
	}

	const int32 Nth = CachedSelectedPandoraEquipSlot->GetNth();
	const EEnum_Direction Direction = FPandoraLoadoutUiModel::GetDirectionFromSelectSlotNumber(Nth);

	APdPlayerState* PlayerState = GetCachedPlayerState();
	UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	UE_LOG(LogInfoUiPresenter, Log, TEXT("[PandoraLoadoutFlow] Requesting pandora loadout slot: slot=%d direction=%d pandora=%s component=%s authority=%s"),
		Nth,
		static_cast<int32>(Direction),
		*GetNameSafe(PandoraDefinition),
		*GetNameSafe(PandoraComponent),
		PandoraComponent && PandoraComponent->GetOwner() && PandoraComponent->GetOwner()->HasAuthority() ? TEXT("true") : TEXT("false"));
	if (PandoraComponent)
	{
		const bool bRequested = PandoraComponent->RequestSetPandoraLoadoutSlot(Direction, PandoraDefinition);
		UE_LOG(LogInfoUiPresenter, Log, TEXT("[PandoraLoadoutFlow] Pandora loadout request result: slot=%d direction=%d pandora=%s requested=%s"),
			Nth,
			static_cast<int32>(Direction),
			*GetNameSafe(PandoraDefinition),
			bRequested ? TEXT("true") : TEXT("false"));

		if (!bRequested)
		{
			RefreshLeftPandoraSlots();
			RefreshSelectPandoraLoadoutImages();
			RefreshSelectPandoraCompatibilityState();
		}
	}
	else
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[PandoraLoadoutFlow] Pandora loadout request skipped: PandoraComponent is null."));
	}
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
		UE_LOG(LogInfoUiPresenter, Log, TEXT("[PandoraLoadoutFlow] Pandora equip slot deselect: clearing cached slot and tile binding. previousSlot=%s"),
			*GetNameSafe(CachedSelectedPandoraEquipSlot));
		CachedSelectedPandoraEquipSlot = nullptr;

		if (UTileView* TileView = RightPandoraWidget->GetTileView())
		{
			TileView->OnItemClicked().RemoveAll(this);
		}
		return;
	}

	if (UTileView* TileView = RightPandoraWidget->GetTileView())
	{
		TileView->OnItemClicked().RemoveAll(this);
		TileView->OnItemClicked().AddUObject(this, &ThisClass::HandleClickedPandoraSlot);
		UE_LOG(LogInfoUiPresenter, Log, TEXT("[PandoraLoadoutFlow] Tile item click rebound for pandora loadout selection: tileView=%s selectedSlot=%s selectedNth=%d"),
			*GetNameSafe(TileView),
			*GetNameSafe(CachedSelectedPandoraEquipSlot),
			CachedSelectedPandoraEquipSlot ? CachedSelectedPandoraEquipSlot->GetNth() : INDEX_NONE);
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

void UInfoUiPresenter::ClearInventoryClickEquipBinding() const
{
	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	URightInventoryWidget* RightInventoryWidget = CurrentInfoWidget ? CurrentInfoWidget->GetRightInventoryWidget() : nullptr;
	if (UTileView* TileView = RightInventoryWidget ? RightInventoryWidget->GetTileView() : nullptr)
	{
		TileView->OnItemClicked().RemoveAll(this);
	}
}

void UInfoUiPresenter::ClearSkinClickEquipBinding() const
{
	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	URightSkinWidget* RightSkinWidget = CurrentInfoWidget ? CurrentInfoWidget->GetRightSkinWidget() : nullptr;
	if (UTileView* TileView = RightSkinWidget ? RightSkinWidget->GetTileView() : nullptr)
	{
		TileView->OnItemClicked().RemoveAll(this);
	}
}

void UInfoUiPresenter::ClearEquipmentSlot(UEquipSlotWidget* TargetEquipSlot, FGameplayTag EquipTypeTag)
{
	if (!TargetEquipSlot)
	{
		return;
	}

	if (!EquipTypeTag.IsValid())
	{
		EquipTypeTag = TargetEquipSlot->GetAcceptedEquipTypeTag();
	}

	UE_LOG(LogInfoUiPresenter, Log, TEXT("[EquipSlotFlow] Clearing equipment slot: slot=%s nth=%d tag=%s item=%s"),
		*GetNameSafe(TargetEquipSlot),
		TargetEquipSlot->GetNth(),
		EquipTypeTag.IsValid() ? *EquipTypeTag.ToString() : TEXT("None"),
		*GetNameSafe(TargetEquipSlot->GetItemInstance()));

	TargetEquipSlot->SetData(nullptr);
	if (CachedSelectedEquipSlot == TargetEquipSlot)
	{
		CachedSelectedEquipSlot = nullptr;
		CachedSelectedEquipTypeTag = FGameplayTag();
	}

	const FGameplayTag WeaponItemTypeTag = GetWeaponItemTypeTag();
	if (!WeaponItemTypeTag.IsValid() || !EquipTypeTag.MatchesTag(WeaponItemTypeTag))
	{
		return;
	}

	const int32 Nth = TargetEquipSlot->GetNth();
	switch (Nth)
	{
	case 1:
		CachedFirstWeapon = nullptr;
		break;
	case 2:
		CachedSecondWeapon = nullptr;
		break;
	case 3:
		CachedThirdWeapon = nullptr;
		break;
	default:
		break;
	}

	if (USelectPandoraWidget* SelectPandoraWidget = GetSelectPandoraWidget())
	{
		SelectPandoraWidget->SetWeaponImage(Nth, nullptr);
	}

	RefreshSelectPandoraCompatibilityState();
}

void UInfoUiPresenter::ClearSkinEquipSlot(USkinEquipSlotWidget* TargetSkinEquipSlot, FGameplayTag EquipTypeTag)
{
	if (!TargetSkinEquipSlot)
	{
		return;
	}

	if (!EquipTypeTag.IsValid())
	{
		EquipTypeTag = TargetSkinEquipSlot->GetAcceptedEquipTypeTag();
	}

	UE_LOG(LogInfoUiPresenter, Log, TEXT("[SkinSlotFlow] Clearing skin equipment slot: slot=%s tag=%s definition=%s skin=%s"),
		*GetNameSafe(TargetSkinEquipSlot),
		EquipTypeTag.IsValid() ? *EquipTypeTag.ToString() : TEXT("None"),
		*GetNameSafe(TargetSkinEquipSlot->GetSkinDefinition()),
		*GetNameSafe(TargetSkinEquipSlot->GetSkinInstance()));

	TargetSkinEquipSlot->SetSkinDefinition(nullptr);
	if (CachedSelectedSkinEquipSlot == TargetSkinEquipSlot)
	{
		CachedSelectedSkinEquipSlot = nullptr;
		CachedSelectedSkinEquipTypeTag = FGameplayTag();
	}

	APdPlayerController* Controller = GetController();
	APdPlayer* PlayerCharacter = Controller ? Cast<APdPlayer>(Controller->GetPawn()) : nullptr;
	USkinEquipmentComponent* SkinEquipmentComponent = PlayerCharacter ? PlayerCharacter->GetSkinEquipmentComponent() : nullptr;
	if (!SkinEquipmentComponent || !EquipTypeTag.IsValid())
	{
		UE_LOG(LogInfoUiPresenter, Warning, TEXT("[SkinSlotFlow] Skin unequip request skipped: component=%s tag=%s"),
			*GetNameSafe(SkinEquipmentComponent),
			EquipTypeTag.IsValid() ? *EquipTypeTag.ToString() : TEXT("None"));
		return;
	}

	const bool bRequested = SkinEquipmentComponent->RequestUnequipSkinSlot(EquipTypeTag);
	const bool bAuthority = SkinEquipmentComponent->GetOwner() && SkinEquipmentComponent->GetOwner()->HasAuthority();
	UE_LOG(LogInfoUiPresenter, Log, TEXT("[SkinSlotFlow] Skin unequip requested: slot=%s requested=%s authority=%s"),
		*EquipTypeTag.ToString(),
		bRequested ? TEXT("true") : TEXT("false"),
		bAuthority ? TEXT("true") : TEXT("false"));

	if (!bRequested || bAuthority)
	{
		RefreshLeftSkinSlots();
	}
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

void UInfoUiPresenter::RefreshLeftEquipmentSlots() const
{
	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	ULeftEquipmentWidget* LeftEquipmentWidget = CurrentInfoWidget ? CurrentInfoWidget->GetLeftEquipmentWidget() : nullptr;
	if (!LeftEquipmentWidget)
	{
		return;
	}

	LeftEquipmentWidget->SetWeaponSlotData(1, CachedFirstWeapon);
	LeftEquipmentWidget->SetWeaponSlotData(2, CachedSecondWeapon);
	LeftEquipmentWidget->SetWeaponSlotData(3, CachedThirdWeapon);
}

void UInfoUiPresenter::RefreshLeftSkinSlots() const
{
	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	ULeftSkinWidget* LeftSkinWidget = CurrentInfoWidget ? CurrentInfoWidget->GetLeftSkinWidget() : nullptr;
	if (!LeftSkinWidget)
	{
		return;
	}

	const APdPlayerController* Controller = GetController();
	const APdPlayer* PlayerCharacter = Controller ? Cast<APdPlayer>(Controller->GetPawn()) : nullptr;
	const USkinEquipmentComponent* SkinEquipmentComponent = PlayerCharacter ? PlayerCharacter->GetSkinEquipmentComponent() : nullptr;
	LeftSkinWidget->RefreshEquippedSkinSlots(SkinEquipmentComponent);
}

void UInfoUiPresenter::RefreshLeftPandoraSlots() const
{
	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	ULeftPandoraWidget* LeftPandoraWidget = CurrentInfoWidget ? CurrentInfoWidget->GetLeftPandoraWidget() : nullptr;
	if (!LeftPandoraWidget)
	{
		return;
	}

	const APdPlayerState* PlayerState = GetCachedPlayerState();
	const UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	LeftPandoraWidget->RefreshPandoraLoadoutSlots(PandoraComponent);
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

void UInfoUiPresenter::BindPandoraLoadoutChangeNotification()
{
	APdPlayerState* PdPlayerState = GetCachedPlayerState();
	UPandoraComponent* PandoraComponent = PdPlayerState ? PdPlayerState->GetPandoraComponent() : nullptr;
	if (BoundPandoraComponent == PandoraComponent)
	{
		return;
	}

	UnbindPandoraLoadoutChangeNotification();

	BoundPandoraComponent = PandoraComponent;
	if (BoundPandoraComponent)
	{
		BoundPandoraComponent->OnPandoraLoadoutChanged.AddUniqueDynamic(this, &ThisClass::HandlePandoraLoadoutChanged);
		UE_LOG(LogInfoUiPresenter, Log, TEXT("[PandoraLoadoutFlow] Bound pandora loadout change notification: component=%s"),
			*GetNameSafe(BoundPandoraComponent));
	}
}

void UInfoUiPresenter::UnbindPandoraLoadoutChangeNotification()
{
	if (BoundPandoraComponent)
	{
		BoundPandoraComponent->OnPandoraLoadoutChanged.RemoveDynamic(this, &ThisClass::HandlePandoraLoadoutChanged);
		UE_LOG(LogInfoUiPresenter, Log, TEXT("[PandoraLoadoutFlow] Unbound pandora loadout change notification: component=%s"),
			*GetNameSafe(BoundPandoraComponent));
		BoundPandoraComponent = nullptr;
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

void UInfoUiPresenter::HandlePandoraLoadoutChanged()
{
	UE_LOG(LogInfoUiPresenter, Log, TEXT("[PandoraLoadoutFlow] Pandora loadout changed: refreshing left pandora slots and select UI images. component=%s"),
		*GetNameSafe(BoundPandoraComponent));
	RefreshLeftPandoraSlots();
	RefreshSelectPandoraLoadoutImages();
	RefreshSelectPandoraCompatibilityState();
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

				for (UItemInstance* ItemInstance : InventoryComponent->AllItemList.Items)
				{
					const UItemDefinition* ItemDefinition = IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
					if (ItemDefinition && ItemDefinition->IdTag.MatchesTag(CurrentItemFilterTag))
					{
						CurrentItemList.Add(ItemInstance);
					}
				}

				UE_LOG(LogInfoUiPresenter, Log, TEXT("[InventoryFilter] Type filter fallback matched: tag=%s objectCount=%d"),
					*CurrentItemFilterTag.ToString(),
					CurrentItemList.Num());
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
