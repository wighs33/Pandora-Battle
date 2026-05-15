#include "UI/Widget/RightSkinWidget.h"

#include "Common/ProjectTagConfig.h"
#include "Components/Button.h"
#include "Components/TileView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RightSkinWidget)

URightSkinWidget::URightSkinWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URightSkinWidget::NativeConstruct()
{
	Super::NativeConstruct();

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

	RebuildFilterButtonList();
}

void URightSkinWidget::NativeDestruct()
{
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

	Super::NativeDestruct();
}

void URightSkinWidget::SelectAllFilter()
{
	OnClicked_SkinFilterAllButton.Broadcast();
}

void URightSkinWidget::SelectTypeFilter(FGameplayTag TypeTag)
{
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

void URightSkinWidget::SetTileView(const TArray<UObject*>& InListItems)
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

void URightSkinWidget::RebuildFilterButtonList()
{
	FilterButtonList.Reset();
	FilterButtonList.Reserve(5);

	FilterButtonList.Add(AllButton);
	FilterButtonList.Add(PandoraButton);
	FilterButtonList.Add(CosmeticsButton);
	FilterButtonList.Add(GestureButton);
	FilterButtonList.Add(RidingButton);
}

FGameplayTag URightSkinWidget::GetPandoraTypeTag() const
{
	return UProjectTagConfig::Get(this)->GetSkinPandoraTypeTag();
}

FGameplayTag URightSkinWidget::GetCosmeticsTypeTag() const
{
	return UProjectTagConfig::Get(this)->GetSkinCosmeticsTypeTag();
}

FGameplayTag URightSkinWidget::GetGestureTypeTag() const
{
	return UProjectTagConfig::Get(this)->GetSkinGestureTypeTag();
}

FGameplayTag URightSkinWidget::GetRidingTypeTag() const
{
	return UProjectTagConfig::Get(this)->GetSkinRidingTypeTag();
}
