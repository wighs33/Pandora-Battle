#include "UI/Info/Item/LeftEquipmentWidget.h"

#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Definition/Common/ProjectTagDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Definition/Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Definition/Pandora/PandoraDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LeftEquipmentWidget)

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
	BeginPandoraWeaponIconPreload();

	const FGameplayTag WeaponEquipTypeTag = ResolveEquipTypeTagForSlot(Weapon1);
	const FGameplayTag ConsumableEquipTypeTag = ResolveEquipTypeTagForSlot(QuickSlot1);
	const FGameplayTag ValuableEquipTypeTag = ResolveEquipTypeTagForSlot(ToolSlot1);
}

void ULeftEquipmentWidget::NativeDestruct()
{
	PandoraWeaponIconLease.Reset();
	CachedWeaponSlotPandoraRequirements.Reset();
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
}

void ULeftEquipmentWidget::ToggleActiveEquipSlots(bool bActive)
{
	RebuildEquipSlotList();

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
	UEquipSlotWidget* TargetSlot = GetWeaponSlot(WeaponSlotNumber);

	if (TargetSlot)
	{
		TargetSlot->SetData(ItemInstance);
	}
}

void ULeftEquipmentWidget::SetWeaponSlotPandoraRequirement(const int32 WeaponSlotNumber,
	const UPandoraDefinition* PandoraDefinition)
{
	if (WeaponSlotNumber >= 1 && WeaponSlotNumber <= 3)
	{
		CachedWeaponSlotPandoraRequirements.SetNum(3);
		CachedWeaponSlotPandoraRequirements[WeaponSlotNumber - 1] = PandoraDefinition;
	}

	if (UEquipSlotWidget* TargetSlot = GetWeaponSlot(WeaponSlotNumber))
	{
		TargetSlot->SetPandoraWeaponRequirementIcon(ResolvePandoraWeaponRequirementIcon(PandoraDefinition),
			PandoraWeaponRequirementOpacity);
	}
}

void ULeftEquipmentWidget::BeginPandoraWeaponIconPreload()
{
	PandoraWeaponIconLease.Reset();

	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		return;
	}

	TArray<FSoftObjectPath> IconPaths;
	for (const TPair<FGameplayTag, TSoftObjectPtr<UTexture2D>>& WeaponIcon : PandoraWeaponIcons)
	{
		if (!WeaponIcon.Value.IsNull())
		{
			IconPaths.Add(WeaponIcon.Value.ToSoftObjectPath());
		}
	}

	PandoraWeaponIconLease = ContentSubsystem->AcquireContent(IconPaths,
		FSimpleDelegate::CreateUObject(this, &ThisClass::RefreshCachedPandoraWeaponRequirements));
}

void ULeftEquipmentWidget::RefreshCachedPandoraWeaponRequirements()
{
	for (int32 SlotIndex = 0; SlotIndex < CachedWeaponSlotPandoraRequirements.Num(); ++SlotIndex)
	{
		if (UEquipSlotWidget* TargetSlot = GetWeaponSlot(SlotIndex + 1))
		{
			TargetSlot->SetPandoraWeaponRequirementIcon(
				ResolvePandoraWeaponRequirementIcon(CachedWeaponSlotPandoraRequirements[SlotIndex]),
				PandoraWeaponRequirementOpacity);
		}
	}
}

void ULeftEquipmentWidget::SetConsumableQuickSlotData(const int32 QuickSlotNumber, UItemInstance* ItemInstance)
{
	UEquipSlotWidget* TargetSlot = nullptr;
	switch (QuickSlotNumber)
	{
	case 1:
		TargetSlot = QuickSlot1;
		break;
	case 2:
		TargetSlot = QuickSlot2;
		break;
	case 3:
		TargetSlot = QuickSlot3;
		break;
	case 4:
		TargetSlot = QuickSlot4;
		break;
	default:
		break;
	}

	if (TargetSlot)
	{
		TargetSlot->SetData(ItemInstance);
	}
}

void ULeftEquipmentWidget::SetEquipmentSlotData(const FGameplayTag EquipTypeTag, UItemInstance* ItemInstance)
{
	const UProjectTagDefinition* TagConfig = UProjectTagDefinition::Get(this);
	UEquipSlotWidget* TargetSlot = nullptr;
	if (EquipTypeTag == TagConfig->GetItemHatEquipTypeTag())
	{
		TargetSlot = HatSlot;
	}
	else if (EquipTypeTag == TagConfig->GetItemTopEquipTypeTag())
	{
		TargetSlot = TopSlot;
	}
	else if (EquipTypeTag == TagConfig->GetItemBottomEquipTypeTag())
	{
		TargetSlot = BottomSlot;
	}
	else if (EquipTypeTag == TagConfig->GetItemShoesEquipTypeTag())
	{
		TargetSlot = ShoesSlot;
	}
	else if (EquipTypeTag == TagConfig->GetItemEarringEquipTypeTag())
	{
		TargetSlot = EarringSlot;
	}
	else if (EquipTypeTag == TagConfig->GetItemNecklaceEquipTypeTag())
	{
		TargetSlot = NecklaceSlot;
	}
	else if (EquipTypeTag == TagConfig->GetItemRingEquipTypeTag())
	{
		TargetSlot = RingSlot;
	}
	else if (EquipTypeTag == TagConfig->GetItemRuneEquipTypeTag())
	{
		TargetSlot = RuneSlot;
	}

	if (TargetSlot)
	{
		TargetSlot->SetData(ItemInstance);
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
			return EquipSlot;
		}
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
	OnClicked_EquipTypeSlot.Broadcast(EquipTypeTag, InSelectedEquipSlot, bInIsSelectedAnyButton);
}

