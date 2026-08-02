#include "UI/Widget/RightSkinWidget.h"

#include "Definition/Common/ProjectTagConfig.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TileView.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Skin/SkinInstance.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "UI/Widget/SkinSlotViewData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RightSkinWidget)

URightSkinWidget::URightSkinWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URightSkinWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyWidgetDefinitionSettings();

	if (AllButton)
	{
		AllButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnAllButtonClicked);
	}

	if (PandoraButton)
	{
		PandoraButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnPandoraButtonClicked);
	}

	if (CosmeticsButton)
	{
		CosmeticsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnCosmeticsButtonClicked);
	}

	if (GestureButton)
	{
		GestureButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnGestureButtonClicked);
	}

	if (RidingButton)
	{
		RidingButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnRidingButtonClicked);
	}

	if (PetButton)
	{
		PetButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnPetButtonClicked);
	}

	if (Btn_Search)
	{
		Btn_Search->OnClicked.AddUniqueDynamic(this, &ThisClass::OnSearchButtonClicked);
	}

	RebuildFilterButtonList();
	FilterButtonHighlightState.Initialize(FilterButtonList, AllButton, SelectedFilterAccentColor);
}

void URightSkinWidget::NativeDestruct()
{
	FilterButtonHighlightState.Reset();

	if (AllButton)
	{
		AllButton->OnClicked.RemoveDynamic(this, &ThisClass::OnAllButtonClicked);
	}

	if (PandoraButton)
	{
		PandoraButton->OnClicked.RemoveDynamic(this, &ThisClass::OnPandoraButtonClicked);
	}

	if (CosmeticsButton)
	{
		CosmeticsButton->OnClicked.RemoveDynamic(this, &ThisClass::OnCosmeticsButtonClicked);
	}

	if (GestureButton)
	{
		GestureButton->OnClicked.RemoveDynamic(this, &ThisClass::OnGestureButtonClicked);
	}

	if (RidingButton)
	{
		RidingButton->OnClicked.RemoveDynamic(this, &ThisClass::OnRidingButtonClicked);
	}

	if (PetButton)
	{
		PetButton->OnClicked.RemoveDynamic(this, &ThisClass::OnPetButtonClicked);
	}

	if (Btn_Search)
	{
		Btn_Search->OnClicked.RemoveDynamic(this, &ThisClass::OnSearchButtonClicked);
	}

	Super::NativeDestruct();
}

void URightSkinWidget::SelectAllFilter()
{
	FilterButtonHighlightState.Select(AllButton, SelectedFilterAccentColor);
	OnClicked_SkinFilterAllButton.Broadcast();
}

void URightSkinWidget::SelectTypeFilter(FGameplayTag TypeTag)
{
	if (UButton* SelectedButton = ResolveFilterButton(TypeTag))
	{
		FilterButtonHighlightState.Select(SelectedButton, SelectedFilterAccentColor);
	}

	OnClicked_SkinFilterTypeButton.Broadcast(TypeTag);
}

void URightSkinWidget::ToggleActiveFiliterButtons(bool bActive)
{
	for (UButton* Button : FilterButtonList)
	{
		if (Button)
		{
			Button->SetIsEnabled(bActive);
		}
	}
}

void URightSkinWidget::ResetFilterHighlightToAll()
{
	FilterButtonHighlightState.Select(AllButton, SelectedFilterAccentColor);
}

void URightSkinWidget::SetTileView(const TArray<UObject*>& InListItems)
{
	CachedSourceListItems.Reset();
	CachedSourceListItems.Reserve(InListItems.Num());
	for (UObject* ListItem : InListItems)
	{
		CachedSourceListItems.Add(ListItem);
	}

	RebuildTileViewFromCachedSourceItems();
}

void URightSkinWidget::ClearTileViewItemClicked()
{
	if (TileView)
	{
		TileView->OnItemClicked().Clear();
	}
}

void URightSkinWidget::OnAllButtonClicked()
{
	SelectAllFilter();
}

void URightSkinWidget::OnPandoraButtonClicked()
{
	SelectTypeFilter(GetPandoraTypeTag());
}

void URightSkinWidget::OnCosmeticsButtonClicked()
{
	SelectTypeFilter(GetCosmeticsTypeTag());
}

void URightSkinWidget::OnGestureButtonClicked()
{
	SelectTypeFilter(GetGestureTypeTag());
}

void URightSkinWidget::OnRidingButtonClicked()
{
	SelectTypeFilter(GetRidingTypeTag());
}

void URightSkinWidget::OnPetButtonClicked()
{
	SelectTypeFilter(GetPetTypeTag());
}

void URightSkinWidget::OnSearchButtonClicked()
{
	ActiveSearchText = SearchBox ? SearchBox->GetText().ToString().TrimStartAndEnd() : FString();
	RebuildTileViewFromCachedSourceItems();
}

void URightSkinWidget::RebuildFilterButtonList()
{
	FilterButtonList.Reset();
	FilterButtonList.Reserve(6);

	FilterButtonList.Add(AllButton);
	FilterButtonList.Add(PandoraButton);
	FilterButtonList.Add(CosmeticsButton);
	FilterButtonList.Add(GestureButton);
	FilterButtonList.Add(RidingButton);
	FilterButtonList.Add(PetButton);
}

