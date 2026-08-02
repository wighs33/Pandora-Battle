#include "UI/InfoUiPresenter.h"

#include "Character/PdPlayer.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Common/LabGameplayTags.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Components/TileView.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Component/Item/InventoryComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Mode/PdHUD.h"
#include "Component/Player/EquipmentComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Pandora/PandoraInstance.h"
#include "Pandora/PandoraLoadoutTypes.h"
#include "Component/Player/StatUpgradeComponent.h"
#include "Component/Skin/SkinComponent.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Component/Skin/SkinEquipmentComponent.h"
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

FGameplayTag ResolveSkinDefinitionMatchTag(const FGameplayTag SlotOrFilterTag)
{
	return SlotOrFilterTag.MatchesTag(LabGameplayTags::Skin_Gesture)
		? LabGameplayTags::Skin_Gesture
		: SlotOrFilterTag;
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

FString GetItemDisplayNameForSort(const UItemInstance* ItemInstance)
{
	const UItemDefinition* ItemDefinition = IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
	if (IsValid(ItemDefinition) && !ItemDefinition->DisplayName.IsEmpty())
	{
		return ItemDefinition->DisplayName.ToString();
	}

	return IsValid(ItemDefinition) ? ItemDefinition->GetName() : GetNameSafe(ItemInstance);
}

void SortItemObjectsByDisplayName(TArray<UObject*>& InOutItems)
{
	InOutItems.StableSort([](const UObject& LeftObject, const UObject& RightObject)
	{
		const UItemInstance* LeftItem = Cast<UItemInstance>(&LeftObject);
		const UItemInstance* RightItem = Cast<UItemInstance>(&RightObject);
		const FString LeftName = GetItemDisplayNameForSort(LeftItem);
		const FString RightName = GetItemDisplayNameForSort(RightItem);

		const int32 NameCompare = LeftName.Compare(RightName, ESearchCase::IgnoreCase);
		if (NameCompare != 0)
		{
			return NameCompare < 0;
		}

		const UObject* LeftTieObject = LeftItem && LeftItem->ItemDefinition ? LeftItem->ItemDefinition.Get() : &LeftObject;
		const UObject* RightTieObject = RightItem && RightItem->ItemDefinition ? RightItem->ItemDefinition.Get() : &RightObject;
		return GetNameSafe(LeftTieObject) < GetNameSafe(RightTieObject);
	});
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
	UnbindInfoUiEvents();
	UnbindInventoryChangeNotification();
	ReleaseItemPresentationPreload();
	UnbindPandoraLoadoutChangeNotification();
	OwningController = nullptr;
	InfoWidget = nullptr;
	CachedSelectedEquipSlot = nullptr;
	CachedSelectedEquipTypeTag = FGameplayTag();
	CachedSelectedSkinEquipSlot = nullptr;
	CachedSelectedSkinEquipTypeTag = FGameplayTag();
	CachedSelectedPandoraEquipSlot = nullptr;
	CurrentPandoraFilterTag = FGameplayTag();
	bUsePandoraTypeFilter = false;
	bShowOnlyOwnedPandorasForEquipSlot = false;
	PendingClearedWeaponId.Invalidate();
	PendingClearedWeaponDirection = EEnum_Direction::Center;
	ResetInventoryDisplaySlots();
	CurrentLeftUiTag = FGameplayTag();
	CurrentItemFilterTag = FGameplayTag();
	bUseItemTypeFilter = false;
}

void UInfoUiPresenter::BindInfoUi(UInfoWidget* InInfoWidget)
{
	if (InfoWidget != InInfoWidget)
	{
		UnbindInfoUiEvents();
	}

	InfoWidget = InInfoWidget;
	if (InfoWidget)
	{
		InfoWidget->OnDroppedItemToCharacterPanel.RemoveDynamic(
			this,
			&ThisClass::HandleDroppedItemToCharacterPanel);
		InfoWidget->OnDroppedItemToCharacterPanel.AddUniqueDynamic(this, &ThisClass::HandleDroppedItemToCharacterPanel);
		InfoWidget->OnDroppedSkinToCharacterPanel.RemoveDynamic(
			this,
			&ThisClass::HandleDroppedSkinToCharacterPanel);
		InfoWidget->OnDroppedSkinToCharacterPanel.AddUniqueDynamic(this, &ThisClass::HandleDroppedSkinToCharacterPanel);
	}
	BindInventoryChangeNotification();
	BindPandoraLoadoutChangeNotification();
	BindStatusWidgetEvents();
	BeginItemPresentationPreload();
}

UItemInstance* UInfoUiPresenter::GetSelectedWeapon(EEnum_Direction Direction) const
{
	const APdPlayerState* PlayerState = GetCachedPlayerState();
	const UInventoryComponent* InventoryComponent = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	return InventoryComponent ? InventoryComponent->GetPandoraWeaponLoadoutItem(Direction) : nullptr;
}

UPandoraInstance* UInfoUiPresenter::GetSelectedPandora(EEnum_Direction Direction) const
{
	const APdPlayerState* PlayerState = GetCachedPlayerState();
	const UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	return PandoraComponent ? PandoraComponent->GetPandoraLoadoutInstance(Direction) : nullptr;
}

bool UInfoUiPresenter::WouldSelectedPandoraDirectionChangeLoadout(const EEnum_Direction Direction) const
{
	APdPlayerController* Controller = GetController();
	APdPlayer* PlayerCharacter = Controller ? Cast<APdPlayer>(Controller->GetPawn()) : nullptr;
	const UEquipmentComponent* EquipmentComponent = PlayerCharacter ? PlayerCharacter->GetEquipmentComponent() : nullptr;
	const APdPlayerState* PlayerState = GetCachedPlayerState();
	const UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;

	if (Direction == EEnum_Direction::Down)
	{
		const bool bWouldUnequipWeapon = EquipmentComponent
			&& (EquipmentComponent->GetCurrentWeaponId().IsValid() || EquipmentComponent->GetCurrentWeaponDefinition());
		const bool bWouldClearPandora = PandoraComponent && PandoraComponent->GetCurrentPandoraDefinition();
		return bWouldUnequipWeapon || bWouldClearPandora;
	}

	if (!PandoraLoadout::IsLoadoutDirection(Direction))
	{
		return false;
	}

	const UPandoraInstance* SlotPandora = GetSelectedPandora(Direction);
	const UPandoraDefinition* SlotPandoraDefinition = SlotPandora ? SlotPandora->PandoraDefinition.Get() : nullptr;
	if (!SlotPandoraDefinition && PandoraComponent)
	{
		SlotPandoraDefinition = PandoraComponent->GetPandoraLoadoutDefinition(Direction);
	}

	bool bPandoraWouldChange = false;
	if (PandoraComponent && SlotPandoraDefinition)
	{
		bPandoraWouldChange = PandoraComponent->GetCurrentPandoraDefinition() != SlotPandoraDefinition
			|| PandoraComponent->GetCurrentPandoraLoadoutDirection() != Direction;
	}

	UItemInstance* SlotWeapon = GetSelectedWeapon(Direction);
	bool bWeaponWouldChange = false;
	if (EquipmentComponent && IsValid(SlotWeapon))
	{
		const FGuid SlotWeaponId = SlotWeapon->GetOrCreateItemId();
		const FGuid CurrentWeaponId = EquipmentComponent->GetCurrentWeaponId();
		const bool bSameWeapon = SlotWeaponId.IsValid() && CurrentWeaponId.IsValid()
			? SlotWeaponId == CurrentWeaponId
			: EquipmentComponent->GetCurrentWeaponDefinition() == SlotWeapon->ItemDefinition.Get();
		bWeaponWouldChange = !bSameWeapon || EquipmentComponent->GetCurrentWeaponLoadoutDirection() != Direction;
	}

	return bPandoraWouldChange || bWeaponWouldChange;
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


		if (EquipmentComponent)
		{
			const bool bRequested = EquipmentComponent->RequestWeaponUnequip();

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



	if (PandoraComponent && PandoraDefinition)
	{
		const bool bPandoraRequested = PandoraComponent->RequestPandoraSelectionForDirection(Direction, PandoraDefinition);

	}

	if (!EquipmentComponent)
	{

		return;
	}

	if (!IsValid(CurrentWeapon))
	{

		return;
	}

	const bool bRequested = EquipmentComponent->RequestWeaponSelectionForDirection(Direction, CurrentWeapon);

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
	ReconcileCurrentWeaponLoadoutDirection();

	if (URightInventoryWidget* RightInventoryWidget = CurrentInfoWidget->GetRightInventoryWidget())
	{
		RightInventoryWidget->ToggleActiveFiliterButtons(true);
		ClearInventoryClickEquipBinding();
		BindRightInventoryWidgetEvents(RightInventoryWidget);
	}
}

void UInfoUiPresenter::HandleClickedStatUpButton(FGameplayTag StatTag)
{
	APdPlayerState* PdPlayerState = GetCachedPlayerState();
	UStatUpgradeComponent* StatUpgradeComponent = PdPlayerState ? PdPlayerState->GetStatUpgradeComponent() : nullptr;


	if (StatUpgradeComponent)
	{
		const bool bRequested = StatUpgradeComponent->RequestStatUp(StatTag);

	}
}

void UInfoUiPresenter::HandleClickedStatDownButton(FGameplayTag StatTag)
{
	APdPlayerState* PdPlayerState = GetCachedPlayerState();
	UStatUpgradeComponent* StatUpgradeComponent = PdPlayerState ? PdPlayerState->GetStatUpgradeComponent() : nullptr;


	if (StatUpgradeComponent)
	{
		const bool bRequested = StatUpgradeComponent->RequestStatDown(StatTag);

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

			BindRightInventoryWidgetEvents(RightInventoryWidget);
		}

		if (ULeftEquipmentWidget* LeftEquipmentWidget = CurrentInfoWidget->GetLeftEquipmentWidget())
		{
			LeftEquipmentWidget->InitialzeEquipSlots();
			LeftEquipmentWidget->OnClicked_EquipTypeSlot.RemoveDynamic(
				this,
				&ThisClass::HandleClickedItemEquipTypeSlot);
			LeftEquipmentWidget->OnClicked_EquipTypeSlot.AddUniqueDynamic(this, &ThisClass::HandleClickedItemEquipTypeSlot);
			LeftEquipmentWidget->OnDroppedItem_EquipTypeSlot.RemoveDynamic(
				this,
				&ThisClass::HandleDroppedItemEquipTypeSlot);
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

			RightSkinWidget->OnClicked_SkinFilterAllButton.RemoveDynamic(
				this,
				&ThisClass::HandleClickedSkinFilterAllButton);
			RightSkinWidget->OnClicked_SkinFilterAllButton.AddUniqueDynamic(this, &ThisClass::HandleClickedSkinFilterAllButton);

			RightSkinWidget->OnClicked_SkinFilterTypeButton.RemoveDynamic(
				this,
				&ThisClass::HandleClickedSkinFilterTypeButton);
			RightSkinWidget->OnClicked_SkinFilterTypeButton.AddUniqueDynamic(this, &ThisClass::HandleClickedSkinFilterTypeButton);
		}

		if (ULeftSkinWidget* LeftSkinWidget = CurrentInfoWidget->GetLeftSkinWidget())
		{
			LeftSkinWidget->InitialzeEquipSlots();
			LeftSkinWidget->OnClicked_SkinEquipTypeSlot.RemoveDynamic(
				this,
				&ThisClass::HandleClickedSkinEquipTypeSlot);
			LeftSkinWidget->OnClicked_SkinEquipTypeSlot.AddUniqueDynamic(this, &ThisClass::HandleClickedSkinEquipTypeSlot);
			LeftSkinWidget->OnDroppedSkin_SkinEquipTypeSlot.RemoveDynamic(
				this,
				&ThisClass::HandleDroppedSkinEquipTypeSlot);
			LeftSkinWidget->OnDroppedSkin_SkinEquipTypeSlot.AddUniqueDynamic(this, &ThisClass::HandleDroppedSkinEquipTypeSlot);
		}
		RefreshLeftSkinSlots();
		return;
	}

	if (LeftUiTag == GetPandoraEquipmentLeftUiTag())
	{
		CachedSelectedPandoraEquipSlot = nullptr;
		bShowOnlyOwnedPandorasForEquipSlot = false;
		bUsePandoraTypeFilter = false;
		CurrentPandoraFilterTag = FGameplayTag();

		TArray<UObject*> CurrentPandoraList;
		BuildPandoraTileViewItems(CurrentPandoraList, FGameplayTag(), false);

		if (URightPandoraWidget* RightPandoraWidget = CurrentInfoWidget->GetRightPandoraWidget())
		{
			RightPandoraWidget->SetTileViewAndShowLockState(CurrentPandoraList);
			BindPandoraTileItemClicked();

			RightPandoraWidget->OnClicked_PandoraFilterAllButton.RemoveDynamic(
				this,
				&ThisClass::HandleClickedPandoraFilterAllButton);
			RightPandoraWidget->OnClicked_PandoraFilterAllButton.AddUniqueDynamic(this, &ThisClass::HandleClickedPandoraFilterAllButton);

			RightPandoraWidget->OnClicked_PandoraFilterTypeButton.RemoveDynamic(
				this,
				&ThisClass::HandleClickedPandoraFilterTypeButton);
			RightPandoraWidget->OnClicked_PandoraFilterTypeButton.AddUniqueDynamic(this, &ThisClass::HandleClickedPandoraFilterTypeButton);
		}

		if (ULeftPandoraWidget* LeftPandoraWidget = CurrentInfoWidget->GetLeftPandoraWidget())
		{
			LeftPandoraWidget->OnClicked_PandoraEquipSlot.RemoveDynamic(
				this,
				&ThisClass::HandleClickedPandoraEquipSlot);
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

	if (!Controller || !ItemInstance)
	{

		return;
	}

	const UItemDefinition* ItemDefinition = ItemInstance->ItemDefinition;

	if (!ItemDefinition || !ItemDefinition->IdTag.IsValid())
	{

		return;
	}

	if (!CachedSelectedEquipSlot || !CachedSelectedEquipTypeTag.IsValid())
	{

		return;
	}

	const bool bMatchesSelectedSlotType = ItemDefinition->IdTag.MatchesTag(CachedSelectedEquipTypeTag);

	if (!bMatchesSelectedSlotType)
	{

		return;
	}

	APdPlayerState* PlayerState = GetCachedPlayerState();
	UInventoryComponent* InventoryComponent = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	const FGameplayTag ConsumableItemTypeTag = GetConsumableItemTypeTag();
	const bool bSelectedConsumableSlot = ConsumableItemTypeTag.IsValid()
		&& CachedSelectedEquipTypeTag.MatchesTag(ConsumableItemTypeTag);
	if (bSelectedConsumableSlot)
	{
		if (!InventoryComponent
			|| !InventoryComponent->SetConsumableQuickSlot(
				CachedSelectedEquipSlot->GetNth() - 1,
				ItemInstance))
		{
			return;
		}

		ClearInventoryClickEquipBinding();
		return;
	}

	const FGameplayTag WeaponItemTypeTag = GetWeaponItemTypeTag();
	const bool bSelectedWeaponSlot = WeaponItemTypeTag.IsValid()
		&& CachedSelectedEquipTypeTag.MatchesTag(WeaponItemTypeTag);
	if (bSelectedWeaponSlot)
	{
		const EEnum_Direction Direction =
			FPandoraLoadoutUiModel::GetDirectionFromSelectSlotNumber(CachedSelectedEquipSlot->GetNth());
		if (!InventoryComponent
			|| !InventoryComponent->SetPandoraWeaponLoadoutSlot(Direction, ItemInstance))
		{
			RefreshLeftEquipmentSlots();
			RefreshSelectPandoraLoadoutImages();
			RefreshSelectPandoraCompatibilityState();
			return;
		}

		PendingClearedWeaponId.Invalidate();
		PendingClearedWeaponDirection = EEnum_Direction::Center;
		ClearInventoryClickEquipBinding();
		return;
	}

	CachedSelectedEquipSlot->SetData(ItemInstance);
	ClearInventoryClickEquipBinding();
	RefreshInventoryTileView();
}

void UInfoUiPresenter::HandleClickedItemEquipTypeSlot(FGameplayTag EquipTypeTag, UEquipSlotWidget* SelectedEquipSlot, bool bIsSelectedAnyButton)
{
	(void)bIsSelectedAnyButton;

	APdPlayerController* Controller = GetController();
	if (!Controller)
	{

		return;
	}

	ClearInventoryClickEquipBinding();
	CachedSelectedEquipSlot = nullptr;
	CachedSelectedEquipTypeTag = FGameplayTag();



	if (!SelectedEquipSlot || !EquipTypeTag.IsValid())
	{

		return;
	}

	if (SelectedEquipSlot->HasEquippedItem())
	{
		ClearEquipmentSlot(SelectedEquipSlot, EquipTypeTag);
		return;
	}

	HandleClickedItemFilterTypeButton(EquipTypeTag);

}

void UInfoUiPresenter::HandleDroppedItemEquipTypeSlot(FGameplayTag EquipTypeTag, UEquipSlotWidget* TargetEquipSlot, UItemInstance* ItemInstance)
{


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

		return;
	}

	const FGameplayTag TargetTag = TargetEquipSlot->GetAcceptedEquipTypeTag();


	CachedSelectedEquipSlot = TargetEquipSlot;
	CachedSelectedEquipTypeTag = TargetTag;
	HandleClickedItemSlot(ItemInstance);
}

void UInfoUiPresenter::HandleDroppedInventorySlot(const int32 SourceSlotIndex, const int32 TargetSlotIndex, UItemInstance* SourceItem)
{
	if (!IsValid(SourceItem))
	{

		return;
	}

	if (SourceSlotIndex == TargetSlotIndex)
	{

		return;
	}

	SourceItem->EnsureItemId();
	const FGuid SourceItemId = SourceItem->GetItemId();
	if (!SourceItemId.IsValid())
	{

		return;
	}

	const int32 SourceDisplayIndex = FindInventoryDisplaySlotIndexByItemId(SourceItemId);
	if (SourceDisplayIndex == INDEX_NONE)
	{

		return;
	}

	int32 TargetDisplayIndex = INDEX_NONE;
	UItemInstance* TargetItem = CachedInventoryViewSlots.IsValidIndex(TargetSlotIndex) ? CachedInventoryViewSlots[TargetSlotIndex].Get() : nullptr;
	if (bUseItemTypeFilter)
	{
		if (!IsValid(TargetItem))
		{

			return;
		}

		TargetItem->EnsureItemId();
		TargetDisplayIndex = FindInventoryDisplaySlotIndexByItemId(TargetItem->GetItemId());
	}
	else
	{
		TargetDisplayIndex = TargetSlotIndex;
	}

	if (TargetDisplayIndex == INDEX_NONE)
	{

		return;
	}

	const int32 RequiredSlotCount = FMath::Max(SourceDisplayIndex, TargetDisplayIndex) + 1;
	if (InventoryDisplaySlots.Num() < RequiredSlotCount)
	{
		InventoryDisplaySlots.SetNum(RequiredSlotCount);
	}

	UItemInstance* PreviousTargetItem = InventoryDisplaySlots[TargetDisplayIndex].Get();
	InventoryDisplaySlots[TargetDisplayIndex] = SourceItem;
	InventoryDisplaySlots[SourceDisplayIndex] = PreviousTargetItem;



	RefreshInventoryTileView();
}

void UInfoUiPresenter::HandleClickedItemFilterTypeButton(FGameplayTag TypeTag)
{
	if (!TypeTag.IsValid())
	{
		bUseItemTypeFilter = false;
		CurrentItemFilterTag = FGameplayTag();

		RefreshInventoryTileView();
		return;
	}

	bUseItemTypeFilter = true;
	CurrentItemFilterTag = TypeTag;


	APdPlayerController* Controller = GetController();
	if (!Controller)
	{

		return;
	}

	RefreshInventoryTileView();
}

void UInfoUiPresenter::HandleClickedItemFilterAllButton()
{
	bUseItemTypeFilter = false;
	CurrentItemFilterTag = FGameplayTag();


	APdPlayerController* Controller = GetController();
	if (!Controller)
	{

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

	if (!Controller || !SkinInstance)
	{

		return;
	}

	const USkinDefinition* SkinDefinition = SkinInstance->SkinDefinition.Get();

	if (!SkinDefinition || !SkinDefinition->IdTag.IsValid())
	{

		return;
	}

	if (!CachedSelectedSkinEquipSlot || !CachedSelectedSkinEquipTypeTag.IsValid())
	{

		return;
	}

	const FGameplayTag RequiredSkinTag = ResolveSkinDefinitionMatchTag(CachedSelectedSkinEquipTypeTag);
	const bool bMatchesSelectedSlotType = SkinDefinition->IdTag.MatchesTag(RequiredSkinTag);

	if (!bMatchesSelectedSlotType)
	{

		return;
	}

	APdPlayer* PlayerCharacter = Cast<APdPlayer>(Controller->GetPawn());
	USkinEquipmentComponent* SkinEquipmentComponent = PlayerCharacter ? PlayerCharacter->GetSkinEquipmentComponent() : nullptr;

	CachedSelectedSkinEquipSlot->SetData(SkinInstance);

	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	if (URightSkinWidget* RightSkinWidget = CurrentInfoWidget ? CurrentInfoWidget->GetRightSkinWidget() : nullptr)
	{
		ClearSkinClickEquipBinding();
	}

	if (SkinEquipmentComponent)
	{
		const bool bRequested = SkinEquipmentComponent->RequestEquipSkin(SkinInstance, CachedSelectedSkinEquipTypeTag);


		if (!bRequested || (SkinEquipmentComponent->GetOwner() && SkinEquipmentComponent->GetOwner()->HasAuthority()))
		{

			RefreshLeftSkinSlots();
		}
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



	if (!SelectedEquipSlot || !EquipTypeTag.IsValid())
	{

		return;
	}

	if (SelectedEquipSlot->HasEquippedSkin())
	{
		ClearSkinEquipSlot(SelectedEquipSlot, EquipTypeTag);
		return;
	}

	CachedSelectedSkinEquipSlot = SelectedEquipSlot;
	CachedSelectedSkinEquipTypeTag = EquipTypeTag;
	HandleClickedSkinFilterTypeButton(ResolveSkinDefinitionMatchTag(EquipTypeTag));

}

void UInfoUiPresenter::HandleDroppedSkinEquipTypeSlot(FGameplayTag EquipTypeTag, USkinEquipSlotWidget* TargetSkinEquipSlot, USkinInstance* SkinInstance)
{


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

		return;
	}

	const FGameplayTag TargetTag = TargetSkinEquipSlot->GetAcceptedEquipTypeTag();


	CachedSelectedSkinEquipSlot = TargetSkinEquipSlot;
	CachedSelectedSkinEquipTypeTag = TargetTag;
	HandleClickedSkinSlot(SkinInstance);
}

void UInfoUiPresenter::HandleClickedSkinFilterTypeButton(FGameplayTag TypeTag)
{
	TypeTag = ResolveSkinDefinitionMatchTag(TypeTag);

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

	if (!Controller || !PandoraInstance)
	{

		return;
	}

	if (!IsPandoraOwnedForEquipInventory(PandoraInstance))
	{

		return;
	}

	const UPandoraDefinition* PandoraDefinition = PandoraInstance->PandoraDefinition;
	if (!PandoraDefinition)
	{

		return;
	}

	APdPlayerState* PlayerState = GetCachedPlayerState();
	UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	EEnum_Direction Direction = EEnum_Direction::Center;
	int32 Nth = 0;
	if (!ResolvePandoraLoadoutSlotForClick(PandoraComponent, PandoraDefinition, Direction, Nth))
	{

		return;
	}


	if (PandoraComponent)
	{
		const bool bRequested = PandoraComponent->RequestSetPandoraLoadoutSlot(Direction, PandoraDefinition);


		if (bRequested)
		{
			ResetPandoraEquipSlotClickState();
			RefreshLeftPandoraSlots();
			RefreshSelectPandoraLoadoutImages();
			RefreshSelectPandoraCompatibilityState();
		}

		if (!bRequested)
		{
			RefreshLeftPandoraSlots();
			RefreshSelectPandoraLoadoutImages();
			RefreshSelectPandoraCompatibilityState();
		}
	}
}

void UInfoUiPresenter::HandleClickedPandoraEquipSlot(UPandoraEquipSlotWidget* SelectedPandoraEquipSlot, bool bIsSelectedAnyButton)
{
	APdPlayerController* Controller = GetController();
	if (!Controller)
	{
		return;
	}

	if (SelectedPandoraEquipSlot && SelectedPandoraEquipSlot->GetCachedData())
	{
		ClearPandoraEquipSlot(SelectedPandoraEquipSlot);
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

		CachedSelectedPandoraEquipSlot = nullptr;
		bShowOnlyOwnedPandorasForEquipSlot = false;

		RefreshPandoraTileView();
		return;
	}

	bShowOnlyOwnedPandorasForEquipSlot = true;
	RefreshPandoraTileView();

	if (UTileView* TileView = RightPandoraWidget->GetTileView())
	{
		BindPandoraTileItemClicked();

	}
}

void UInfoUiPresenter::HandleClickedPandoraFilterTypeButton(FGameplayTag TypeTag)
{
	APdPlayerController* Controller = GetController();
	if (!Controller)
	{
		return;
	}

	if (!TypeTag.IsValid())
	{
		bUsePandoraTypeFilter = false;
		CurrentPandoraFilterTag = FGameplayTag();

		RefreshPandoraTileView();
		return;
	}

	bUsePandoraTypeFilter = true;
	CurrentPandoraFilterTag = TypeTag;

	RefreshPandoraTileView();
}

void UInfoUiPresenter::HandleClickedPandoraFilterAllButton()
{
	APdPlayerController* Controller = GetController();
	if (!Controller)
	{
		return;
	}

	bUsePandoraTypeFilter = false;
	CurrentPandoraFilterTag = FGameplayTag();

	RefreshPandoraTileView();
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

FGameplayTag UInfoUiPresenter::GetConsumableItemTypeTag() const
{
	return UProjectTagConfig::Get(this)->GetItemConsumableTypeTag();
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

	UItemInstance* ClearedItemInstance = TargetEquipSlot->GetItemInstance();
	const FGameplayTag WeaponItemTypeTag = GetWeaponItemTypeTag();
	const FGameplayTag ConsumableItemTypeTag = GetConsumableItemTypeTag();
	APdPlayerState* PlayerState = GetCachedPlayerState();
	UInventoryComponent* InventoryComponent = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	if (ConsumableItemTypeTag.IsValid() && EquipTypeTag.MatchesTag(ConsumableItemTypeTag))
	{
		if (!InventoryComponent
			|| !InventoryComponent->ClearConsumableQuickSlot(TargetEquipSlot->GetNth() - 1))
		{
			RefreshLeftEquipmentSlots();
			return;
		}

		CachedSelectedEquipSlot = nullptr;
		CachedSelectedEquipTypeTag = FGameplayTag();
		return;
	}

	if (WeaponItemTypeTag.IsValid() && EquipTypeTag.MatchesTag(WeaponItemTypeTag))
	{
		const EEnum_Direction Direction =
			FPandoraLoadoutUiModel::GetDirectionFromSelectSlotNumber(TargetEquipSlot->GetNth());
		const FGuid ClearedWeaponId = IsValid(ClearedItemInstance)
			? ClearedItemInstance->GetItemId()
			: FGuid();
		PendingClearedWeaponId = ClearedWeaponId;
		PendingClearedWeaponDirection = Direction;
		if (!InventoryComponent
			|| !InventoryComponent->ClearPandoraWeaponLoadoutSlot(Direction))
		{
			PendingClearedWeaponId.Invalidate();
			PendingClearedWeaponDirection = EEnum_Direction::Center;
			RefreshLeftEquipmentSlots();
			RefreshSelectPandoraLoadoutImages();
			RefreshSelectPandoraCompatibilityState();
			return;
		}

		CachedSelectedEquipSlot = nullptr;
		CachedSelectedEquipTypeTag = FGameplayTag();
		return;
	}

	TargetEquipSlot->SetData(nullptr);
	if (CachedSelectedEquipSlot == TargetEquipSlot)
	{
		CachedSelectedEquipSlot = nullptr;
		CachedSelectedEquipTypeTag = FGameplayTag();
	}
	RefreshInventoryTileView();
}

void UInfoUiPresenter::ClearPandoraEquipSlot(UPandoraEquipSlotWidget* TargetPandoraEquipSlot)
{
	if (!TargetPandoraEquipSlot)
	{
		return;
	}

	const int32 Nth = TargetPandoraEquipSlot->GetNth();
	const EEnum_Direction Direction = FPandoraLoadoutUiModel::GetDirectionFromSelectSlotNumber(Nth);
	if (!PandoraLoadout::IsLoadoutDirection(Direction))
	{

		return;
	}

	APdPlayerState* PlayerState = GetCachedPlayerState();
	UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	if (!PandoraComponent)
	{

		return;
	}

	const bool bRequested = PandoraComponent->RequestSetPandoraLoadoutSlot(Direction, nullptr);
	if (!bRequested)
	{
		RefreshLeftPandoraSlots();
		RefreshSelectPandoraLoadoutImages();
		RefreshSelectPandoraCompatibilityState();
		return;
	}

	if (CachedSelectedPandoraEquipSlot == TargetPandoraEquipSlot)
	{
		CachedSelectedPandoraEquipSlot = nullptr;
	}
	bShowOnlyOwnedPandorasForEquipSlot = false;

	TargetPandoraEquipSlot->ToggleText_Apply(false);

	if (UInfoWidget* CurrentInfoWidget = GetInfoWidget())
	{
		if (ULeftPandoraWidget* LeftPandoraWidget = CurrentInfoWidget->GetLeftPandoraWidget())
		{
			LeftPandoraWidget->ClearPandoraEquipSlotSelection();
		}
	}

	RefreshPandoraTileView();
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

		return;
	}

	const bool bRequested = SkinEquipmentComponent->RequestUnequipSkinSlot(EquipTypeTag);
	const bool bAuthority = SkinEquipmentComponent->GetOwner() && SkinEquipmentComponent->GetOwner()->HasAuthority();


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
	const UItemInstance* LeftWeapon = GetSelectedWeapon(EEnum_Direction::Left);
	const UItemInstance* UpWeapon = GetSelectedWeapon(EEnum_Direction::Up);
	const UItemInstance* RightWeapon = GetSelectedWeapon(EEnum_Direction::Right);
	const TArray<FPandoraSelectSlotUiData> Slots = FPandoraLoadoutUiModel::BuildSelectSlots(
		PandoraComponent,
		LeftWeapon,
		UpWeapon,
		RightWeapon);

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
	const UItemInstance* LeftWeapon = GetSelectedWeapon(EEnum_Direction::Left);
	const UItemInstance* UpWeapon = GetSelectedWeapon(EEnum_Direction::Up);
	const UItemInstance* RightWeapon = GetSelectedWeapon(EEnum_Direction::Right);
	const TArray<FPandoraSelectSlotUiData> Slots = FPandoraLoadoutUiModel::BuildSelectSlots(
		PandoraComponent,
		LeftWeapon,
		UpWeapon,
		RightWeapon);

	for (const FPandoraSelectSlotUiData& Slot : Slots)
	{
		SelectPandoraWidget->SetPandoraImage(Slot.SlotNumber, Slot.IconTexture);
	}

	const auto ResolveWeaponIcon = [](const UItemInstance* WeaponInstance) -> UTexture2D*
	{
		const UItemDefinition* WeaponDefinition =
			IsValid(WeaponInstance) ? WeaponInstance->ItemDefinition.Get() : nullptr;
		return WeaponDefinition ? WeaponDefinition->IconTexture.Get() : nullptr;
	};
	SelectPandoraWidget->SetWeaponImage(1, ResolveWeaponIcon(LeftWeapon));
	SelectPandoraWidget->SetWeaponImage(2, ResolveWeaponIcon(UpWeapon));
	SelectPandoraWidget->SetWeaponImage(3, ResolveWeaponIcon(RightWeapon));
}

void UInfoUiPresenter::ReconcileCurrentWeaponLoadoutDirection()
{
	APdPlayerController* Controller = GetController();
	APdPlayer* PlayerCharacter = Controller ? Cast<APdPlayer>(Controller->GetPawn()) : nullptr;
	UEquipmentComponent* EquipmentComponent = PlayerCharacter ? PlayerCharacter->GetEquipmentComponent() : nullptr;
	const APdPlayerState* PlayerState = GetCachedPlayerState();
	const UInventoryComponent* InventoryComponent = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	if (!EquipmentComponent || !InventoryComponent)
	{
		return;
	}

	const FGuid CurrentWeaponId = EquipmentComponent->GetCurrentWeaponId();
	if (!CurrentWeaponId.IsValid())
	{
		PendingClearedWeaponId.Invalidate();
		PendingClearedWeaponDirection = EEnum_Direction::Center;
		return;
	}

	for (const EEnum_Direction Direction :
		{ EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right })
	{
		if (InventoryComponent->GetPandoraWeaponLoadoutItemId(Direction) != CurrentWeaponId)
		{
			continue;
		}

		if (EquipmentComponent->GetCurrentWeaponLoadoutDirection() != Direction)
		{
			if (UItemInstance* CurrentWeapon = InventoryComponent->FindItemInstanceById(CurrentWeaponId))
			{
				EquipmentComponent->RequestCurrentWeaponLoadoutDirection(Direction, CurrentWeapon);
			}
		}
		PendingClearedWeaponId.Invalidate();
		PendingClearedWeaponDirection = EEnum_Direction::Center;
		return;
	}

	const bool bConfirmedPendingClear = PendingClearedWeaponId == CurrentWeaponId
		&& PandoraLoadout::IsLoadoutDirection(PendingClearedWeaponDirection)
		&& InventoryComponent->GetPandoraWeaponLoadoutItemId(PendingClearedWeaponDirection)
			!= PendingClearedWeaponId;
	if (bConfirmedPendingClear)
	{
		EquipmentComponent->RequestWeaponUnequip();
		PendingClearedWeaponId.Invalidate();
		PendingClearedWeaponDirection = EEnum_Direction::Center;
	}
}

int32 UInfoUiPresenter::ResolveCurrentEquippedWeaponSlotNumber() const
{
	APdPlayerController* Controller = GetController();
	APdPlayer* PlayerCharacter = Controller ? Cast<APdPlayer>(Controller->GetPawn()) : nullptr;
	const UEquipmentComponent* EquipmentComponent = PlayerCharacter ? PlayerCharacter->GetEquipmentComponent() : nullptr;
	if (!EquipmentComponent)
	{
		return 0;
	}

	const FGuid CurrentWeaponId = EquipmentComponent->GetCurrentWeaponId();
	const UItemDefinition* CurrentWeaponDefinition = EquipmentComponent->GetCurrentWeaponDefinition();
	const auto MatchesCurrentWeapon = [CurrentWeaponId, CurrentWeaponDefinition](const UItemInstance* WeaponInstance)
	{
		if (!IsValid(WeaponInstance))
		{
			return false;
		}

		const FGuid WeaponId = WeaponInstance->GetItemId();
		if (CurrentWeaponId.IsValid())
		{
			return WeaponId.IsValid() && CurrentWeaponId == WeaponId;
		}

		return CurrentWeaponDefinition && WeaponInstance->ItemDefinition == CurrentWeaponDefinition;
	};

	if (MatchesCurrentWeapon(GetSelectedWeapon(EEnum_Direction::Left)))
	{
		return 1;
	}

	if (MatchesCurrentWeapon(GetSelectedWeapon(EEnum_Direction::Up)))
	{
		return 2;
	}

	if (MatchesCurrentWeapon(GetSelectedWeapon(EEnum_Direction::Right)))
	{
		return 3;
	}


	return 0;
}

void UInfoUiPresenter::RefreshLeftEquipmentSlots() const
{
	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	ULeftEquipmentWidget* LeftEquipmentWidget = CurrentInfoWidget ? CurrentInfoWidget->GetLeftEquipmentWidget() : nullptr;
	if (!LeftEquipmentWidget)
	{
		return;
	}

	LeftEquipmentWidget->SetWeaponSlotData(1, GetSelectedWeapon(EEnum_Direction::Left));
	LeftEquipmentWidget->SetWeaponSlotData(2, GetSelectedWeapon(EEnum_Direction::Up));
	LeftEquipmentWidget->SetWeaponSlotData(3, GetSelectedWeapon(EEnum_Direction::Right));

	const APdPlayerState* PlayerState = GetCachedPlayerState();
	const UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	LeftEquipmentWidget->SetWeaponSlotPandoraRequirement(
		1,
		PandoraComponent ? PandoraComponent->GetPandoraLoadoutDefinition(EEnum_Direction::Left) : nullptr);
	LeftEquipmentWidget->SetWeaponSlotPandoraRequirement(
		2,
		PandoraComponent ? PandoraComponent->GetPandoraLoadoutDefinition(EEnum_Direction::Up) : nullptr);
	LeftEquipmentWidget->SetWeaponSlotPandoraRequirement(
		3,
		PandoraComponent ? PandoraComponent->GetPandoraLoadoutDefinition(EEnum_Direction::Right) : nullptr);

	const UInventoryComponent* InventoryComponent = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	for (int32 SlotIndex = 0; SlotIndex < UInventoryComponent::ConsumableQuickSlotCount; ++SlotIndex)
	{
		LeftEquipmentWidget->SetConsumableQuickSlotData(SlotIndex + 1, InventoryComponent ? InventoryComponent->GetConsumableQuickSlotItem(SlotIndex) : nullptr);
	}
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

	const auto ResolveWeaponIcon = [](const UItemInstance* WeaponInstance) -> UTexture2D*
	{
		const UItemDefinition* WeaponDefinition =
			IsValid(WeaponInstance) ? WeaponInstance->ItemDefinition.Get() : nullptr;
		return WeaponDefinition ? WeaponDefinition->IconTexture.Get() : nullptr;
	};
	LeftPandoraWidget->SetWeaponImage(
		1,
		ResolveWeaponIcon(GetSelectedWeapon(EEnum_Direction::Left)));
	LeftPandoraWidget->SetWeaponImage(
		2,
		ResolveWeaponIcon(GetSelectedWeapon(EEnum_Direction::Up)));
	LeftPandoraWidget->SetWeaponImage(
		3,
		ResolveWeaponIcon(GetSelectedWeapon(EEnum_Direction::Right)));
}

bool UInfoUiPresenter::IsPandoraOwnedForEquipInventory(const UPandoraInstance* PandoraInstance) const
{
	if (!IsValid(PandoraInstance))
	{
		return false;
	}

	UPandoraDefinition* PandoraDefinition = const_cast<UPandoraDefinition*>(PandoraInstance->PandoraDefinition.Get());
	if (!IsValid(PandoraDefinition))
	{
		return false;
	}

	const APdPlayerState* PlayerState = GetCachedPlayerState();
	const UPandoraTreeComponent* PandoraTreeComponent = PlayerState ? PlayerState->GetPandoraTreeComponent() : nullptr;
	if (PandoraTreeComponent)
	{
		return PandoraTreeComponent->IsPandoraUnlockedForTree(PandoraDefinition);
	}

	return PandoraInstance->IsOwned;
}

void UInfoUiPresenter::BuildPandoraTileViewItems(TArray<UObject*>& OutListItems, const FGameplayTag TypeTag, const bool bOwnedOnly) const
{
	OutListItems.Reset();

	const APdPlayerState* PlayerState = GetCachedPlayerState();
	const UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	if (!PandoraComponent)
	{

		return;
	}

	const FPandoraList* SourcePandoraList = &PandoraComponent->AllPandoraList;
	if (TypeTag.IsValid())
	{
		SourcePandoraList = PandoraComponent->Map_Type_PandoraList.Find(TypeTag);
	}

	if (!SourcePandoraList)
	{

		return;
	}

	OutListItems.Reserve(SourcePandoraList->Pandoras.Num());
	for (const TObjectPtr<UPandoraInstance>& Pandora : SourcePandoraList->Pandoras)
	{
		UPandoraInstance* PandoraInstance = Pandora.Get();
		if (!IsValid(PandoraInstance))
		{
			continue;
		}

		if (bOwnedOnly && !IsPandoraOwnedForEquipInventory(PandoraInstance))
		{
			continue;
		}

		OutListItems.Add(PandoraInstance);
	}
}

void UInfoUiPresenter::RefreshPandoraTileView() const
{
	TArray<UObject*> CurrentPandoraList;
	BuildPandoraTileViewItems(
		CurrentPandoraList,
		bUsePandoraTypeFilter ? CurrentPandoraFilterTag : FGameplayTag(),
		bShowOnlyOwnedPandorasForEquipSlot);

	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	if (URightPandoraWidget* RightPandoraWidget = CurrentInfoWidget ? CurrentInfoWidget->GetRightPandoraWidget() : nullptr)
	{
		RightPandoraWidget->SetTileViewAndShowLockState(CurrentPandoraList);

	}
}

void UInfoUiPresenter::BindPandoraTileItemClicked()
{
	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	URightPandoraWidget* RightPandoraWidget = CurrentInfoWidget ? CurrentInfoWidget->GetRightPandoraWidget() : nullptr;
	UTileView* TileView = RightPandoraWidget ? RightPandoraWidget->GetTileView() : nullptr;
	if (!TileView)
	{
		return;
	}

	if (BoundPandoraTileView.Get() == TileView && PandoraTileItemClickedDelegateHandle.IsValid())
	{
		return;
	}

	if (UTileView* PreviousTileView = BoundPandoraTileView.Get())
	{
		if (PandoraTileItemClickedDelegateHandle.IsValid())
		{
			PreviousTileView->OnItemClicked().Remove(PandoraTileItemClickedDelegateHandle);
		}
	}

	BoundPandoraTileView = TileView;
	PandoraTileItemClickedDelegateHandle =
		TileView->OnItemClicked().AddUObject(this, &ThisClass::HandleClickedPandoraSlot);
}

bool UInfoUiPresenter::ResolvePandoraLoadoutSlotForClick(
	const UPandoraComponent* PandoraComponent,
	const UPandoraDefinition* PandoraDefinition,
	EEnum_Direction& OutDirection,
	int32& OutSlotNumber) const
{
	OutDirection = EEnum_Direction::Center;
	OutSlotNumber = 0;

	if (!PandoraComponent || !PandoraDefinition)
	{
		return false;
	}

	if (CachedSelectedPandoraEquipSlot)
	{
		const int32 SelectedNth = CachedSelectedPandoraEquipSlot->GetNth();
		const EEnum_Direction SelectedDirection = FPandoraLoadoutUiModel::GetDirectionFromSelectSlotNumber(SelectedNth);
		if (PandoraLoadout::IsLoadoutDirection(SelectedDirection))
		{
			OutDirection = SelectedDirection;
			OutSlotNumber = SelectedNth;
			return true;
		}
	}

	for (const EEnum_Direction Direction : { EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right })
	{
		if (PandoraComponent->GetPandoraLoadoutDefinition(Direction) == PandoraDefinition)
		{
			OutDirection = Direction;
			OutSlotNumber = FPandoraLoadoutUiModel::GetSelectSlotNumberFromDirection(Direction);
			return true;
		}
	}

	for (const EEnum_Direction Direction : { EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right })
	{
		if (!PandoraComponent->GetPandoraLoadoutDefinition(Direction))
		{
			OutDirection = Direction;
			OutSlotNumber = FPandoraLoadoutUiModel::GetSelectSlotNumberFromDirection(Direction);
			return true;
		}
	}

	return false;
}

void UInfoUiPresenter::ResetPandoraEquipSlotClickState()
{
	if (CachedSelectedPandoraEquipSlot)
	{
		CachedSelectedPandoraEquipSlot->ToggleText_Apply(false);
	}

	CachedSelectedPandoraEquipSlot = nullptr;
	bShowOnlyOwnedPandorasForEquipSlot = false;

	if (UInfoWidget* CurrentInfoWidget = GetInfoWidget())
	{
		if (ULeftPandoraWidget* LeftPandoraWidget = CurrentInfoWidget->GetLeftPandoraWidget())
		{
			LeftPandoraWidget->ClearPandoraEquipSlotSelection();
		}
	}

	RefreshPandoraTileView();
}

void UInfoUiPresenter::UnbindInfoUiEvents()
{
	if (InfoWidget)
	{
		InfoWidget->OnDroppedItemToCharacterPanel.RemoveDynamic(
			this,
			&ThisClass::HandleDroppedItemToCharacterPanel);
		InfoWidget->OnDroppedSkinToCharacterPanel.RemoveDynamic(
			this,
			&ThisClass::HandleDroppedSkinToCharacterPanel);

		if (ULeftEquipmentWidget* LeftEquipmentWidget = InfoWidget->GetLeftEquipmentWidget())
		{
			LeftEquipmentWidget->OnClicked_EquipTypeSlot.RemoveDynamic(
				this,
				&ThisClass::HandleClickedItemEquipTypeSlot);
			LeftEquipmentWidget->OnDroppedItem_EquipTypeSlot.RemoveDynamic(
				this,
				&ThisClass::HandleDroppedItemEquipTypeSlot);
		}
		if (URightInventoryWidget* RightInventoryWidget = InfoWidget->GetRightInventoryWidget())
		{
			RightInventoryWidget->OnClicked_FilterAllButton.RemoveDynamic(
				this,
				&ThisClass::HandleClickedItemFilterAllButton);
			RightInventoryWidget->OnClicked_FilterTypeButton.RemoveDynamic(
				this,
				&ThisClass::HandleClickedItemFilterTypeButton);
			RightInventoryWidget->OnDropped_InventorySlot.RemoveDynamic(
				this,
				&ThisClass::HandleDroppedInventorySlot);
		}
		if (ULeftSkinWidget* LeftSkinWidget = InfoWidget->GetLeftSkinWidget())
		{
			LeftSkinWidget->OnClicked_SkinEquipTypeSlot.RemoveDynamic(
				this,
				&ThisClass::HandleClickedSkinEquipTypeSlot);
			LeftSkinWidget->OnDroppedSkin_SkinEquipTypeSlot.RemoveDynamic(
				this,
				&ThisClass::HandleDroppedSkinEquipTypeSlot);
		}
		if (URightSkinWidget* RightSkinWidget = InfoWidget->GetRightSkinWidget())
		{
			RightSkinWidget->OnClicked_SkinFilterAllButton.RemoveDynamic(
				this,
				&ThisClass::HandleClickedSkinFilterAllButton);
			RightSkinWidget->OnClicked_SkinFilterTypeButton.RemoveDynamic(
				this,
				&ThisClass::HandleClickedSkinFilterTypeButton);
		}
		if (ULeftPandoraWidget* LeftPandoraWidget = InfoWidget->GetLeftPandoraWidget())
		{
			LeftPandoraWidget->OnClicked_PandoraEquipSlot.RemoveDynamic(
				this,
				&ThisClass::HandleClickedPandoraEquipSlot);
		}
		if (URightPandoraWidget* RightPandoraWidget = InfoWidget->GetRightPandoraWidget())
		{
			RightPandoraWidget->OnClicked_PandoraFilterAllButton.RemoveDynamic(
				this,
				&ThisClass::HandleClickedPandoraFilterAllButton);
			RightPandoraWidget->OnClicked_PandoraFilterTypeButton.RemoveDynamic(
				this,
				&ThisClass::HandleClickedPandoraFilterTypeButton);
		}
		if (URightStatusWidget* RightStatusWidget = InfoWidget->GetRightStatusWidget())
		{
			RightStatusWidget->OnClicked_StatUpButton.RemoveDynamic(
				this,
				&ThisClass::HandleClickedStatUpButton);
			RightStatusWidget->OnClicked_StatDownButton.RemoveDynamic(
				this,
				&ThisClass::HandleClickedStatDownButton);
		}
	}

	if (UTileView* TileView = BoundPandoraTileView.Get())
	{
		if (PandoraTileItemClickedDelegateHandle.IsValid())
		{
			TileView->OnItemClicked().Remove(PandoraTileItemClickedDelegateHandle);
		}
	}
	BoundPandoraTileView.Reset();
	PandoraTileItemClickedDelegateHandle.Reset();
}

void UInfoUiPresenter::BindInventoryChangeNotification()
{
	APdPlayerState* PdPlayerState = GetCachedPlayerState();
	UInventoryComponent* InventoryComponent = PdPlayerState ? PdPlayerState->GetInventoryComponent() : nullptr;
	if (BoundInventoryComponent == InventoryComponent
		&& (!InventoryComponent
			|| (InventoryChangedDelegateHandle.IsValid()
				&& PandoraWeaponLoadoutChangedDelegateHandle.IsValid())))
	{
		return;
	}

	UnbindInventoryChangeNotification();

	ResetInventoryDisplaySlots();
	BoundInventoryComponent = InventoryComponent;
	if (BoundInventoryComponent)
	{
		InventoryChangedDelegateHandle = BoundInventoryComponent->OnInventoryChanged.AddUObject(
			this,
			&ThisClass::HandleInventoryChanged);
		PandoraWeaponLoadoutChangedDelegateHandle =
			BoundInventoryComponent->OnPandoraWeaponLoadoutChanged.AddUObject(
				this,
				&ThisClass::HandlePandoraWeaponLoadoutChanged);
	}
}

void UInfoUiPresenter::UnbindInventoryChangeNotification()
{
	if (BoundInventoryComponent)
	{
		if (InventoryChangedDelegateHandle.IsValid())
		{
			BoundInventoryComponent->OnInventoryChanged.Remove(InventoryChangedDelegateHandle);
		}
		if (PandoraWeaponLoadoutChangedDelegateHandle.IsValid())
		{
			BoundInventoryComponent->OnPandoraWeaponLoadoutChanged.Remove(
				PandoraWeaponLoadoutChangedDelegateHandle);
		}
		BoundInventoryComponent = nullptr;
	}
	InventoryChangedDelegateHandle.Reset();
	PandoraWeaponLoadoutChangedDelegateHandle.Reset();
}

void UInfoUiPresenter::BeginItemPresentationPreload()
{
	ReleaseItemPresentationPreload();
	const int32 PreloadGeneration = ++ItemPresentationPreloadGeneration;

	const UGameInstance* GameInstance =
		GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem || !BoundInventoryComponent)
	{
		return;
	}

	TArray<FSoftObjectPath> IconPaths;
	for (const TObjectPtr<UItemInstance>& ItemInstance :
		BoundInventoryComponent->GetAllItems().Items)
	{
		const UItemDefinition* ItemDefinition =
			IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
		if (ItemDefinition)
		{
			IconPaths.Add(ItemDefinition->IconTexture.ToSoftObjectPath());
		}
	}

	ItemPresentationPreloadHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
			IconPaths,
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this, PreloadGeneration]()
				{
					if (PreloadGeneration != ItemPresentationPreloadGeneration)
					{
						return;
					}

					RefreshLeftEquipmentSlots();
					RefreshInventoryTileView();
					RefreshLeftPandoraSlots();
					RefreshSelectPandoraLoadoutImages();
				}));
}