void ULeftEquipmentWidget::HandleEquipSlotClicked(UEquipSlotWidget* ItemSlot)
{
	const FGameplayTag ResolvedTag = ResolveEquipTypeTagForSlot(ItemSlot);

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
}

void ULeftEquipmentWidget::RebuildEquipSlotNameList()
{
	EquipSlotNameList = {
			MenuText(TEXT("Info.Hat")),
			MenuText(TEXT("Info.Top")),
			MenuText(TEXT("Info.Bottom")),
			MenuText(TEXT("Info.Shoes")),
			MenuText(TEXT("Info.Earring")),
			MenuText(TEXT("Info.Necklace")),
			MenuText(TEXT("Info.Ring")),
			MenuText(TEXT("Info.Rune")),
			INVTEXT("1"),
			INVTEXT("2"),
			INVTEXT("3"),
			INVTEXT("4"),
			INVTEXT("1"),
			INVTEXT("2"),
			INVTEXT("3"),
			INVTEXT("4"),
			MenuText(TEXT("Info.FirstWeapon")),
			MenuText(TEXT("Info.SecondWeapon")),
			MenuText(TEXT("Info.ThirdWeapon")),
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

	OnDroppedItem_EquipTypeSlot.Broadcast(ResolvedTag, ItemSlot, ItemInstance);
}

UEquipSlotWidget* ULeftEquipmentWidget::GetWeaponSlot(const int32 WeaponSlotNumber) const
{
	switch (WeaponSlotNumber)
	{
	case 1:
		return Weapon1;
	case 2:
		return Weapon2;
	case 3:
		return Weapon3;
	default:
		return nullptr;
	}
}

UTexture2D* ULeftEquipmentWidget::ResolvePandoraWeaponRequirementIcon(const UPandoraDefinition* PandoraDefinition) const
{
	if (!PandoraDefinition)
	{
		return nullptr;
	}

	const FGameplayTagContainer& RequiredWeaponTags = PandoraDefinition->GetActivatableWeaponTags();
	const auto HasWeaponTag = [&RequiredWeaponTags](const FGameplayTag& WeaponTag)
	{
		if (!WeaponTag.IsValid())
		{
			return false;
		}

		for (const FGameplayTag& RequiredWeaponTag : RequiredWeaponTags)
		{
			if (RequiredWeaponTag == WeaponTag || RequiredWeaponTag.MatchesTag(WeaponTag))
			{
				return true;
			}
		}

		return false;
	};

	for (const TPair<FGameplayTag, TSoftObjectPtr<UTexture2D>>& WeaponIcon : PandoraWeaponIcons)
	{
		if (HasWeaponTag(WeaponIcon.Key))
		{
			return WeaponIcon.Value.Get();
		}
	}

	return nullptr;
}

FGameplayTag ULeftEquipmentWidget::ResolveEquipTypeTagForSlot(const UEquipSlotWidget* ItemSlot) const
{
	if (ItemSlot && ItemSlot->GetEquipTypeTag().IsValid())
	{
		return ItemSlot->GetEquipTypeTag();
	}

	if (ItemSlot == HatSlot)
	{
		return UProjectTagDefinition::Get(this)->GetItemHatEquipTypeTag();
	}

	if (ItemSlot == TopSlot)
	{
		return UProjectTagDefinition::Get(this)->GetItemTopEquipTypeTag();
	}

	if (ItemSlot == BottomSlot)
	{
		return UProjectTagDefinition::Get(this)->GetItemBottomEquipTypeTag();
	}

	if (ItemSlot == ShoesSlot)
	{
		return UProjectTagDefinition::Get(this)->GetItemShoesEquipTypeTag();
	}

	if (ItemSlot == EarringSlot)
	{
		return UProjectTagDefinition::Get(this)->GetItemEarringEquipTypeTag();
	}

	if (ItemSlot == NecklaceSlot)
	{
		return UProjectTagDefinition::Get(this)->GetItemNecklaceEquipTypeTag();
	}

	if (ItemSlot == RingSlot)
	{
		return UProjectTagDefinition::Get(this)->GetItemRingEquipTypeTag();
	}

	if (ItemSlot == RuneSlot)
	{
		return UProjectTagDefinition::Get(this)->GetItemRuneEquipTypeTag();
	}

	if (ItemSlot == QuickSlot1 || ItemSlot == QuickSlot2 || ItemSlot == QuickSlot3 || ItemSlot == QuickSlot4)
	{
		return UProjectTagDefinition::Get(this)->GetItemConsumableTypeTag();
	}

	if (ItemSlot == ToolSlot1 || ItemSlot == ToolSlot2 || ItemSlot == ToolSlot3 || ItemSlot == ToolSlot4)
	{
		return UProjectTagDefinition::Get(this)->GetItemValuableTypeTag();
	}

	if (ItemSlot == Weapon1 || ItemSlot == Weapon2 || ItemSlot == Weapon3)
	{
		return UProjectTagDefinition::Get(this)->GetItemWeaponTypeTag();
	}

	return FGameplayTag();
}

void ULeftEquipmentWidget::OnMenuLanguageChanged()
{
	RebuildEquipSlotNameList();
	ApplyEquipSlotNames();
}
