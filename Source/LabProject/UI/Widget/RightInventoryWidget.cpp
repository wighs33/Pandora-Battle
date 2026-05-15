#include "UI/Widget/RightInventoryWidget.h"

#include "Common/ProjectTagConfig.h"
#include "Components/Button.h"
#include "Components/TileView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RightInventoryWidget)

DEFINE_LOG_CATEGORY_STATIC(LogRightInventoryWidget, Log, All);

URightInventoryWidget::URightInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URightInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	const FGameplayTag WeaponTypeTag = GetWeaponTypeTag();
	const FGameplayTag EquipmentTypeTag = GetEquipmentTypeTag();
	const FGameplayTag ValuableTypeTag = GetValuableTypeTag();
	const FGameplayTag ConsumableTypeTag = GetConsumableTypeTag();

	UE_LOG(LogRightInventoryWidget, Log, TEXT("[InventoryFilter] RightInventory NativeConstruct: widget=%s all=%s weapon=%s equipment=%s valuable=%s consumable=%s tile=%s tags=(weapon:%s equipment:%s valuable:%s consumable:%s)"),
		*GetNameSafe(this),
		*GetNameSafe(AllButton),
		*GetNameSafe(WeaponButton),
		*GetNameSafe(EquipmentButton),
		*GetNameSafe(ValuableButton),
		*GetNameSafe(ConsumableButton),
		*GetNameSafe(TileView),
		*WeaponTypeTag.ToString(),
		*EquipmentTypeTag.ToString(),
		*ValuableTypeTag.ToString(),
		*ConsumableTypeTag.ToString());

	if (AllButton)
	{
		AllButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnAllButtonClicked);
	}

	if (WeaponButton)
	{
		WeaponButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnWeaponButtonClicked);
	}

	if (EquipmentButton)
	{
		EquipmentButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnEquipmentButtonClicked);
	}

	if (ValuableButton)
	{
		ValuableButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnValuableButtonClicked);
	}

	if (ConsumableButton)
	{
		ConsumableButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnConsumableButtonClicked);
	}

	RebuildFilterButtonList();
}

void URightInventoryWidget::NativeDestruct()
{
	if (AllButton)
	{
		AllButton->OnClicked.RemoveDynamic(this, &ThisClass::OnAllButtonClicked);
	}

	if (WeaponButton)
	{
		WeaponButton->OnClicked.RemoveDynamic(this, &ThisClass::OnWeaponButtonClicked);
	}

	if (EquipmentButton)
	{
		EquipmentButton->OnClicked.RemoveDynamic(this, &ThisClass::OnEquipmentButtonClicked);
	}

	if (ValuableButton)
	{
		ValuableButton->OnClicked.RemoveDynamic(this, &ThisClass::OnValuableButtonClicked);
	}

	if (ConsumableButton)
	{
		ConsumableButton->OnClicked.RemoveDynamic(this, &ThisClass::OnConsumableButtonClicked);
	}

	Super::NativeDestruct();
}

void URightInventoryWidget::SelectAllFilter()
{
	UE_LOG(LogRightInventoryWidget, Log, TEXT("[InventoryFilter] RightInventory all filter clicked: widget=%s"),
		*GetNameSafe(this));
	OnClicked_FilterAllButton.Broadcast();
}

void URightInventoryWidget::SelectTypeFilter(FGameplayTag TypeTag)
{
	UE_LOG(LogRightInventoryWidget, Log, TEXT("[InventoryFilter] RightInventory type filter clicked: widget=%s tag=%s valid=%s"),
		*GetNameSafe(this),
		*TypeTag.ToString(),
		TypeTag.IsValid() ? TEXT("true") : TEXT("false"));
	OnClicked_FilterTypeButton.Broadcast(TypeTag);
}

void URightInventoryWidget::ToggleActiveFiliterButtons(bool bActive)
{
	UE_LOG(LogRightInventoryWidget, Log, TEXT("[InventoryFilter] RightInventory filter buttons active=%s count=%d"),
		bActive ? TEXT("true") : TEXT("false"),
		FilterButtonList.Num());

	for (UButton* Button : FilterButtonList)
	{
		if (Button)
		{
			Button->SetIsEnabled(bActive);
		}
	}
}

