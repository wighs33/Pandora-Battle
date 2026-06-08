#include "UI/Widget/ItemDetailWidget.h"

#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Item/ItemInstance.h"
#include "Skin/SkinInstance.h"
#include "UI/Widget/ItemViewData.h"

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

	const FPdItemViewData ViewData = FPdItemViewDataBuilder::FromItemInstance(InItemInstance);
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

	const FPdItemViewData ViewData = FPdItemViewDataBuilder::FromSkinInstance(InSkinInstance);
	SetItemViewData(ViewData);
}

void UItemDetailWidget::SetSkinDefinition(const USkinDefinition* SkinDefinition)
{
	CacheOptionalWidgets();

	const FPdItemViewData ViewData = FPdItemViewDataBuilder::FromSkinDefinition(SkinDefinition);
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
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("IconImage")));
	}
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("ItemIconImage")));
	}
	if (!IconImage)
	{
		IconImage = Cast<UImage>(GetWidgetFromName(TEXT("DetailIconImage")));
	}

	if (!NameText)
	{
		NameText = Cast<UTextBlock>(GetWidgetFromName(TEXT("NameText")));
	}
	if (!NameText)
	{
		NameText = Cast<UTextBlock>(GetWidgetFromName(TEXT("ItemNameText")));
	}
	if (!NameText)
	{
		NameText = Cast<UTextBlock>(GetWidgetFromName(TEXT("DetailNameText")));
	}

	if (!DescriptionText)
	{
		DescriptionText = Cast<UTextBlock>(GetWidgetFromName(TEXT("DescriptionText")));
	}
	if (!DescriptionText)
	{
		DescriptionText = Cast<UTextBlock>(GetWidgetFromName(TEXT("ItemDescriptionText")));
	}
	if (!DescriptionText)
	{
		DescriptionText = Cast<UTextBlock>(GetWidgetFromName(TEXT("DetailDescriptionText")));
	}

	if (!StatsList)
	{
		StatsList = Cast<UPanelWidget>(GetWidgetFromName(TEXT("StatsList")));
	}
	if (!StatsList)
	{
		StatsList = Cast<UPanelWidget>(GetWidgetFromName(TEXT("StatList")));
	}
	if (!StatsList)
	{
		StatsList = Cast<UPanelWidget>(GetWidgetFromName(TEXT("VB_Stats")));
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
