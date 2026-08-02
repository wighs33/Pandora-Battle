#include "UI/Widget/LeftEquipmentWidget.h"

#include "Data/ContentDataSubsystem.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Definition/Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Definition/Pandora/PandoraDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LeftEquipmentWidget)

ULeftEquipmentWidget::ULeftEquipmentWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PandoraAxeIcon = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/Game/UI/Asset/Images/ItemIcons/weapon-axe.weapon-axe")));
	PandoraBowIcon = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/Game/UI/Asset/Images/ItemIcons/weapon-bow.weapon-bow")));
	PandoraDaggerIcon = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/Game/UI/Asset/Images/ItemIcons/weapon-dagger.weapon-dagger")));
	PandoraGreatswordIcon = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/Game/UI/Asset/Images/ItemIcons/weapon-greatsword.weapon-greatsword")));
	PandoraSwordIcon = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/Game/UI/Asset/Images/ItemIcons/weapon-sword.weapon-sword")));
	PandoraGunIcon = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/Game/UI/Asset/Images/ItemIcons/weapoon-gun.weapoon-gun")));
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
	BeginPandoraWeaponIconPreload();

	const FGameplayTag WeaponEquipTypeTag = ResolveEquipTypeTagForSlot(Weapon1);
	const FGameplayTag ConsumableEquipTypeTag = ResolveEquipTypeTagForSlot(QuickSlot1);
	const FGameplayTag ValuableEquipTypeTag = ResolveEquipTypeTagForSlot(ToolSlot1);


}

void ULeftEquipmentWidget::NativeDestruct()
{
	ReleasePandoraWeaponIconPreload();
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

void ULeftEquipmentWidget::SetWeaponSlotPandoraRequirement(
	const int32 WeaponSlotNumber,
	const UPandoraDefinition* PandoraDefinition)
{
	if (WeaponSlotNumber >= 1 && WeaponSlotNumber <= 3)
	{
		CachedWeaponSlotPandoraRequirements.SetNum(3);
		CachedWeaponSlotPandoraRequirements[WeaponSlotNumber - 1] =
			const_cast<UPandoraDefinition*>(PandoraDefinition);
	}

	if (UEquipSlotWidget* TargetSlot = GetWeaponSlot(WeaponSlotNumber))
	{
		TargetSlot->SetPandoraWeaponRequirementIcon(
			ResolvePandoraWeaponRequirementIcon(PandoraDefinition),
			PandoraWeaponRequirementOpacity);
	}
}

void ULeftEquipmentWidget::BeginPandoraWeaponIconPreload()
{
	ReleasePandoraWeaponIconPreload();
	const int32 PreloadGeneration = ++PandoraWeaponIconPreloadGeneration;

	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		return;
	}

	TArray<FSoftObjectPath> IconPaths;
	const TSoftObjectPtr<UTexture2D> Icons[] = {
		PandoraAxeIcon,
		PandoraBowIcon,
		PandoraDaggerIcon,
		PandoraGreatswordIcon,
		PandoraSwordIcon,
		PandoraGunIcon
	};
	for (const TSoftObjectPtr<UTexture2D>& Icon : Icons)
	{
		if (!Icon.IsNull())
		{
			IconPaths.Add(Icon.ToSoftObjectPath());
		}
	}

	PandoraWeaponIconPreloadHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
			IconPaths,
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this, PreloadGeneration]()
				{
					if (PreloadGeneration == PandoraWeaponIconPreloadGeneration)
					{
						RefreshCachedPandoraWeaponRequirements();
					}
				}));
}

void ULeftEquipmentWidget::ReleasePandoraWeaponIconPreload()
{
	++PandoraWeaponIconPreloadGeneration;
	if (PandoraWeaponIconPreloadHandle.IsValid())
	{
		PandoraWeaponIconPreloadHandle->CancelHandle();
		PandoraWeaponIconPreloadHandle->ReleaseHandle();
		PandoraWeaponIconPreloadHandle.Reset();
	}
}

void ULeftEquipmentWidget::RefreshCachedPandoraWeaponRequirements()
{
	for (int32 SlotIndex = 0; SlotIndex < CachedWeaponSlotPandoraRequirements.Num(); ++SlotIndex)
	{
		if (UEquipSlotWidget* TargetSlot = GetWeaponSlot(SlotIndex + 1))
		{
			TargetSlot->SetPandoraWeaponRequirementIcon(
				ResolvePandoraWeaponRequirementIcon(
					CachedWeaponSlotPandoraRequirements[SlotIndex]),
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

void ULeftEquipmentWidget::GetEquippedItemIds(TSet<FGuid>& OutItemIds, const FGameplayTag ExcludedEquipTypeRootTag) const
{
	for (UEquipSlotWidget* EquipSlot : EquipSlotList)
	{
		if (!EquipSlot)
		{
			continue;
		}

		const FGameplayTag SlotTag = ResolveEquipTypeTagForSlot(EquipSlot);
		if (ExcludedEquipTypeRootTag.IsValid() && SlotTag.IsValid() && SlotTag.MatchesTag(ExcludedEquipTypeRootTag))
		{
			continue;
		}

		UItemInstance* ItemInstance = EquipSlot->GetItemInstance();
		if (!IsValid(ItemInstance))
		{
			continue;
		}

		const FGuid ItemId = ItemInstance->GetOrCreateItemId();
		if (ItemId.IsValid())
		{
			OutItemIds.Add(ItemId);

		}
	}
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

UTexture2D* ULeftEquipmentWidget::ResolvePandoraWeaponRequirementIcon(
	const UPandoraDefinition* PandoraDefinition) const
{
	if (!PandoraDefinition)
	{
		return nullptr;
	}

	const FGameplayTagContainer& RequiredWeaponTags = PandoraDefinition->ActivatableWeaponTags;
	const auto HasWeaponTag = [&RequiredWeaponTags](const TCHAR* WeaponTagName)
	{
		const FGameplayTag WeaponTag =
			FGameplayTag::RequestGameplayTag(FName(WeaponTagName), false);
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

	if (HasWeaponTag(TEXT("Item.Weapon.Axe")))
	{
		return PandoraAxeIcon.Get();
	}
	if (HasWeaponTag(TEXT("Item.Weapon.Bow")))
	{
		return PandoraBowIcon.Get();
	}
	if (HasWeaponTag(TEXT("Item.Weapon.Dagger")))
	{
		return PandoraDaggerIcon.Get();
	}
	if (HasWeaponTag(TEXT("Item.Weapon.GreatSword")))
	{
		return PandoraGreatswordIcon.Get();
	}
	if (HasWeaponTag(TEXT("Item.Weapon.Sword")))
	{
		return PandoraSwordIcon.Get();
	}
	if (HasWeaponTag(TEXT("Item.Weapon.Gun")))
	{
		return PandoraGunIcon.Get();
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