void URightInventoryWidget::SetTileView(const TArray<UObject*>& InListItems)
{
	if (!TileView)
	{
		UE_LOG(LogRightInventoryWidget, Warning, TEXT("[InventoryFilter] SetTileView skipped: TileView is null. requestedCount=%d"),
			InListItems.Num());
		return;
	}

	TileView->ClearListItems();

	for (UObject* ListItem : InListItems)
	{
		if (ListItem)
		{
			TileView->AddItem(ListItem);
		}
	}

	UE_LOG(LogRightInventoryWidget, Log, TEXT("[InventoryFilter] SetTileView applied: widget=%s requestedCount=%d tileCount=%d"),
		*GetNameSafe(this),
		InListItems.Num(),
		TileView->GetNumItems());
}

void URightInventoryWidget::ClearTileViewItemClicked()
{
	if (TileView)
	{
		TileView->OnItemClicked().Clear();
	}
}

void URightInventoryWidget::OnAllButtonClicked()
{
	SelectAllFilter();
}

void URightInventoryWidget::OnWeaponButtonClicked()
{
	const FGameplayTag WeaponTypeTag = GetWeaponTypeTag();
	UE_LOG(LogRightInventoryWidget, Log, TEXT("[InventoryFilter] Weapon button clicked: tag=%s"),
		*WeaponTypeTag.ToString());
	SelectTypeFilter(WeaponTypeTag);
}

void URightInventoryWidget::OnEquipmentButtonClicked()
{
	const FGameplayTag EquipmentTypeTag = GetEquipmentTypeTag();
	UE_LOG(LogRightInventoryWidget, Log, TEXT("[InventoryFilter] Equipment button clicked: tag=%s"),
		*EquipmentTypeTag.ToString());
	SelectTypeFilter(EquipmentTypeTag);
}

void URightInventoryWidget::OnValuableButtonClicked()
{
	const FGameplayTag ValuableTypeTag = GetValuableTypeTag();
	UE_LOG(LogRightInventoryWidget, Log, TEXT("[InventoryFilter] Valuable button clicked: tag=%s"),
		*ValuableTypeTag.ToString());
	SelectTypeFilter(ValuableTypeTag);
}

void URightInventoryWidget::OnConsumableButtonClicked()
{
	const FGameplayTag ConsumableTypeTag = GetConsumableTypeTag();
	UE_LOG(LogRightInventoryWidget, Log, TEXT("[InventoryFilter] Consumable button clicked: tag=%s"),
		*ConsumableTypeTag.ToString());
	SelectTypeFilter(ConsumableTypeTag);
}

void URightInventoryWidget::RebuildFilterButtonList()
{
	FilterButtonList.Reset();
	FilterButtonList.Reserve(5);

	FilterButtonList.Add(AllButton);
	FilterButtonList.Add(WeaponButton);
	FilterButtonList.Add(EquipmentButton);
	FilterButtonList.Add(ConsumableButton);
	FilterButtonList.Add(ValuableButton);

	UE_LOG(LogRightInventoryWidget, Log, TEXT("[InventoryFilter] RebuildFilterButtonList: all=%s weapon=%s equipment=%s consumable=%s valuable=%s count=%d"),
		*GetNameSafe(AllButton),
		*GetNameSafe(WeaponButton),
		*GetNameSafe(EquipmentButton),
		*GetNameSafe(ConsumableButton),
		*GetNameSafe(ValuableButton),
		FilterButtonList.Num());
}

FGameplayTag URightInventoryWidget::GetWeaponTypeTag() const
{
	return UProjectTagConfig::Get(this)->GetItemWeaponTypeTag();
}

FGameplayTag URightInventoryWidget::GetEquipmentTypeTag() const
{
	return UProjectTagConfig::Get(this)->GetItemEquipmentTypeTag();
}

FGameplayTag URightInventoryWidget::GetValuableTypeTag() const
{
	return UProjectTagConfig::Get(this)->GetItemValuableTypeTag();
}

FGameplayTag URightInventoryWidget::GetConsumableTypeTag() const
{
	return UProjectTagConfig::Get(this)->GetItemConsumableTypeTag();
}