void UInfoUiPresenter::ReleaseItemPresentationPreload()
{
	++ItemPresentationPreloadGeneration;
	if (ItemPresentationPreloadHandle.IsValid())
	{
		ItemPresentationPreloadHandle->CancelHandle();
		ItemPresentationPreloadHandle->ReleaseHandle();
		ItemPresentationPreloadHandle.Reset();
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

	}
}

void UInfoUiPresenter::UnbindPandoraLoadoutChangeNotification()
{
	if (BoundPandoraComponent)
	{
		BoundPandoraComponent->OnPandoraLoadoutChanged.RemoveDynamic(this, &ThisClass::HandlePandoraLoadoutChanged);

		BoundPandoraComponent = nullptr;
	}
}

void UInfoUiPresenter::ResetInventoryDisplaySlots()
{
	InventoryDisplaySlots.Reset();
	CachedInventoryViewSlots.Reset();
	bInventoryDisplaySlotsInitialized = false;
}

void UInfoUiPresenter::ReconcileInventoryDisplaySlots(const TArray<UObject*>& InventoryItems)
{
	TMap<FGuid, UItemInstance*> CurrentItemsById;
	CurrentItemsById.Reserve(InventoryItems.Num());

	for (UObject* ItemObject : InventoryItems)
	{
		UItemInstance* ItemInstance = Cast<UItemInstance>(ItemObject);
		if (!IsValid(ItemInstance))
		{
			continue;
		}

		ItemInstance->EnsureItemId();
		const FGuid ItemId = ItemInstance->GetItemId();
		if (ItemId.IsValid())
		{
			CurrentItemsById.Add(ItemId, ItemInstance);
		}
	}

	if (!bInventoryDisplaySlotsInitialized)
	{
		TArray<UObject*> SortedItems = InventoryItems;
		SortItemObjectsByDisplayName(SortedItems);

		InventoryDisplaySlots.Reset(SortedItems.Num());
		for (UObject* SortedObject : SortedItems)
		{
			if (UItemInstance* ItemInstance = Cast<UItemInstance>(SortedObject))
			{
				InventoryDisplaySlots.Add(ItemInstance);
			}
		}

		bInventoryDisplaySlotsInitialized = true;

		return;
	}

	TSet<FGuid> ExistingSlotIds;
	for (TObjectPtr<UItemInstance>& SlotItem : InventoryDisplaySlots)
	{
		UItemInstance* ItemInstance = SlotItem.Get();
		if (!IsValid(ItemInstance))
		{
			SlotItem = nullptr;
			continue;
		}

		ItemInstance->EnsureItemId();
		const FGuid ItemId = ItemInstance->GetItemId();
		if (!ItemId.IsValid() || !CurrentItemsById.Contains(ItemId))
		{
			SlotItem = nullptr;
			continue;
		}

		ExistingSlotIds.Add(ItemId);
	}

	TArray<UObject*> NewItems;
	for (UObject* ItemObject : InventoryItems)
	{
		UItemInstance* ItemInstance = Cast<UItemInstance>(ItemObject);
		if (!IsValid(ItemInstance))
		{
			continue;
		}

		ItemInstance->EnsureItemId();
		const FGuid ItemId = ItemInstance->GetItemId();
		if (ItemId.IsValid() && !ExistingSlotIds.Contains(ItemId))
		{
			NewItems.Add(ItemInstance);
		}
	}

	SortItemObjectsByDisplayName(NewItems);

	for (UObject* NewItemObject : NewItems)
	{
		UItemInstance* NewItem = Cast<UItemInstance>(NewItemObject);
		if (!IsValid(NewItem))
		{
			continue;
		}

		int32 EmptySlotIndex = INDEX_NONE;
		for (int32 SlotIndex = 0; SlotIndex < InventoryDisplaySlots.Num(); ++SlotIndex)
		{
			if (!InventoryDisplaySlots[SlotIndex].Get())
			{
				EmptySlotIndex = SlotIndex;
				break;
			}
		}

		if (EmptySlotIndex == INDEX_NONE)
		{
			InventoryDisplaySlots.Add(NewItem);
		}
		else
		{
			InventoryDisplaySlots[EmptySlotIndex] = NewItem;
		}
	}

}

