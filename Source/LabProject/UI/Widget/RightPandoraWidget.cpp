#include "UI/Widget/RightPandoraWidget.h"

#include "Common/ProjectTagConfig.h"
#include "Components/Button.h"
#include "Components/TileView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RightPandoraWidget)

URightPandoraWidget::URightPandoraWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URightPandoraWidget::NativeConstruct()
{
	Super::NativeConstruct();

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

	RebuildFilterButtonList();
}

void URightPandoraWidget::NativeDestruct()
{
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

	Super::NativeDestruct();
}

void URightPandoraWidget::SelectAllFilter()
{
	OnClicked_PandoraFilterAllButton.Broadcast();
}

void URightPandoraWidget::SelectTypeFilter(FGameplayTag TypeTag)
{
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

void URightPandoraWidget::SetTileViewAndShowLockState(const TArray<UObject*>& InListItems)
{
	if (!TileView)
	{
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

FGameplayTag URightPandoraWidget::GetOffensiveTypeTag() const
{
	return UProjectTagConfig::Get(this)->GetPandoraOffensiveTypeTag();
}

FGameplayTag URightPandoraWidget::GetDefensiveTypeTag() const
{
	return UProjectTagConfig::Get(this)->GetPandoraDefensiveTypeTag();
}

FGameplayTag URightPandoraWidget::GetSupportTypeTag() const
{
	return UProjectTagConfig::Get(this)->GetPandoraSupportTypeTag();
}

FGameplayTag URightPandoraWidget::GetSpecialTypeTag() const
{
	return UProjectTagConfig::Get(this)->GetPandoraSpecialTypeTag();
}
