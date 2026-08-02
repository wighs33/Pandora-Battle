#include "UI/Shop/ShopPreviewPanelWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/Shop/ShopEntryViewData.h"
#include "UI/WidgetLookup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ShopPreviewPanelWidget)

void UShopPreviewPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ResolveWidgets();
	if (Btn_Buy)
	{
		Btn_Buy->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBuyClicked);
	}

	RefreshUI();
}

void UShopPreviewPanelWidget::NativeDestruct()
{
	if (Btn_Buy)
	{
		Btn_Buy->OnClicked.RemoveDynamic(this, &ThisClass::HandleBuyClicked);
	}

	Super::NativeDestruct();
}

void UShopPreviewPanelWidget::SetEntryData(UShopEntryViewData* InEntryData)
{
	if (EntryData != InEntryData)
	{
		MessageText = FText::GetEmpty();
	}

	EntryData = InEntryData;
	RefreshUI();
}

void UShopPreviewPanelWidget::SetMessage(const FText& Message)
{
	MessageText = Message;
	ApplyMessage();
}

void UShopPreviewPanelWidget::HandleBuyClicked()
{
	if (EntryData)
	{
		OnBuyRequested.Broadcast(EntryData);
	}
}

void UShopPreviewPanelWidget::ResolveWidgets()
{
	if (!Txt_Name)
	{
		Txt_Name = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("Txt_Name"),
			TEXT("Txt_ItemName"),
			TEXT("Txt_PandoraName"),
			TEXT("Txt_DisplayName"),
			TEXT("Txt_Title")
		});
	}
	CaptureDefaultNameColor();

	if (!Txt_Description)
	{
		Txt_Description = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("Desc"),
			TEXT("Description"),
			TEXT("Txt_Description"),
			TEXT("Txt_ItemDescription"),
			TEXT("Txt_PandoraDescription"),
			TEXT("Txt_Desc"),
			TEXT("Txt_PreviewDescription")
		});
	}
	ConfigureDescriptionTextBlock();

	if (!Txt_Price)
	{
		Txt_Price = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("Txt_Price"),
			TEXT("Txt_GoldPrice"),
			TEXT("Txt_Cost")
		});
	}
	CaptureDefaultPriceColor();

	if (!Txt_State)
	{
		Txt_State = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("Txt_State"),
			TEXT("Txt_Status"),
			TEXT("Txt_Owned")
		});
	}

	if (!Txt_Message)
	{
		Txt_Message = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("Txt_Message"),
			TEXT("Txt_StatusMessage"),
			TEXT("Txt_ResultMessage"),
			TEXT("Txt_Warning")
		});
	}

	if (!Img_Icon)
	{
		Img_Icon = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("Img_Icon"),
			TEXT("Img_ItemIcon"),
			TEXT("Img_PandoraIcon"),
			TEXT("Img_PreviewIcon"),
			TEXT("Img_Preview")
		});
	}

	if (!Btn_Buy)
	{
		Btn_Buy = PdWidgetLookup::FindWidgetByNames<UButton>(this, {
			TEXT("Btn_Buy"),
			TEXT("Btn_Purchase"),
			TEXT("Btn_Unlock")
		});
	}
}

void UShopPreviewPanelWidget::ConfigureDescriptionTextBlock()
{
	if (Txt_Description)
	{
		Txt_Description->SetAutoWrapText(bAutoWrapDescription);
		Txt_Description->SetWrapTextAt(DescriptionWrapTextAt);
	}
}

void UShopPreviewPanelWidget::CaptureDefaultNameColor()
{
	if (Txt_Name && !bDefaultNameColorCaptured)
	{
		DefaultNameColor = Txt_Name->GetColorAndOpacity();
		bDefaultNameColorCaptured = true;
	}
}

void UShopPreviewPanelWidget::CaptureDefaultPriceColor()
{
	if (Txt_Price && !bDefaultPriceColorCaptured)
	{
		DefaultPriceColor = Txt_Price->GetColorAndOpacity();
		bDefaultPriceColorCaptured = true;
	}
}

void UShopPreviewPanelWidget::ApplyPriceColor(const FShopEntryUiData* UiData)
{
	if (!Txt_Price)
	{
		return;
	}

	const bool bShouldShowNotEnoughGoldColor = UiData && UiData->bValid && !UiData->bOwned && UiData->bCanSell && !UiData->bCanAfford;
	Txt_Price->SetColorAndOpacity(bShouldShowNotEnoughGoldColor ? NotEnoughGoldPriceColor : DefaultPriceColor);
}

void UShopPreviewPanelWidget::ApplyMessage()
{
	if (Txt_Message)
	{
		const FShopEntryUiData* UiData = EntryData ? &EntryData->GetUiData() : nullptr;
		const FText EffectiveOwnedText = OwnedText.IsEmpty()
			? NSLOCTEXT("ShopPreviewPanelWidget", "OwnedFallbackText", "Owned")
			: OwnedText;
		const FText EffectiveMessage = (UiData && UiData->bValid && UiData->bOwned) ? EffectiveOwnedText : MessageText;
		Txt_Message->SetText(EffectiveMessage);
		Txt_Message->SetVisibility(EffectiveMessage.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UShopPreviewPanelWidget::RefreshUI()
{
	ResolveWidgets();

	const FShopEntryUiData* UiData = EntryData ? &EntryData->GetUiData() : nullptr;
	if (!UiData || !UiData->bValid)
	{
		if (Txt_Name)
		{
			Txt_Name->SetText(EmptyPreviewText);
			Txt_Name->SetColorAndOpacity(DefaultNameColor);
		}
		if (Txt_Description)
		{
			Txt_Description->SetText(FText::GetEmpty());
		}
		if (Txt_Price)
		{
			Txt_Price->SetText(FText::GetEmpty());
		}
		ApplyPriceColor(nullptr);
		if (Txt_State)
		{
			Txt_State->SetText(FText::GetEmpty());
		}
		if (Img_Icon)
		{
			Img_Icon->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (Btn_Buy)
		{
			Btn_Buy->SetIsEnabled(false);
		}
		ApplyMessage();
		return;
	}

	if (Txt_Name)
	{
		Txt_Name->SetText(UiData->DisplayName);
		Txt_Name->SetColorAndOpacity(DefaultNameColor);
	}
	if (Txt_Description)
	{
		Txt_Description->SetText(UiData->Description);
	}
	if (Txt_Price)
	{
		Txt_Price->SetText(FText::Format(PriceTextFormat, FText::AsNumber(UiData->GoldPrice)));
	}
	ApplyPriceColor(UiData);
	if (Txt_State)
	{
		const FText StateText = !UiData->bCanSell
			? NotForSaleText
			: (!UiData->bOwned && UiData->bCanAfford ? AvailableText : FText::GetEmpty());
		Txt_State->SetText(StateText);
		Txt_State->SetVisibility(StateText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (Img_Icon)
	{
		Img_Icon->SetBrushResourceObject(UiData->IconResource);
		Img_Icon->SetVisibility(UiData->IconResource ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Btn_Buy)
	{
		Btn_Buy->SetIsEnabled(!UiData->bOwned && UiData->bCanSell && UiData->bCanAfford);
	}
	ApplyMessage();
}
