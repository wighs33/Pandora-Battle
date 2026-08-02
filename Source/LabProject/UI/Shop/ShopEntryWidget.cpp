#include "UI/Shop/ShopEntryWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/Shop/ShopEntryViewData.h"
#include "UI/WidgetLookup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ShopEntryWidget)

void UShopEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ResolveWidgets();
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

void UShopEntryWidget::ResolveWidgets()
{
	if (!Txt_Name)
	{
		Txt_Name = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("Txt_Name"),
			TEXT("Txt_ItemName"),
			TEXT("Txt_ShopName"),
			TEXT("Txt_PandoraName"),
			TEXT("Txt_DisplayName")
		});
	}

	if (!Txt_Price)
	{
		Txt_Price = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("Txt_Price"),
			TEXT("Txt_GoldPrice"),
			TEXT("Txt_Cost")
		});
	}

	if (!Txt_State)
	{
		Txt_State = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("Txt_State"),
			TEXT("Txt_Status"),
			TEXT("Txt_Owned")
		});
	}

	if (!Img_Icon)
	{
		Img_Icon = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("Img_Icon"),
			TEXT("IconImage"),
			TEXT("Img_ItemIcon"),
			TEXT("Img_PandoraIcon"),
			TEXT("Img_ShopIcon")
		});
	}

	if (!SelectionBorderImage)
	{
		SelectionBorderImage = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("SelectionBorderImage"),
			TEXT("Img_SelectionBorder"),
			TEXT("Img_Selected")
		});
	}

	if (!Btn_Select)
	{
		Btn_Select = PdWidgetLookup::FindWidgetByNames<UButton>(this, {
			TEXT("Btn_Select"),
			TEXT("Btn_Click"),
			TEXT("Btn_Entry"),
			TEXT("Btn_ShopEntry")
		});
	}
}

void UShopEntryWidget::RefreshUI()
{
	ResolveWidgets();

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
		if (Img_Icon)
		{
			Img_Icon->SetVisibility(ESlateVisibility::Collapsed);
		}
		ApplySelectionVisual();
		return;
	}

	if (Txt_Name)
	{
		Txt_Name->SetText(UiData->DisplayName);
	}

	if (Txt_Price)
	{
		Txt_Price->SetText(FText::Format(PriceTextFormat, FText::AsNumber(UiData->GoldPrice)));
	}

	if (Txt_State)
	{
		const FText StateText = UiData->bOwned
			? OwnedText
			: (!UiData->bCanSell ? NotForSaleText : (UiData->bCanAfford ? AvailableText : NotEnoughGoldText));
		Txt_State->SetText(StateText);
	}

	if (Img_Icon)
	{
		Img_Icon->SetBrushResourceObject(UiData->IconResource);
		Img_Icon->SetVisibility(UiData->IconResource ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	ApplySelectionVisual();
}

void UShopEntryWidget::ApplySelectionVisual()
{
	ResolveWidgets();

	if (!SelectionBorderImage)
	{
		return;
	}

	SelectionBorderImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SelectionBorderImage->SetColorAndOpacity(bIsSelected ? SelectionBorderSelectedColor : SelectionBorderDefaultColor);
}
