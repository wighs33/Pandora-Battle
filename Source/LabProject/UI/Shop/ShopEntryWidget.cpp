#include "UI/Shop/ShopEntryWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/Shop/ShopEntryViewData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ShopEntryWidget)

void UShopEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Select)
	{
		Btn_Select->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSelectClicked);
	}

	RefreshUI();
}

void UShopEntryWidget::NativeDestruct()
{
	if (Btn_Select)
	{
		Btn_Select->OnClicked.RemoveDynamic(this, &ThisClass::HandleSelectClicked);
	}

	Super::NativeDestruct();
}

void UShopEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	SetEntryData(Cast<UShopEntryViewData>(ListItemObject));
}

void UShopEntryWidget::NativeOnItemSelectionChanged(const bool bInIsSelected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(bInIsSelected);

	SetSelected(bInIsSelected);
}

void UShopEntryWidget::SetEntryData(UShopEntryViewData* InEntryData)
{
	EntryData = InEntryData;
	bIsSelected = false;
	RefreshUI();
}

void UShopEntryWidget::SetSelected(const bool bInSelected)
{
	if (bIsSelected == bInSelected)
	{
		return;
	}

	bIsSelected = bInSelected;
	ApplySelectionVisual();
}

void UShopEntryWidget::HandleSelectClicked()
{
	if (EntryData)
	{
		EntryData->BroadcastClicked();
	}
}

void UShopEntryWidget::RefreshUI()
{
	const FShopEntryUiData* UiData = EntryData ? &EntryData->GetUiData() : nullptr;
	if (!UiData || !UiData->bValid)
	{
		if (Txt_Name)
		{
			Txt_Name->SetText(FText::GetEmpty());
		}
		if (Txt_Price)
		{
			Txt_Price->SetText(FText::GetEmpty());
		}
		if (Txt_State)
		{
			Txt_State->SetText(FText::GetEmpty());
		}
		if (IconImage)
		{
			IconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		ApplySelectionVisual();
		return;
	}

	if (Txt_Name)
	{
		Txt_Name->SetText(EntryData->GetLocalizedName(GetLocalization()));
	}

	if (Txt_Price)
	{
		Txt_Price->SetText(FText::Format(MenuTextOrFallback(TEXT("Shop.Price"), PriceTextFormat), FText::AsNumber(UiData->GoldPrice)));
	}

	if (Txt_State)
	{
		const FText StateText = UiData->bOwned
			? MenuTextOrFallback(TEXT("Shop.Owned"), OwnedText)
			: (!UiData->bCanSell ? MenuTextOrFallback(TEXT("Shop.NotForSale"), NotForSaleText)
				: (UiData->bCanAfford ? MenuTextOrFallback(TEXT("Shop.Available"), AvailableText)
					: MenuTextOrFallback(TEXT("Shop.NeedGold"), NotEnoughGoldText)));
		Txt_State->SetText(StateText);
	}

	if (IconImage)
	{
		IconImage->SetBrushResourceObject(UiData->IconResource);
		IconImage->SetVisibility(UiData->IconResource ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	ApplySelectionVisual();
}

void UShopEntryWidget::ApplySelectionVisual()
{
	if (!SelectionBorderImage)
	{
		return;
	}

	SelectionBorderImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	FSlateBrush SelectionBrush = SelectionBorderImage->GetBrush();
	SelectionBrush.OutlineSettings.Color = FSlateColor(bIsSelected ? SelectionBorderSelectedColor : SelectionBorderDefaultColor);
	SelectionBorderImage->SetBrush(SelectionBrush);
	SelectionBorderImage->SetColorAndOpacity(FLinearColor::White);
}
