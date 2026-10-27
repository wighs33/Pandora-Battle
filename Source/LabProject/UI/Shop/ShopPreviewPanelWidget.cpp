#include "UI/Shop/ShopPreviewPanelWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/Shop/ShopEntryViewData.h"
#include "Localization/MenuLocalizationSubsystem.h"

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
	// 구매 후 목록을 새로 고치면 표시용 데이터 객체가 바뀐다. 같은 상품이 선택된 동안에는
	// 번역된 결과 메시지를 유지한다.
	if (!EntryData || !InEntryData || EntryData->GetProductObject() != InEntryData->GetProductObject()
		|| EntryData->GetProductType() != InEntryData->GetProductType())
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
	CaptureDefaultNameColor();
	ConfigureDescriptionTextBlock();
	CaptureDefaultPriceColor();
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
		FText EffectiveMessage = MessageText;
		if (UiData && UiData->bValid && UiData->bOwned && EffectiveMessage.IsEmpty())
		{
			EffectiveMessage = MenuText(TEXT("Shop.Owned"));
		}
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
			Txt_Name->SetText(MenuText(TEXT("Shop.SelectItem")));
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
		Txt_Name->SetText(EntryData->GetDisplayName());
		Txt_Name->SetColorAndOpacity(DefaultNameColor);
	}
	if (Txt_Description)
	{
		Txt_Description->SetText(EntryData->GetDescription());
	}
	if (Txt_Price)
	{
		Txt_Price->SetText(FText::Format(MenuText(TEXT("Shop.Price")), FText::AsNumber(UiData->GoldPrice)));
	}
	ApplyPriceColor(UiData);
	if (Txt_State)
	{
		const FText StateText = UiData->bOwned ? MenuText(TEXT("Shop.Owned"))
			: (!UiData->bCanSell ? MenuText(TEXT("Shop.NotForSale"))
				: (UiData->bCanAfford ? MenuText(TEXT("Shop.Available"))
					: MenuText(TEXT("Shop.NeedGold"))));
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
