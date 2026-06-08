#include "UI/Widget/LeftEquipmentWidget.h"

#include "Common/ProjectTagConfig.h"
#include "Item/ItemDefinition.h"
#include "Item/ItemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LeftEquipmentWidget)

DEFINE_LOG_CATEGORY_STATIC(LogLeftEquipmentWidget, Log, All);

ULeftEquipmentWidget::ULeftEquipmentWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULeftEquipmentWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	RebuildEquipSlotList();
	RebuildEquipSlotNameList();
	ApplyResolvedEquipTypeTags();
	ApplyEquipSlotNames();
}

void ULeftEquipmentWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RebuildEquipSlotList();
	RebuildEquipSlotNameList();
	ApplyResolvedEquipTypeTags();
	ApplyEquipSlotNames();
	BindEquipSlotCallbacks();

	const FGameplayTag WeaponEquipTypeTag = ResolveEquipTypeTagForSlot(Weapon1);
	const FGameplayTag ConsumableEquipTypeTag = ResolveEquipTypeTagForSlot(QuickSlot1);
	const FGameplayTag ValuableEquipTypeTag = ResolveEquipTypeTagForSlot(ToolSlot1);

	UE_LOG(LogLeftEquipmentWidget, Log, TEXT("[InventoryFilter] LeftEquipment NativeConstruct: widget=%s slotCount=%d weaponTag=%s consumableTag=%s valuableTag=%s"),
		*GetNameSafe(this),
		EquipSlotList.Num(),
		*WeaponEquipTypeTag.ToString(),
		*ConsumableEquipTypeTag.ToString(),
		*ValuableEquipTypeTag.ToString());
}

void ULeftEquipmentWidget::NativeDestruct()
{
	UnbindEquipSlotCallbacks();

	Super::NativeDestruct();
}

void ULeftEquipmentWidget::InitialzeEquipSlots()
{
	for (UEquipSlotWidget* EquipSlot : EquipSlotList)
	{
		if (EquipSlot)
		{
			EquipSlot->SetIsEnabled(true);
			EquipSlot->SetSelected(false);
		}
	}
	SelectedEquipSlot = nullptr;
	bIsSelectedAnyButton = false;
	UE_LOG(LogLeftEquipmentWidget, Log, TEXT("[InventoryFilter] Equipment slots initialized: widget=%s slotCount=%d"),
		*GetNameSafe(this),
		EquipSlotList.Num());
}

void ULeftEquipmentWidget::ToggleActiveEquipSlots(bool bActive)
{
	RebuildEquipSlotList();
	UE_LOG(LogLeftEquipmentWidget, Log, TEXT("[InventoryFilter] Equipment slots active=%s count=%d"),
		bActive ? TEXT("true") : TEXT("false"),
		EquipSlotList.Num());

	for (UEquipSlotWidget* EquipSlot : EquipSlotList)
	{
		if (EquipSlot)
		{
			EquipSlot->SetIsEnabled(bActive);
		}
	}
}

void ULeftEquipmentWidget::SelectEquipSlot(FGameplayTag EquipTypeTag, UEquipSlotWidget* InSelectedEquipSlot)
{
	if (!InSelectedEquipSlot)
	{
		return;
	}

	SelectedEquipSlot = InSelectedEquipSlot;

	UE_LOG(LogLeftEquipmentWidget, Log, TEXT("[InventoryFilter] EquipSlot clicked for filter/clear: widget=%s slot=%s nth=%d tag=%s occupied=%s"),
		*GetNameSafe(this),
		*GetNameSafe(SelectedEquipSlot),
		SelectedEquipSlot ? SelectedEquipSlot->GetNth() : INDEX_NONE,
		*EquipTypeTag.ToString(),
		SelectedEquipSlot && SelectedEquipSlot->HasEquippedItem() ? TEXT("true") : TEXT("false"));

	BroadcastClickedEquipTypeSlot(EquipTypeTag, SelectedEquipSlot, false);

	for (UEquipSlotWidget* EquipSlot : EquipSlotList)
	{
		if (EquipSlot)
		{
			EquipSlot->SetIsEnabled(true);
			EquipSlot->SetSelected(false);
		}
	}

	bIsSelectedAnyButton = false;
}

