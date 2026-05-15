#include "UI/Widget/LeftEquipmentWidget.h"

#include "Common/ProjectTagConfig.h"

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
	ApplyEquipSlotNames();
}

void ULeftEquipmentWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RebuildEquipSlotList();
	RebuildEquipSlotNameList();
	ApplyEquipSlotNames();
	BindEquipSlotCallbacks();

	const FGameplayTag WeaponEquipTypeTag = ResolveEquipTypeTagForSlot(Weapon1);
	const FGameplayTag ConsumableEquipTypeTag = ResolveEquipTypeTagForSlot(QuickSlot1);
	const FGameplayTag ValuableEquipTypeTag = ResolveEquipTypeTagForSlot(ToolSlot);

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
	ToggleActiveEquipSlots(true);
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

	UE_LOG(LogLeftEquipmentWidget, Log, TEXT("[InventoryFilter] SelectEquipSlot: widget=%s slot=%s nth=%d tag=%s selectedAnyBefore=%s"),
		*GetNameSafe(this),
		*GetNameSafe(SelectedEquipSlot),
		SelectedEquipSlot ? SelectedEquipSlot->GetNth() : INDEX_NONE,
		*EquipTypeTag.ToString(),
		bIsSelectedAnyButton ? TEXT("true") : TEXT("false"));

	BroadcastClickedEquipTypeSlot(EquipTypeTag, SelectedEquipSlot, bIsSelectedAnyButton);
	ToggleActiveEquipSlots(bIsSelectedAnyButton);

	SelectedEquipSlot->SetIsEnabled(true);
	bIsSelectedAnyButton = !bIsSelectedAnyButton;
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
	EquipSlotList.Reserve(16);

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
	EquipSlotList.Add(ToolSlot);
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
			FText::FromString(TEXT("Tool")),
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

void ULeftEquipmentWidget::BindEquipSlotCallbacks()
{
	for (UEquipSlotWidget* EquipSlot : EquipSlotList)
	{
		if (EquipSlot)
		{
			EquipSlot->OnClicked_EquipSlot.AddUniqueDynamic(this, &ThisClass::HandleEquipSlotClicked);
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
		}
	}
}

FGameplayTag ULeftEquipmentWidget::ResolveEquipTypeTagForSlot(const UEquipSlotWidget* ItemSlot) const
{
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

	if (ItemSlot == ToolSlot)
	{
		return UProjectTagConfig::Get(this)->GetItemValuableTypeTag();
	}

	if (ItemSlot == Weapon1 || ItemSlot == Weapon2 || ItemSlot == Weapon3)
	{
		return UProjectTagConfig::Get(this)->GetItemWeaponTypeTag();
	}

	return FGameplayTag();
}