void UInfoUiPresenter::BuildInventoryViewSlots(const TArray<UObject*>& SourceItems, TArray<UObject*>& OutViewItems)
{
	OutViewItems.Reset();
	CachedInventoryViewSlots.Reset();

	TSet<FGuid> SourceItemIds;
	SourceItemIds.Reserve(SourceItems.Num());
	for (UObject* SourceObject : SourceItems)
	{
		UItemInstance* ItemInstance = Cast<UItemInstance>(SourceObject);
		if (!IsValid(ItemInstance))
		{
			continue;
		}

		ItemInstance->EnsureItemId();
		const FGuid ItemId = ItemInstance->GetItemId();
		if (ItemId.IsValid())
		{
			SourceItemIds.Add(ItemId);
		}
	}

	TSet<FGuid> AddedItemIds;
	for (const TObjectPtr<UItemInstance>& SlotItemPtr : InventoryDisplaySlots)
	{
		UItemInstance* SlotItem = SlotItemPtr.Get();
		UItemInstance* VisibleItem = nullptr;
		if (IsValid(SlotItem))
		{
			SlotItem->EnsureItemId();
			const FGuid SlotItemId = SlotItem->GetItemId();
			if (SlotItemId.IsValid() && SourceItemIds.Contains(SlotItemId))
			{
				VisibleItem = SlotItem;
				AddedItemIds.Add(SlotItemId);
			}
		}

		if (bUseItemTypeFilter)
		{
			if (VisibleItem)
			{
				OutViewItems.Add(VisibleItem);
				CachedInventoryViewSlots.Add(VisibleItem);
			}
		}
		else
		{
			OutViewItems.Add(VisibleItem);
			CachedInventoryViewSlots.Add(VisibleItem);
		}
	}

	for (UObject* SourceObject : SourceItems)
	{
		UItemInstance* ItemInstance = Cast<UItemInstance>(SourceObject);
		if (!IsValid(ItemInstance))
		{
			continue;
		}

		ItemInstance->EnsureItemId();
		const FGuid ItemId = ItemInstance->GetItemId();
		if (ItemId.IsValid() && !AddedItemIds.Contains(ItemId))
		{
			OutViewItems.Add(ItemInstance);
			CachedInventoryViewSlots.Add(ItemInstance);
			AddedItemIds.Add(ItemId);
		}
	}
}