void ULeftEquipmentWidget::SetWeaponSlotData(const int32 WeaponSlotNumber, UItemInstance* ItemInstance)
{
	UEquipSlotWidget* TargetSlot = nullptr;
	switch (WeaponSlotNumber)
	{
	case 1:
		TargetSlot = Weapon1;
		break;
	case 2:
		TargetSlot = Weapon2;
		break;
	case 3:
		TargetSlot = Weapon3;
		break;
	default:
		break;
	}

	if (TargetSlot)
	{
		UE_LOG(LogLeftEquipmentWidget, Log, TEXT("[EquipSlotFlow] SetWeaponSlotData: widget=%s weaponSlot=%d targetSlot=%s targetNth=%d item=%s definition=%s"),
			*GetNameSafe(this),
			WeaponSlotNumber,
			*GetNameSafe(TargetSlot),
			TargetSlot->GetNth(),
			*GetNameSafe(ItemInstance),
			*GetNameSafe(ItemInstance ? ItemInstance->ItemDefinition.Get() : nullptr));
		TargetSlot->SetData(ItemInstance);
	}
	else
	{
		UE_LOG(LogLeftEquipmentWidget, Warning, TEXT("[EquipSlotFlow] SetWeaponSlotData skipped: widget=%s weaponSlot=%d targetSlot=null item=%s"),
			*GetNameSafe(this),
			WeaponSlotNumber,
			*GetNameSafe(ItemInstance));
	}
}

UEquipSlotWidget* ULeftEquipmentWidget::FindFirstCompatibleEquipSlot(UItemInstance* ItemInstance) const
{
	const UItemDefinition* ItemDefinition = ItemInstance ? ItemInstance->ItemDefinition.Get() : nullptr;
	if (!ItemDefinition || !ItemDefinition->IdTag.IsValid())
	{
		return nullptr;
	}

	UEquipSlotWidget* FirstCompatibleSlot = nullptr;
	for (UEquipSlotWidget* EquipSlot : EquipSlotList)
	{
		if (!EquipSlot)
		{
			continue;
		}

		const FGameplayTag SlotTag = ResolveEquipTypeTagForSlot(EquipSlot);
		if (!SlotTag.IsValid() || !ItemDefinition->IdTag.MatchesTag(SlotTag))
		{
			continue;
		}

		if (!FirstCompatibleSlot)
		{
			FirstCompatibleSlot = EquipSlot;
		}

		if (!EquipSlot->HasEquippedItem())
		{
			UE_LOG(LogLeftEquipmentWidget, Log, TEXT("[CharacterPanelDrop] Found empty compatible equipment slot: widget=%s slot=%s tag=%s item=%s idTag=%s"),
				*GetNameSafe(this),
				*GetNameSafe(EquipSlot),
				*SlotTag.ToString(),
				*GetNameSafe(ItemInstance),
				*ItemDefinition->IdTag.ToString());
			return EquipSlot;
		}
	}

	if (FirstCompatibleSlot)
	{
		UE_LOG(LogLeftEquipmentWidget, Log, TEXT("[CharacterPanelDrop] No empty compatible equipment slot, replacing first compatible slot: widget=%s slot=%s item=%s idTag=%s"),
			*GetNameSafe(this),
			*GetNameSafe(FirstCompatibleSlot),
			*GetNameSafe(ItemInstance),
			*ItemDefinition->IdTag.ToString());
	}
	return FirstCompatibleSlot;
}

UEquipSlotWidget* ULeftEquipmentWidget::FindFirstEquippedCompatibleEquipSlot(UItemInstance* ItemInstance) const
{
	const UItemDefinition* ItemDefinition = ItemInstance ? ItemInstance->ItemDefinition.Get() : nullptr;
	if (!ItemDefinition || !ItemDefinition->IdTag.IsValid())
	{
		return nullptr;
	}

	for (UEquipSlotWidget* EquipSlot : EquipSlotList)
	{
		if (!EquipSlot || !EquipSlot->HasEquippedItem())
		{
			continue;
		}

		const FGameplayTag SlotTag = ResolveEquipTypeTagForSlot(EquipSlot);
		if (SlotTag.IsValid() && ItemDefinition->IdTag.MatchesTag(SlotTag))
		{
			return EquipSlot;
		}
	}

	return nullptr;
}

void ULeftEquipmentWidget::BroadcastClickedEquipTypeSlot(FGameplayTag EquipTypeTag, UEquipSlotWidget* InSelectedEquipSlot, bool bInIsSelectedAnyButton)
{
	UE_LOG(LogLeftEquipmentWidget, Log, TEXT("[InventoryFilter] Broadcast EquipTypeSlot: widget=%s slot=%s nth=%d tag=%s selectedAny=%s"),
		*GetNameSafe(this),
		*GetNameSafe(InSelectedEquipSlot),
		InSelectedEquipSlot ? InSelectedEquipSlot->GetNth() : INDEX_NONE,
		*EquipTypeTag.ToString(),
		bInIsSelectedAnyButton ? TEXT("true") : TEXT("false"));
	OnClicked_EquipTypeSlot.Broadcast(EquipTypeTag, InSelectedEquipSlot, bInIsSelectedAnyButton);
}

