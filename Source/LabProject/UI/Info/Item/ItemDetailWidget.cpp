#include "UI/Info/Item/ItemDetailWidget.h"

#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Item/ItemInstance.h"
#include "Definition/Skin/SkinDefinition.h"
#include "UI/Info/Item/ItemViewData.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "Engine/Font.h"
#include "Localization/MenuLocalizationSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ItemDetailWidget)

namespace
{
	FString FormatStatMagnitude(const float Magnitude)
	{
		const float SafeMagnitude = FMath::IsFinite(Magnitude) ? Magnitude : 0.0f;
		return FString::FromInt(FMath::RoundToInt(SafeMagnitude));
	}

	int32 RoundStatMagnitude(const float Magnitude)
	{
		return FMath::RoundToInt(FMath::IsFinite(Magnitude) ? Magnitude : 0.0f);
	}
}

void UItemDetailWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UMenuLocalizationSubsystem* Localization = GetLocalization())
	{
		Localization->OnLanguageChanged.AddUniqueDynamic(this, &ThisClass::RefreshLocalizedDetails);
	}
	RefreshLocalizedDetails();
}

void UItemDetailWidget::NativeDestruct()
{
	if (UMenuLocalizationSubsystem* Localization = GetLocalization())
	{
		Localization->OnLanguageChanged.RemoveDynamic(this, &ThisClass::RefreshLocalizedDetails);
	}
	Super::NativeDestruct();
}

UMenuLocalizationSubsystem* UItemDetailWidget::GetLocalization() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UMenuLocalizationSubsystem>() : nullptr;
}

void UItemDetailWidget::RefreshLocalizedDetails()
{
	if (DisplayedItem.IsValid())
	{
		SetItem(DisplayedItem.Get());
	}
	else if (DisplayedSkin.IsValid())
	{
		SetSkinDefinition(DisplayedSkin.Get());
	}
	ApplyLocalizedFont();
}

void UItemDetailWidget::ApplyLocalizedFont() const
{
	const UMenuLocalizationSubsystem* Localization = GetLocalization();
	UFont* Font = Localization ? Localization->GetFontForLanguage(Localization->GetLanguage()) : nullptr;
	if (!Font || !WidgetTree) return;
	WidgetTree->ForEachWidget([Font](UWidget* Widget)
	{
		if (UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			FSlateFontInfo Info = Text->GetFont();
			Info.FontObject = Font;
			Info.TypefaceFontName = NAME_None;
			Text->SetFont(Info);
		}
	});
}

void UItemDetailWidget::SetItem(UItemInstance* InItemInstance, UItemInstance* InCompareItemInstance)
{
	(void)InCompareItemInstance;

	const FItemViewData ViewData = FItemViewDataBuilder::FromItemInstance(InItemInstance, GetLocalization());
	SetItemViewData(ViewData);
	DisplayedItem = InItemInstance;
}

void UItemDetailWidget::SetItemViewData(const FItemViewData& InViewData)
{
	DisplayedItem.Reset();
	DisplayedSkin.Reset();
	if (!InViewData.HasContent())
	{
		ClearDetails();
		return;
	}

	SetHeader(InViewData);
	PopulateStats(InViewData.Stats, InViewData.UpgradeBonusStats);
	ApplyLocalizedFont();
}

void UItemDetailWidget::SetSkinDefinition(const USkinDefinition* SkinDefinition)
{
	const FItemViewData ViewData = FItemViewDataBuilder::FromSkinDefinition(SkinDefinition, GetLocalization());
	SetItemViewData(ViewData);
	DisplayedSkin = SkinDefinition;
}

void UItemDetailWidget::ClearDetails()
{
	DisplayedItem.Reset();
	DisplayedSkin.Reset();
	SetHeader(FItemViewData());

	if (StatsList)
	{
		StatsList->ClearChildren();
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

void UItemDetailWidget::SetHeader(const FItemViewData& ViewData) const
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

void UItemDetailWidget::PopulateStats(const TMap<FGameplayTag, float>& NewStats,
	const TMap<FGameplayTag, float>& UpgradeBonusStats)
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
		AddStatRow(StatTag, NewValue, UpgradeBonusStats.FindRef(StatTag));
	}
}

void UItemDetailWidget::AddStatRow(const FGameplayTag StatTag, const float NewValue, const float UpgradeBonusValue)
{
	if (!StatsList)
	{
		return;
	}

	FString ValueText = FormatStatMagnitude(NewValue);
	const int32 RoundedUpgradeBonus = RoundStatMagnitude(UpgradeBonusValue);
	if (RoundedUpgradeBonus != 0)
	{
		const FString UpgradeSign = RoundedUpgradeBonus > 0 ? TEXT("+") : TEXT("");
		ValueText += FString::Printf(TEXT(" (%s%d)"),
			*UpgradeSign,
			RoundedUpgradeBonus);
	}

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
