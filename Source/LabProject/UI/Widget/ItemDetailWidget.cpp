#include "UI/Widget/ItemDetailWidget.h"

#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Item/ItemInstance.h"
#include "Skin/SkinInstance.h"
#include "UI/Widget/ItemViewData.h"
#include "UI/WidgetLookup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ItemDetailWidget)

void UItemDetailWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	CacheOptionalWidgets();
}

void UItemDetailWidget::SetItem(UItemInstance* InItemInstance, UItemInstance* InCompareItemInstance)
{
	CacheOptionalWidgets();
	(void)InCompareItemInstance;

	const FPdItemViewData ViewData = FItemViewDataBuilder::FromItemInstance(InItemInstance);
	SetItemViewData(ViewData);
}

void UItemDetailWidget::SetItemViewData(const FPdItemViewData& InViewData)
{
	CacheOptionalWidgets();

	if (!InViewData.HasContent())
	{
		ClearDetails();
		return;
	}

	SetHeader(InViewData);
	PopulateStats(InViewData.Stats);
}

void UItemDetailWidget::SetSkin(USkinInstance* InSkinInstance)
{
	CacheOptionalWidgets();

	const FPdItemViewData ViewData = FItemViewDataBuilder::FromSkinInstance(InSkinInstance);
	SetItemViewData(ViewData);
}

void UItemDetailWidget::SetSkinDefinition(const USkinDefinition* SkinDefinition)
{
	CacheOptionalWidgets();

	const FPdItemViewData ViewData = FItemViewDataBuilder::FromSkinDefinition(SkinDefinition);
	SetItemViewData(ViewData);
}

void UItemDetailWidget::ClearDetails()
{
	CacheOptionalWidgets();

	SetHeader(FPdItemViewData());

	if (StatsList)
	{
		StatsList->ClearChildren();
	}
}

void UItemDetailWidget::CacheOptionalWidgets()
{
	if (!IconImage)
	{
		IconImage = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("IconImage"),
			TEXT("ItemIconImage"),
			TEXT("DetailIconImage")
		});
	}

	if (!NameText)
	{
		NameText = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("NameText"),
			TEXT("ItemNameText"),
			TEXT("DetailNameText")
		});
	}

	if (!DescriptionText)
	{
		DescriptionText = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("DescriptionText"),
			TEXT("ItemDescriptionText"),
			TEXT("DetailDescriptionText")
		});
	}

	if (!StatsList)
	{
		StatsList = PdWidgetLookup::FindWidgetByNames<UPanelWidget>(this, {
			TEXT("StatsList"),
			TEXT("StatList"),
			TEXT("VB_Stats")
		});
	}
}

void UItemDetailWidget::SetIconResource(UObject* IconResource) const
{
	if (!IconImage)
	{
		return;
	}

	IconImage->SetBrushResourceObject(IconResource);
	IconImage->SetVisibility(IconResource ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UItemDetailWidget::SetHeader(const FPdItemViewData& ViewData) const
{
	SetIconResource(ViewData.IconResource);

	if (NameText)
	{
		NameText->SetText(ViewData.DisplayName);
	}

	if (DescriptionText)
	{
		DescriptionText->SetText(ViewData.Description);
		DescriptionText->SetVisibility(ViewData.Description.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UItemDetailWidget::PopulateStats(const TMap<FGameplayTag, float>& NewStats)
{
	if (!StatsList)
	{
		return;
	}

	StatsList->ClearChildren();

	TArray<FGameplayTag> StatTags;
	NewStats.GetKeys(StatTags);
	StatTags.Sort([](const FGameplayTag& A, const FGameplayTag& B)
	{
		return A.ToString() < B.ToString();
	});

	for (const FGameplayTag& StatTag : StatTags)
	{
		const float NewValue = NewStats.FindRef(StatTag);
		AddStatRow(StatTag, NewValue);
	}
}

void UItemDetailWidget::AddStatRow(const FGameplayTag StatTag, const float NewValue)
{
	if (!StatsList)
	{
		return;
	}

	FString ValueText = FString::Printf(TEXT("%.0f"), NewValue);

	UTextBlock* RowText = NewObject<UTextBlock>(this);
	if (!RowText)
	{
		return;
	}

	RowText->SetText(FText::FromString(FString::Printf(TEXT("%s  %s"), *GetDisplayNameForStatTag(StatTag), *ValueText)));
	RowText->SetColorAndOpacity(FSlateColor(NeutralStatColor));
	RowText->SetJustification(ETextJustify::Left);
	StatsList->AddChild(RowText);
}

FString UItemDetailWidget::GetDisplayNameForStatTag(const FGameplayTag StatTag)
{
	FString TagString = StatTag.IsValid() ? StatTag.ToString() : TEXT("Stat");
	FString Left;
	FString Right;
	if (TagString.Split(TEXT("."), &Left, &Right, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
	{
		return Right;
	}

	return TagString;
}
