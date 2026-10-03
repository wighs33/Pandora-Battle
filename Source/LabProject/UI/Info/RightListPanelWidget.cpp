#include "UI/Info/RightListPanelWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TileView.h"
#include "Definition/Common/ProjectTagDefinition.h"
#include "Localization/MenuLocalizationSubsystem.h"
#include "UI/Common/ButtonClickRelay.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RightListPanelWidget)

void URightListPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TypeFilters.Reset();
	ApplyWidgetDefinitionSettings();

	if (AllButton)
	{
		AllButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAllFilterClicked);
	}
	if (Btn_Search)
	{
		Btn_Search->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSearchClicked);
	}

	FilterButtonList.Reset();
	FilterButtonList.Add(AllButton);
	for (int32 FilterIndex = 0; FilterIndex < TypeFilters.Num(); ++FilterIndex)
	{
		UButton* Button = TypeFilters[FilterIndex].Button.Get();
		FilterButtonList.Add(Button);
		if (Button)
		{
			UButtonClickRelay* ClickRelay = NewObject<UButtonClickRelay>(this);
			ClickRelay->Bind(Button, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleTypeFilterClicked, FilterIndex));
			TypeFilterClickRelays.Add(ClickRelay);
		}
	}
	FilterButtonHighlightState.Initialize(FilterButtonList, AllButton, SelectedFilterAccentColor);
}

void URightListPanelWidget::NativeDestruct()
{
	FilterButtonHighlightState.Reset();

	if (AllButton)
	{
		AllButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleAllFilterClicked);
	}
	if (Btn_Search)
	{
		Btn_Search->OnClicked.RemoveDynamic(this, &ThisClass::HandleSearchClicked);
	}
	for (UButtonClickRelay* ClickRelay : TypeFilterClickRelays)
	{
		if (ClickRelay)
		{
			ClickRelay->Unbind();
		}
	}
	TypeFilterClickRelays.Reset();

	Super::NativeDestruct();
}

void URightListPanelWidget::SelectAllFilter()
{
	FilterButtonHighlightState.Select(AllButton, SelectedFilterAccentColor);
	OnClicked_FilterAllButton.Broadcast();
}

// 태그에 맞는 분류 버튼이 있으면 강조하고, 없어도 선택 알림은 보낸다.
void URightListPanelWidget::SelectTypeFilter(const FGameplayTag TypeTag)
{
	for (int32 FilterIndex = 0; TypeTag.IsValid() && FilterIndex < TypeFilters.Num(); ++FilterIndex)
	{
		if (TypeTag.MatchesTagExact(GetTypeFilterTag(FilterIndex)))
		{
			if (UButton* SelectedButton = TypeFilters[FilterIndex].Button.Get())
			{
				FilterButtonHighlightState.Select(SelectedButton, SelectedFilterAccentColor);
			}
			break;
		}
	}

	OnClicked_FilterTypeButton.Broadcast(TypeTag);
}

void URightListPanelWidget::SetFilterButtonsEnabled(const bool bEnabled)
{
	for (UButton* Button : FilterButtonList)
	{
		if (Button)
		{
			Button->SetIsEnabled(bEnabled);
		}
	}
}

void URightListPanelWidget::ResetFilterHighlightToAll()
{
	FilterButtonHighlightState.Select(AllButton, SelectedFilterAccentColor);
}

void URightListPanelWidget::SetTileView(const TArray<UObject*>& InListItems)
{
	CachedSourceListItems.Reset(InListItems.Num());
	for (UObject* ListItem : InListItems)
	{
		CachedSourceListItems.Add(ListItem);
	}

	RebuildTileView();
}

void URightListPanelWidget::ClearTileViewItemClicked()
{
	if (TileView)
	{
		TileView->OnItemClicked().Clear();
	}
}

void URightListPanelWidget::AddTypeFilter(UButton* Button, const FGameplayTag TagOverride, const FDefaultTypeTagGetter DefaultTag)
{
	TypeFilters.Add({Button, TagOverride, DefaultTag});
}

FGameplayTag URightListPanelWidget::GetTypeFilterTag(const int32 FilterIndex) const
{
	if (!TypeFilters.IsValidIndex(FilterIndex))
	{
		return FGameplayTag();
	}

	const FTypeFilter& Filter = TypeFilters[FilterIndex];
	return Filter.TagOverride.IsValid() ? Filter.TagOverride : (UProjectTagDefinition::Get(this)->*Filter.DefaultTag)();
}

bool URightListPanelWidget::MatchesSearch(const UObject* Product, const FText& DisplayName, const FString& SearchText) const
{
	if (SearchText.IsEmpty())
	{
		return true;
	}

	if (!IsValid(Product))
	{
		return false;
	}

	const FText LocalizedName = GetLocalization() ? GetLocalization()->GetProductText(Product, TEXT("Name"), DisplayName) : DisplayName;
	return LocalizedName.ToString().Contains(SearchText, ESearchCase::IgnoreCase)
		|| Product->GetName().Contains(SearchText, ESearchCase::IgnoreCase);
}

void URightListPanelWidget::HandleAllFilterClicked()
{
	SelectAllFilter();
}

void URightListPanelWidget::HandleSearchClicked()
{
	ActiveSearchText = SearchBox ? SearchBox->GetText().ToString().TrimStartAndEnd() : FString();
	RebuildTileView();
}

void URightListPanelWidget::HandleTypeFilterClicked(const int32 FilterIndex)
{
	SelectTypeFilter(GetTypeFilterTag(FilterIndex));
}
