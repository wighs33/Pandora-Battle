#include "Settings/UiLanguage.h"

const TArray<EGuideLanguage>& UiLanguage::All()
{
	static const TArray<EGuideLanguage> Languages = {
		EGuideLanguage::English, EGuideLanguage::SimplifiedChinese, EGuideLanguage::Russian,
		EGuideLanguage::Spanish, EGuideLanguage::PortugueseBrazil, EGuideLanguage::German,
		EGuideLanguage::Japanese, EGuideLanguage::French, EGuideLanguage::Polish,
		EGuideLanguage::Korean, EGuideLanguage::TraditionalChinese, EGuideLanguage::Turkish};
	return Languages;
}

bool UiLanguage::IsSupported(EGuideLanguage Language) { return All().Contains(Language); }

bool UiLanguage::UsesCjkFont(EGuideLanguage Language)
{
	return Language == EGuideLanguage::Korean || Language == EGuideLanguage::Japanese
		|| Language == EGuideLanguage::SimplifiedChinese || Language == EGuideLanguage::TraditionalChinese;
}

FString UiLanguage::NativeName(EGuideLanguage Language)
{
	switch (Language)
	{
	case EGuideLanguage::Korean: return TEXT("한국어");
	case EGuideLanguage::English: return TEXT("English");
	case EGuideLanguage::Japanese: return TEXT("日本語");
	case EGuideLanguage::SimplifiedChinese: return TEXT("简体中文");
	case EGuideLanguage::Spanish: return TEXT("Español (España)");
	case EGuideLanguage::Russian: return TEXT("Русский");
	case EGuideLanguage::PortugueseBrazil: return TEXT("Português (Brasil)");
	case EGuideLanguage::German: return TEXT("Deutsch");
	case EGuideLanguage::French: return TEXT("Français");
	case EGuideLanguage::Polish: return TEXT("Polski");
	case EGuideLanguage::TraditionalChinese: return TEXT("繁體中文");
	case EGuideLanguage::Turkish: return TEXT("Türkçe");
	default: return FString();
	}
}
