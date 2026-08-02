#include "UI/Widget/RightPandoraWidget.h"

#include "Definition/Common/ProjectTagConfig.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TileView.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Pandora/PandoraInstance.h"
#include "Definition/UI/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RightPandoraWidget)

URightPandoraWidget::URightPandoraWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URightPandoraWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyWidgetDefinitionSettings();

	if (AllButton)
	{
		AllButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnAllButtonClicked);
	}

	if (OffensiveButton)
	{
		OffensiveButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnOffensiveButtonClicked);
	}

	if (DefensiveButton)
	{
		DefensiveButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnDefensiveButtonClicked);
	}

	if (SupportButton)
	{
		SupportButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnSupportButtonClicked);
	}

	if (SpecialButton)
	{
		SpecialButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnSpecialButtonClicked);
	}

	if (Btn_Search)
	{
		Btn_Search->OnClicked.AddUniqueDynamic(this, &ThisClass::OnSearchButtonClicked);
	}

	RebuildFilterButtonList();
	FilterButtonHighlightState.Initialize(FilterButtonList, AllButton, SelectedFilterAccentColor);
}

void URightPandoraWidget::NativeDestruct()
{
	FilterButtonHighlightState.Reset();

	if (AllButton)
	{
		AllButton->OnClicked.RemoveDynamic(this, &ThisClass::OnAllButtonClicked);
	}

	if (OffensiveButton)
	{
		OffensiveButton->OnClicked.RemoveDynamic(this, &ThisClass::OnOffensiveButtonClicked);
	}

	if (DefensiveButton)
	{
		DefensiveButton->OnClicked.RemoveDynamic(this, &ThisClass::OnDefensiveButtonClicked);
	}

	if (SupportButton)
	{
		SupportButton->OnClicked.RemoveDynamic(this, &ThisClass::OnSupportButtonClicked);
	}

	if (SpecialButton)
	{
		SpecialButton->OnClicked.RemoveDynamic(this, &ThisClass::OnSpecialButtonClicked);
	}

	if (Btn_Search)
	{
		Btn_Search->OnClicked.RemoveDynamic(this, &ThisClass::OnSearchButtonClicked);
	}

	Super::NativeDestruct();
}

void URightPandoraWidget::SelectAllFilter()
{
	FilterButtonHighlightState.Select(AllButton, SelectedFilterAccentColor);
	OnClicked_PandoraFilterAllButton.Broadcast();
}

void URightPandoraWidget::SelectTypeFilter(FGameplayTag TypeTag)
{
	if (UButton* SelectedButton = ResolveFilterButton(TypeTag))
	{
		FilterButtonHighlightState.Select(SelectedButton, SelectedFilterAccentColor);
	}

	OnClicked_PandoraFilterTypeButton.Broadcast(TypeTag);
}

void URightPandoraWidget::ToggleActiveFiliterButtons(bool bActive)
{
	for (UButton* Button : FilterButtonList)
	{
		if (Button)
		{
			Button->SetIsEnabled(bActive);
		}
	}
}

void URightPandoraWidget::ResetFilterHighlightToAll()
{
	FilterButtonHighlightState.Select(AllButton, SelectedFilterAccentColor);
}

void URightPandoraWidget::SetTileViewAndShowLockState(const TArray<UObject*>& InListItems)
{
	CachedSourceListItems.Reset();
	CachedSourceListItems.Reserve(InListItems.Num());
	for (UObject* ListItem : InListItems)
	{
		CachedSourceListItems.Add(ListItem);
	}

	RebuildTileViewFromCachedSourceItems();
}

void URightPandoraWidget::ClearTileViewItemClicked()
{
	if (TileView)
	{
		TileView->OnItemClicked().Clear();
	}
}

void URightPandoraWidget::OnAllButtonClicked()
{
	SelectAllFilter();
}

void URightPandoraWidget::OnOffensiveButtonClicked()
{
	SelectTypeFilter(GetOffensiveTypeTag());
}

void URightPandoraWidget::OnDefensiveButtonClicked()
{
	SelectTypeFilter(GetDefensiveTypeTag());
}

void URightPandoraWidget::OnSupportButtonClicked()
{
	SelectTypeFilter(GetSupportTypeTag());
}

void URightPandoraWidget::OnSpecialButtonClicked()
{
	SelectTypeFilter(GetSpecialTypeTag());
}