void ULeftEquipmentWidget::HandleEquipSlotClicked(UEquipSlotWidget* ItemSlot)
{
	const FGameplayTag ResolvedTag = ResolveEquipTypeTagForSlot(ItemSlot);
	UE_LOG(LogLeftEquipmentWidget, Log, TEXT("[InventoryFilter] EquipSlot clicked: widget=%s slot=%s nth=%d resolvedTag=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ItemSlot),
		ItemSlot ? ItemSlot->GetNth() : INDEX_NONE,
		*ResolvedTag.ToString());
	SelectEquipSlot(ResolvedTag, ItemSlot);
}

void ULeftEquipmentWidget::RebuildEquipSlotList()
{
	EquipSlotList.Reset();
	EquipSlotList.Reserve(19);

	EquipSlotList.Add(HatSlot);
	EquipSlotList.Add(TopSlot);
	EquipSlotList.Add(BottomSlot);
	EquipSlotList.Add(ShoesSlot);
	EquipSlotList.Add(EarringSlot);
	EquipSlotList.Add(NecklaceSlot);
	EquipSlotList.Add(RingSlot);
	EquipSlotList.Add(RuneSlot);
	EquipSlotList.Add(QuickSlot1);
	EquipSlotList.Add(QuickSlot2);
	EquipSlotList.Add(QuickSlot3);
	EquipSlotList.Add(QuickSlot4);
	EquipSlotList.Add(ToolSlot1);
	EquipSlotList.Add(ToolSlot2);
	EquipSlotList.Add(ToolSlot3);
	EquipSlotList.Add(ToolSlot4);
	EquipSlotList.Add(Weapon1);
	EquipSlotList.Add(Weapon2);
	EquipSlotList.Add(Weapon3);

	UE_LOG(LogLeftEquipmentWidget, Verbose, TEXT("[InventoryFilter] RebuildEquipSlotList: count=%d weapon1=%s weapon2=%s weapon3=%s"),
		EquipSlotList.Num(),
		*GetNameSafe(Weapon1),
		*GetNameSafe(Weapon2),
		*GetNameSafe(Weapon3));
}

void ULeftEquipmentWidget::RebuildEquipSlotNameList()
{
	EquipSlotNameList =
		{
			FText::FromString(TEXT("Hat")),
			FText::FromString(TEXT("Top")),
			FText::FromString(TEXT("Bottom")),
			FText::FromString(TEXT("Shoes")),
			FText::FromString(TEXT("Earring")),
			FText::FromString(TEXT("Necklace")),
			FText::FromString(TEXT("Ring")),
			FText::FromString(TEXT("Rune")),
			FText::FromString(TEXT("1")),
			FText::FromString(TEXT("2")),
			FText::FromString(TEXT("3")),
			FText::FromString(TEXT("4")),
			FText::FromString(TEXT("1")),
			FText::FromString(TEXT("2")),
			FText::FromString(TEXT("3")),
			FText::FromString(TEXT("4")),
			FText::FromString(TEXT("First\r\nWeapon")),
			FText::FromString(TEXT("Second\r\nWeapon")),
			FText::FromString(TEXT("Third\r\nWeapon")),
		};
}

void ULeftEquipmentWidget::ApplyEquipSlotNames()
{
	const int32 SlotCount = FMath::Min(EquipSlotList.Num(), EquipSlotNameList.Num());

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		if (EquipSlotList[SlotIndex])
		{
			EquipSlotList[SlotIndex]->SetText(EquipSlotNameList[SlotIndex]);
		}
	}
}

void ULeftEquipmentWidget::ApplyResolvedEquipTypeTags()
{
	for (UEquipSlotWidget* EquipSlot : EquipSlotList)
	{
		if (EquipSlot)
		{
			EquipSlot->SetResolvedEquipTypeTag(ResolveEquipTypeTagForSlot(EquipSlot));
		}
	}
}

