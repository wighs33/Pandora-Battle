#include "UI/Widget/RightInventoryWidget.h"

#include "Definition/Common/ProjectTagConfig.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TileView.h"
#include "Definition/Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "UI/Widget/InventorySlotViewData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RightInventoryWidget)

URightInventoryWidget::URightInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URightInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyWidgetDefinitionSettings();

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

	if (Btn_Search)
	{
		Btn_Search->OnClicked.AddUniqueDynamic(this, &ThisClass::OnSearchButtonClicked);
	}

	RebuildFilterButtonList();
	FilterButtonHighlightState.Initialize(FilterButtonList, AllButton, SelectedFilterAccentColor);
}

void URightInventoryWidget::NativeDestruct()
{
	FilterButtonHighlightState.Reset();

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

	if (Btn_Search)
	{
		Btn_Search->OnClicked.RemoveDynamic(this, &ThisClass::OnSearchButtonClicked);
	}

	Super::NativeDestruct();
}

void URightInventoryWidget::SelectAllFilter()
{
	FilterButtonHighlightState.Select(AllButton, SelectedFilterAccentColor);
	OnClicked_FilterAllButton.Broadcast();
}

void URightInventoryWidget::SelectTypeFilter(FGameplayTag TypeTag)
{
	if (UButton* SelectedButton = ResolveFilterButton(TypeTag))
	{
		FilterButtonHighlightState.Select(SelectedButton, SelectedFilterAccentColor);
	}

	OnClicked_FilterTypeButton.Broadcast(TypeTag);
}

void URightInventoryWidget::ToggleActiveFiliterButtons(bool bActive)
{


	for (UButton* Button : FilterButtonList)
	{
		if (Button)
		{
			Button->SetIsEnabled(bActive);
		}
	}
}

void URightInventoryWidget::ResetFilterHighlightToAll()
{
	FilterButtonHighlightState.Select(AllButton, SelectedFilterAccentColor);
}

void URightInventoryWidget::SetTileView(const TArray<UObject*>& InListItems)
{
	CachedSourceListItems.Reset();
	CachedSourceListItems.Reserve(InListItems.Num());
	for (UObject* ListItem : InListItems)
	{
		CachedSourceListItems.Add(ListItem);
	}

	RebuildTileViewFromCachedSourceItems();
}

void URightInventoryWidget::ClearTileViewItemClicked()
{
	if (TileView)
	{
		TileView->OnItemClicked().Clear();
	}
}

void URightInventoryWidget::SetInventorySlotCount(const int32 InInventorySlotCount)
{
	const int32 NewInventorySlotCount = FMath::Max(InInventorySlotCount, 0);
	if (InventorySlotCount == NewInventorySlotCount)
	{
		return;
	}


	InventorySlotCount = NewInventorySlotCount;
}

void URightInventoryWidget::BroadcastDroppedInventorySlot(const int32 SourceSlotIndex, const int32 TargetSlotIndex, UItemInstance* SourceItem)
{

	OnDropped_InventorySlot.Broadcast(SourceSlotIndex, TargetSlotIndex, SourceItem);
}

void URightInventoryWidget::OnAllButtonClicked()
{
	SelectAllFilter();
}

void URightInventoryWidget::OnWeaponButtonClicked()
{
	const FGameplayTag WeaponTypeTag = GetWeaponTypeTag();

	SelectTypeFilter(WeaponTypeTag);
}

void URightInventoryWidget::OnEquipmentButtonClicked()
{
	const FGameplayTag EquipmentTypeTag = GetEquipmentTypeTag();

	SelectTypeFilter(EquipmentTypeTag);
}

void URightInventoryWidget::OnValuableButtonClicked()
{
	const FGameplayTag ValuableTypeTag = GetValuableTypeTag();

	SelectTypeFilter(ValuableTypeTag);
}

void URightInventoryWidget::OnConsumableButtonClicked()
{
	const FGameplayTag ConsumableTypeTag = GetConsumableTypeTag();

	SelectTypeFilter(ConsumableTypeTag);
}

void URightInventoryWidget::OnSearchButtonClicked()
{
	ActiveSearchText = SearchBox ? SearchBox->GetText().ToString().TrimStartAndEnd() : FString();

	RebuildTileViewFromCachedSourceItems();
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


}