int32 UInfoUiPresenter::FindInventoryDisplaySlotIndexByItemId(const FGuid ItemId) const
{
	if (!ItemId.IsValid())
	{
		return INDEX_NONE;
	}

	for (int32 SlotIndex = 0; SlotIndex < InventoryDisplaySlots.Num(); ++SlotIndex)
	{
		const UItemInstance* ItemInstance = InventoryDisplaySlots[SlotIndex].Get();
		if (IsValid(ItemInstance) && ItemInstance->GetItemId() == ItemId)
		{
			return SlotIndex;
		}
	}

	return INDEX_NONE;
}

void UInfoUiPresenter::BindRightInventoryWidgetEvents(URightInventoryWidget* RightInventoryWidget)
{
	if (!RightInventoryWidget)
	{
		return;
	}

	RightInventoryWidget->OnClicked_FilterAllButton.RemoveDynamic(
		this,
		&ThisClass::HandleClickedItemFilterAllButton);
	RightInventoryWidget->OnClicked_FilterAllButton.AddUniqueDynamic(this, &ThisClass::HandleClickedItemFilterAllButton);

	RightInventoryWidget->OnClicked_FilterTypeButton.RemoveDynamic(
		this,
		&ThisClass::HandleClickedItemFilterTypeButton);
	RightInventoryWidget->OnClicked_FilterTypeButton.AddUniqueDynamic(this, &ThisClass::HandleClickedItemFilterTypeButton);

	RightInventoryWidget->OnDropped_InventorySlot.RemoveDynamic(
		this,
		&ThisClass::HandleDroppedInventorySlot);
	RightInventoryWidget->OnDropped_InventorySlot.AddUniqueDynamic(this, &ThisClass::HandleDroppedInventorySlot);
}