void URightPandoraWidget::OnSearchButtonClicked()
{
	ActiveSearchText = SearchBox ? SearchBox->GetText().ToString().TrimStartAndEnd() : FString();
	RebuildTileViewFromCachedSourceItems();
}

void URightPandoraWidget::RebuildFilterButtonList()
{
	FilterButtonList.Reset();
	FilterButtonList.Reserve(5);

	FilterButtonList.Add(AllButton);
	FilterButtonList.Add(OffensiveButton);
	FilterButtonList.Add(DefensiveButton);
	FilterButtonList.Add(SupportButton);
	FilterButtonList.Add(SpecialButton);
}

void URightPandoraWidget::RebuildTileViewFromCachedSourceItems()
{
	if (!TileView)
	{
		return;
	}

	TileView->ClearListItems();

	const FString SearchText = ActiveSearchText.TrimStartAndEnd();
	const bool bUseSearch = !SearchText.IsEmpty();
	for (const TObjectPtr<UObject>& ListItem : CachedSourceListItems)
	{
		UPandoraInstance* PandoraInstance = Cast<UPandoraInstance>(ListItem.Get());
		if (!PandoraInstance)
		{
			continue;
		}

		if (bUseSearch && !DoesPandoraMatchSearch(PandoraInstance, SearchText))
		{
			continue;
		}

		TileView->AddItem(PandoraInstance);
	}
}

bool URightPandoraWidget::DoesPandoraMatchSearch(const UPandoraInstance* PandoraInstance, const FString& SearchText) const
{
	if (SearchText.IsEmpty())
	{
		return true;
	}

	const UPandoraDefinition* PandoraDefinition = IsValid(PandoraInstance) ? PandoraInstance->PandoraDefinition.Get() : nullptr;
	if (!PandoraDefinition)
	{
		return false;
	}

	const FString DisplayName = PandoraDefinition->DisplayName.ToString();
	if (DisplayName.Contains(SearchText, ESearchCase::IgnoreCase))
	{
		return true;
	}

	return PandoraDefinition->GetName().Contains(SearchText, ESearchCase::IgnoreCase);
}

void URightPandoraWidget::ApplyWidgetDefinitionSettings()
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const FRightPandoraWidgetSettings& Settings = WidgetDefinition->GetRightPandoraWidgetSettings();
		OffensiveTypeTagOverride = Settings.OffensiveTypeTag;
		DefensiveTypeTagOverride = Settings.DefensiveTypeTag;
		SupportTypeTagOverride = Settings.SupportTypeTag;
		SpecialTypeTagOverride = Settings.SpecialTypeTag;
	}
}

UButton* URightPandoraWidget::ResolveFilterButton(const FGameplayTag TypeTag) const
{
	if (!TypeTag.IsValid())
	{
		return nullptr;
	}

	if (TypeTag.MatchesTagExact(GetOffensiveTypeTag()))
	{
		return OffensiveButton;
	}

	if (TypeTag.MatchesTagExact(GetDefensiveTypeTag()))
	{
		return DefensiveButton;
	}

	if (TypeTag.MatchesTagExact(GetSupportTypeTag()))
	{
		return SupportButton;
	}

	if (TypeTag.MatchesTagExact(GetSpecialTypeTag()))
	{
		return SpecialButton;
	}

	return nullptr;
}

FGameplayTag URightPandoraWidget::GetOffensiveTypeTag() const
{
	return OffensiveTypeTagOverride.IsValid()
		? OffensiveTypeTagOverride
		: UProjectTagConfig::Get(this)->GetPandoraOffensiveTypeTag();
}

FGameplayTag URightPandoraWidget::GetDefensiveTypeTag() const
{
	return DefensiveTypeTagOverride.IsValid()
		? DefensiveTypeTagOverride
		: UProjectTagConfig::Get(this)->GetPandoraDefensiveTypeTag();
}

FGameplayTag URightPandoraWidget::GetSupportTypeTag() const
{
	return SupportTypeTagOverride.IsValid()
		? SupportTypeTagOverride
		: UProjectTagConfig::Get(this)->GetPandoraSupportTypeTag();
}

FGameplayTag URightPandoraWidget::GetSpecialTypeTag() const
{
	return SpecialTypeTagOverride.IsValid()
		? SpecialTypeTagOverride
		: UProjectTagConfig::Get(this)->GetPandoraSpecialTypeTag();
}