void ULeftEquipmentWidget::BindEquipSlotCallbacks()
{
	for (UEquipSlotWidget* EquipSlot : EquipSlotList)
	{
		if (EquipSlot)
		{
			EquipSlot->OnClicked_EquipSlot.AddUniqueDynamic(this, &ThisClass::HandleEquipSlotClicked);
			EquipSlot->OnDroppedItem_EquipSlot.AddUniqueDynamic(this, &ThisClass::HandleEquipSlotItemDropped);
		}
	}
	UE_LOG(LogLeftEquipmentWidget, Log, TEXT("[InventoryFilter] Equip slot callbacks bound: count=%d"),
		EquipSlotList.Num());
}

void ULeftEquipmentWidget::UnbindEquipSlotCallbacks()
{
	for (UEquipSlotWidget* EquipSlot : EquipSlotList)
	{
		if (EquipSlot)
		{
			EquipSlot->OnClicked_EquipSlot.RemoveDynamic(this, &ThisClass::HandleEquipSlotClicked);
			EquipSlot->OnDroppedItem_EquipSlot.RemoveDynamic(this, &ThisClass::HandleEquipSlotItemDropped);
		}
	}
}

void ULeftEquipmentWidget::HandleEquipSlotItemDropped(UEquipSlotWidget* ItemSlot, UItemInstance* ItemInstance)
{
	const FGameplayTag ResolvedTag = ResolveEquipTypeTagForSlot(ItemSlot);
	UE_LOG(LogLeftEquipmentWidget, Log, TEXT("[EquipSlotDragDrop] Item dropped on left equipment slot: widget=%s slot=%s nth=%d resolvedTag=%s item=%s definition=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ItemSlot),
		ItemSlot ? ItemSlot->GetNth() : INDEX_NONE,
		ResolvedTag.IsValid() ? *ResolvedTag.ToString() : TEXT("None"),
		*GetNameSafe(ItemInstance),
		*GetNameSafe(ItemInstance ? ItemInstance->ItemDefinition.Get() : nullptr));
	OnDroppedItem_EquipTypeSlot.Broadcast(ResolvedTag, ItemSlot, ItemInstance);
}

FGameplayTag ULeftEquipmentWidget::ResolveEquipTypeTagForSlot(const UEquipSlotWidget* ItemSlot) const
{
	if (ItemSlot && ItemSlot->GetEquipTypeTag().IsValid())
	{
		UE_LOG(LogLeftEquipmentWidget, Log, TEXT("[EquipSlotFlow] ResolveEquipTypeTagForSlot using slot override: widget=%s slot=%s nth=%d tag=%s"),
			*GetNameSafe(this),
			*GetNameSafe(ItemSlot),
			ItemSlot->GetNth(),
			*ItemSlot->GetEquipTypeTag().ToString());
		return ItemSlot->GetEquipTypeTag();
	}

	if (ItemSlot == HatSlot)
	{
		return UProjectTagConfig::Get(this)->GetItemHatEquipTypeTag();
	}

	if (ItemSlot == TopSlot)
	{
		return UProjectTagConfig::Get(this)->GetItemTopEquipTypeTag();
	}

	if (ItemSlot == BottomSlot)
	{
		return UProjectTagConfig::Get(this)->GetItemBottomEquipTypeTag();
	}

	if (ItemSlot == ShoesSlot)
	{
		return UProjectTagConfig::Get(this)->GetItemShoesEquipTypeTag();
	}

	if (ItemSlot == EarringSlot)
	{
		return UProjectTagConfig::Get(this)->GetItemEarringEquipTypeTag();
	}

	if (ItemSlot == NecklaceSlot)
	{
		return UProjectTagConfig::Get(this)->GetItemNecklaceEquipTypeTag();
	}

	if (ItemSlot == RingSlot)
	{
		return UProjectTagConfig::Get(this)->GetItemRingEquipTypeTag();
	}

	if (ItemSlot == RuneSlot)
	{
		return UProjectTagConfig::Get(this)->GetItemRuneEquipTypeTag();
	}

	if (ItemSlot == QuickSlot1 || ItemSlot == QuickSlot2 || ItemSlot == QuickSlot3 || ItemSlot == QuickSlot4)
	{
		return UProjectTagConfig::Get(this)->GetItemConsumableTypeTag();
	}

	if (ItemSlot == ToolSlot1 || ItemSlot == ToolSlot2 || ItemSlot == ToolSlot3 || ItemSlot == ToolSlot4)
	{
		return UProjectTagConfig::Get(this)->GetItemValuableTypeTag();
	}

	if (ItemSlot == Weapon1 || ItemSlot == Weapon2 || ItemSlot == Weapon3)
	{
		return UProjectTagConfig::Get(this)->GetItemWeaponTypeTag();
	}

	return FGameplayTag();
}
