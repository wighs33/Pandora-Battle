#include "Definition/UI/MenuTextRow.h"

const FText& FMenuTextRow::GetTranslation(EGuideLanguage Language) const
{
	switch (Language)
	{
	case EGuideLanguage::Korean: return Korean;
	case EGuideLanguage::Japanese: return Japanese;
	case EGuideLanguage::SimplifiedChinese: return SimplifiedChinese;
	case EGuideLanguage::Spanish: return Spanish;
	case EGuideLanguage::Russian: return Russian;
	case EGuideLanguage::PortugueseBrazil: return PortugueseBrazil;
	case EGuideLanguage::German: return German;
	case EGuideLanguage::French: return French;
	case EGuideLanguage::Polish: return Polish;
	case EGuideLanguage::TraditionalChinese: return TraditionalChinese;
	case EGuideLanguage::Turkish: return Turkish;
	default: return English;
	}
}

FText FMenuTextRow::Resolve(EGuideLanguage Language) const
{
	const FText& Translation = GetTranslation(Language);
	if (!Translation.IsEmptyOrWhitespace()) return Translation;
	return !English.IsEmptyOrWhitespace() ? English : Korean;
}