void URightSkinWidget::RebuildTileViewFromCachedSourceItems()
{
	if (!TileView)
	{
		return;
	}

	TileView->ClearListItems();
	CachedSlotViewData.Reset();

	TArray<USkinInstance*> SkinInstances;
	SkinInstances.Reserve(CachedSourceListItems.Num());

	const FString SearchText = ActiveSearchText.TrimStartAndEnd();
	const bool bUseSearch = !SearchText.IsEmpty();
	for (const TObjectPtr<UObject>& ListItem : CachedSourceListItems)
	{
		USkinInstance* SkinInstance = Cast<USkinInstance>(ListItem.Get());
		if (!SkinInstance)
		{
			continue;
		}

		if (bUseSearch && !DoesSkinMatchSearch(SkinInstance, SearchText))
		{
			continue;
		}

		SkinInstances.Add(SkinInstance);
	}

	const int32 SlotCountToDisplay = bUseSearch ? SkinInstances.Num() : FMath::Max(SkinSlotCount, SkinInstances.Num());
	CachedSlotViewData.Reserve(SlotCountToDisplay);

	for (int32 SlotIndex = 0; SlotIndex < SlotCountToDisplay; ++SlotIndex)
	{
		USkinSlotViewData* SlotViewData = NewObject<USkinSlotViewData>(this);
		SlotViewData->Initialize(SlotIndex, SkinInstances.IsValidIndex(SlotIndex) ? SkinInstances[SlotIndex] : nullptr);
		CachedSlotViewData.Add(SlotViewData);
		TileView->AddItem(SlotViewData);
	}
}

bool URightSkinWidget::DoesSkinMatchSearch(const USkinInstance* SkinInstance, const FString& SearchText) const
{
	if (SearchText.IsEmpty())
	{
		return true;
	}

	const USkinDefinition* SkinDefinition = IsValid(SkinInstance) ? SkinInstance->SkinDefinition.Get() : nullptr;
	if (!SkinDefinition)
	{
		return false;
	}

	const FString DisplayName = SkinDefinition->DisplayName.ToString();
	if (DisplayName.Contains(SearchText, ESearchCase::IgnoreCase))
	{
		return true;
	}

	return SkinDefinition->GetName().Contains(SearchText, ESearchCase::IgnoreCase);
}

void URightSkinWidget::ApplyWidgetDefinitionSettings()
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const FSkinWidgetSettings& Settings = WidgetDefinition->GetSkinWidgetSettings();
		SkinSlotCount = FMath::Max(Settings.SkinSlotCount, 0);
		PandoraTypeTagOverride = Settings.PandoraTypeTag;
		CosmeticsTypeTagOverride = Settings.CosmeticsTypeTag;
		GestureTypeTagOverride = Settings.GestureTypeTag;
		RidingTypeTagOverride = Settings.RidingTypeTag;
		PetTypeTagOverride = Settings.PetTypeTag;
	}
}

UButton* URightSkinWidget::ResolveFilterButton(const FGameplayTag TypeTag) const
{
	if (!TypeTag.IsValid())
	{
		return nullptr;
	}

	if (TypeTag.MatchesTagExact(GetPandoraTypeTag()))
	{
		return PandoraButton;
	}

	if (TypeTag.MatchesTagExact(GetCosmeticsTypeTag()))
	{
		return CosmeticsButton;
	}

	if (TypeTag.MatchesTagExact(GetGestureTypeTag()))
	{
		return GestureButton;
	}

	if (TypeTag.MatchesTagExact(GetRidingTypeTag()))
	{
		return RidingButton;
	}

	if (TypeTag.MatchesTagExact(GetPetTypeTag()))
	{
		return PetButton;
	}

	return nullptr;
}

FGameplayTag URightSkinWidget::GetPandoraTypeTag() const
{
	return PandoraTypeTagOverride.IsValid()
		? PandoraTypeTagOverride
		: UProjectTagConfig::Get(this)->GetSkinPandoraTypeTag();
}

FGameplayTag URightSkinWidget::GetCosmeticsTypeTag() const
{
	return CosmeticsTypeTagOverride.IsValid()
		? CosmeticsTypeTagOverride
		: UProjectTagConfig::Get(this)->GetSkinCosmeticsTypeTag();
}

FGameplayTag URightSkinWidget::GetGestureTypeTag() const
{
	return GestureTypeTagOverride.IsValid()
		? GestureTypeTagOverride
		: UProjectTagConfig::Get(this)->GetSkinGestureTypeTag();
}

FGameplayTag URightSkinWidget::GetRidingTypeTag() const
{
	return RidingTypeTagOverride.IsValid()
		? RidingTypeTagOverride
		: UProjectTagConfig::Get(this)->GetSkinRidingTypeTag();
}

FGameplayTag URightSkinWidget::GetPetTypeTag() const
{
	return PetTypeTagOverride.IsValid()
		? PetTypeTagOverride
		: UProjectTagConfig::Get(this)->GetSkinPetTypeTag();
}