void URightInventoryWidget::RebuildTileViewFromCachedSourceItems()
{
	if (!TileView)
	{

		return;
	}

	TileView->ClearListItems();
	CachedSlotViewData.Reset();

	const FString SearchText = ActiveSearchText.TrimStartAndEnd();
	const bool bUseSearch = !SearchText.IsEmpty();
	int32 ItemCount = 0;
	int32 MatchedItemCount = 0;
	for (const TObjectPtr<UObject>& ListItem : CachedSourceListItems)
	{
		if (Cast<UItemInstance>(ListItem.Get()))
		{
			++ItemCount;
		}
	}

	const int32 SlotCountToDisplay = bUseSearch ? CachedSourceListItems.Num() : FMath::Max(InventorySlotCount, CachedSourceListItems.Num());
	CachedSlotViewData.Reserve(SlotCountToDisplay);

	for (int32 SlotIndex = 0; SlotIndex < SlotCountToDisplay; ++SlotIndex)
	{
		UItemInstance* ItemInstance = CachedSourceListItems.IsValidIndex(SlotIndex) ? Cast<UItemInstance>(CachedSourceListItems[SlotIndex].Get()) : nullptr;
		if (bUseSearch)
		{
			if (!DoesItemMatchSearch(ItemInstance, SearchText))
			{
				continue;
			}

			++MatchedItemCount;
		}
		else if (ItemInstance)
		{
			++MatchedItemCount;
		}

		UInventorySlotViewData* SlotViewData = NewObject<UInventorySlotViewData>(this);
		SlotViewData->Initialize(SlotIndex, ItemInstance);
		CachedSlotViewData.Add(SlotViewData);
		TileView->AddItem(SlotViewData);
	}


}

bool URightInventoryWidget::DoesItemMatchSearch(const UItemInstance* ItemInstance, const FString& SearchText) const
{
	if (SearchText.IsEmpty())
	{
		return true;
	}

	const UItemDefinition* ItemDefinition = IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
	if (!ItemDefinition)
	{
		return false;
	}

	const FString DisplayName = ItemDefinition->DisplayName.ToString();
	if (DisplayName.Contains(SearchText, ESearchCase::IgnoreCase))
	{
		return true;
	}

	return ItemDefinition->GetName().Contains(SearchText, ESearchCase::IgnoreCase);
}

void URightInventoryWidget::ApplyWidgetDefinitionSettings()
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const FInventoryWidgetSettings& Settings = WidgetDefinition->GetInventoryWidgetSettings();
		InventorySlotCount = FMath::Max(Settings.GameInventoryItemCountLimit, 0);
		WeaponTypeTagOverride = Settings.WeaponTypeTag;
		EquipmentTypeTagOverride = Settings.EquipmentTypeTag;
		ValuableTypeTagOverride = Settings.ValuableTypeTag;
		ConsumableTypeTagOverride = Settings.ConsumableTypeTag;
	}
}

UButton* URightInventoryWidget::ResolveFilterButton(const FGameplayTag TypeTag) const
{
	if (!TypeTag.IsValid())
	{
		return nullptr;
	}

	if (TypeTag.MatchesTagExact(GetWeaponTypeTag()))
	{
		return WeaponButton;
	}

	if (TypeTag.MatchesTagExact(GetEquipmentTypeTag()))
	{
		return EquipmentButton;
	}

	if (TypeTag.MatchesTagExact(GetValuableTypeTag()))
	{
		return ValuableButton;
	}

	if (TypeTag.MatchesTagExact(GetConsumableTypeTag()))
	{
		return ConsumableButton;
	}

	return nullptr;
}

FGameplayTag URightInventoryWidget::GetWeaponTypeTag() const
{
	return WeaponTypeTagOverride.IsValid()
		? WeaponTypeTagOverride
		: UProjectTagConfig::Get(this)->GetItemWeaponTypeTag();
}

FGameplayTag URightInventoryWidget::GetEquipmentTypeTag() const
{
	return EquipmentTypeTagOverride.IsValid()
		? EquipmentTypeTagOverride
		: UProjectTagConfig::Get(this)->GetItemEquipmentTypeTag();
}

FGameplayTag URightInventoryWidget::GetValuableTypeTag() const
{
	return ValuableTypeTagOverride.IsValid()
		? ValuableTypeTagOverride
		: UProjectTagConfig::Get(this)->GetItemValuableTypeTag();
}

FGameplayTag URightInventoryWidget::GetConsumableTypeTag() const
{
	return ConsumableTypeTagOverride.IsValid()
		? ConsumableTypeTagOverride
		: UProjectTagConfig::Get(this)->GetItemConsumableTypeTag();
}