void UInfoUiPresenter::BindStatusWidgetEvents()
{
	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	URightStatusWidget* RightStatusWidget = CurrentInfoWidget ? CurrentInfoWidget->GetRightStatusWidget() : nullptr;
	if (!RightStatusWidget)
	{

		return;
	}

	RightStatusWidget->OnClicked_StatUpButton.RemoveDynamic(this, &ThisClass::HandleClickedStatUpButton);
	RightStatusWidget->OnClicked_StatUpButton.AddUniqueDynamic(this, &ThisClass::HandleClickedStatUpButton);
	RightStatusWidget->OnClicked_StatDownButton.RemoveDynamic(this, &ThisClass::HandleClickedStatDownButton);
	RightStatusWidget->OnClicked_StatDownButton.AddUniqueDynamic(this, &ThisClass::HandleClickedStatDownButton);

}

void UInfoUiPresenter::HandleInventoryChanged()
{
	BeginItemPresentationPreload();
	RefreshLeftEquipmentSlots();
	RefreshInventoryTileView();
	RefreshLeftPandoraSlots();
	RefreshSelectPandoraLoadoutImages();
	RefreshSelectPandoraCompatibilityState();
}

void UInfoUiPresenter::HandlePandoraWeaponLoadoutChanged()
{
	BeginItemPresentationPreload();
	RefreshLeftEquipmentSlots();
	RefreshLeftPandoraSlots();
	RefreshInventoryTileView();
	RefreshSelectPandoraLoadoutImages();
	RefreshSelectPandoraCompatibilityState();
	ReconcileCurrentWeaponLoadoutDirection();
}

