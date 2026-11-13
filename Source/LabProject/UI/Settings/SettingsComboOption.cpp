#include "UI/Settings/SettingsComboOption.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"

namespace
{
	constexpr int32 OptionFontSize = 22;
	const FMargin OptionPadding(12.f, 6.f);
}

UTextBlock* SettingsComboOption::Make(UWidgetTree& Tree, const FString& Option, UFont* Font)
{
	UTextBlock* Text = Tree.ConstructWidget<UTextBlock>();
	Text->SetText(FText::AsCultureInvariant(Option));
	FSlateFontInfo FontInfo = Text->GetFont();
	FontInfo.Size = OptionFontSize;
	FontInfo.TypefaceFontName = NAME_None;
	if (Font)
	{
		FontInfo.FontObject = Font;
	}
	Text->SetFont(FontInfo);
	Text->SetColorAndOpacity(FSlateColor::UseForeground());
	Text->SetMargin(OptionPadding);
	return Text;
}