void UInfoUiPresenter::HandlePandoraLoadoutChanged()
{

	RefreshLeftPandoraSlots();
	RefreshLeftEquipmentSlots();
	RefreshSelectPandoraLoadoutImages();
	RefreshSelectPandoraCompatibilityState();
}

void UInfoUiPresenter::RefreshInventoryTileView()
{
	const FGameplayTag EquipmentLeftUiTag = GetEquipmentLeftUiTag();
	if (CurrentLeftUiTag != EquipmentLeftUiTag)
	{
		return;
	}

	UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	if (!CurrentInfoWidget)
	{
		return;
	}

	URightInventoryWidget* RightInventoryWidget = CurrentInfoWidget->GetRightInventoryWidget();
	if (!RightInventoryWidget)
	{
		return;
	}

	if (!CurrentInfoWidget->IsInViewport())
	{
		return;
	}

	TArray<UObject*> AllInventoryItems;
	TArray<UObject*> CurrentItemList;

	const APdPlayerState* PdPlayerState = GetCachedPlayerState();
	const UInventoryComponent* InventoryComponent = PdPlayerState ? PdPlayerState->GetInventoryComponent() : nullptr;

	if (InventoryComponent)
	{
		const bool bCanUseTypeFilter = bUseItemTypeFilter
			&& CurrentItemFilterTag.IsValid()
			&& !InventoryComponent->GetFilteredItemMap().IsEmpty();

		if (bCanUseTypeFilter)
		{
			AppendItemListAsObjects(InventoryComponent->GetAllItems(), AllInventoryItems);
			for (UObject* ItemObject : AllInventoryItems)
			{
				UItemInstance* ItemInstance = Cast<UItemInstance>(ItemObject);
				const UItemDefinition* ItemDefinition = IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
				if (ItemDefinition && ItemDefinition->IdTag.MatchesTag(CurrentItemFilterTag))
				{
					CurrentItemList.Add(ItemInstance);
				}
			}


		}
		else
		{
			AppendItemListAsObjects(InventoryComponent->GetAllItems(), AllInventoryItems);
			CurrentItemList = AllInventoryItems;

		}
	}

	const int32 HiddenEquippedItemCount = RemoveEquippedItemsFromInventoryList(AllInventoryItems);
	if (bUseItemTypeFilter)
	{
		RemoveEquippedItemsFromInventoryList(CurrentItemList);
	}
	else
	{
		CurrentItemList = AllInventoryItems;
	}

	ReconcileInventoryDisplaySlots(AllInventoryItems);

	TArray<UObject*> ViewSlotItems;
	BuildInventoryViewSlots(CurrentItemList, ViewSlotItems);
	RightInventoryWidget->SetTileView(ViewSlotItems);
}

int32 UInfoUiPresenter::RemoveEquippedItemsFromInventoryList(TArray<UObject*>& InOutItemList) const
{
	TSet<FGuid> EquippedItemIds;

	const APdPlayerState* PlayerState = GetCachedPlayerState();
	const UInventoryComponent* InventoryComponent =
		PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	const FGameplayTag WeaponItemTypeTag = GetWeaponItemTypeTag();
	const FGameplayTag ConsumableItemTypeTag = GetConsumableItemTypeTag();

	// Non-loadout equipment is still represented by the equipment UI. Weapon and
	// consumable slots must use the replicated InventoryComponent as their source
	// of truth because the UI can be one refresh behind the server-confirmed state.
	const UInfoWidget* CurrentInfoWidget = GetInfoWidget();
	const ULeftEquipmentWidget* LeftEquipmentWidget = CurrentInfoWidget ? CurrentInfoWidget->GetLeftEquipmentWidget() : nullptr;
	if (LeftEquipmentWidget)
	{
		TSet<FGuid> UiEquippedItemIds;
		LeftEquipmentWidget->GetEquippedItemIds(UiEquippedItemIds);
		for (const FGuid ItemId : UiEquippedItemIds)
		{
			const UItemInstance* ItemInstance =
				InventoryComponent ? InventoryComponent->FindItemInstanceById(ItemId) : nullptr;
			const UItemDefinition* ItemDefinition =
				IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
			const bool bIsReplicatedLoadoutItem = ItemDefinition
				&& ItemDefinition->IdTag.IsValid()
				&& ((WeaponItemTypeTag.IsValid()
						&& ItemDefinition->IdTag.MatchesTag(WeaponItemTypeTag))
					|| (ConsumableItemTypeTag.IsValid()
						&& ItemDefinition->IdTag.MatchesTag(ConsumableItemTypeTag)));
			if (!bIsReplicatedLoadoutItem)
			{
				EquippedItemIds.Add(ItemId);
			}
		}
	}

	if (InventoryComponent)
	{
		for (int32 SlotIndex = 0;
			SlotIndex < UInventoryComponent::ConsumableQuickSlotCount;
			++SlotIndex)
		{
			const UItemInstance* QuickSlotItem =
				InventoryComponent->GetConsumableQuickSlotItem(SlotIndex);
			if (IsValid(QuickSlotItem) && QuickSlotItem->GetItemId().IsValid())
			{
				EquippedItemIds.Add(QuickSlotItem->GetItemId());
			}
		}

		for (const EEnum_Direction Direction :
			{ EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right })
		{
			const FGuid WeaponItemId =
				InventoryComponent->GetPandoraWeaponLoadoutItemId(Direction);
			if (WeaponItemId.IsValid())
			{
				EquippedItemIds.Add(WeaponItemId);
			}
		}
	}

	if (EquippedItemIds.IsEmpty() || InOutItemList.IsEmpty())
	{
		return 0;
	}

	const int32 BeforeCount = InOutItemList.Num();
	InOutItemList.RemoveAll([&EquippedItemIds](UObject* ItemObject)
	{
		const UItemInstance* ItemInstance = Cast<UItemInstance>(ItemObject);
		return IsValid(ItemInstance) && EquippedItemIds.Contains(ItemInstance->GetItemId());
	});

	const int32 HiddenCount = BeforeCount - InOutItemList.Num();
	return HiddenCount;
}
