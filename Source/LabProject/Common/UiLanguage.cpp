#include "Common/UiLanguage.h"

#include "Internationalization/Internationalization.h"

const TArray<FString>& UiLanguage::Cultures()
{
	static const TArray<FString> Names = {
		TEXT("en"), TEXT("zh-Hans"), TEXT("ru"), TEXT("es-ES"), TEXT("pt-BR"), TEXT("de"),
		TEXT("ja"), TEXT("fr"), TEXT("pl"), TEXT("ko"), TEXT("zh-Hant"), TEXT("tr")};
	return Names;
}

FString UiLanguage::FindSupported(const FString& Culture)
{
	for (const FString& Candidate : FInternationalization::Get().GetPrioritizedCultureNames(Culture))
	{
		if (Cultures().Contains(Candidate))
		{
			return Candidate;
		}
	}
	return FString();
}

bool UiLanguage::UsesCjkFont(const FString& Culture)
{
	return Culture == TEXT("ko") || Culture == TEXT("ja") || Culture == TEXT("zh-Hans") || Culture == TEXT("zh-Hant");
}

FString UiLanguage::NativeName(const FString& Culture)
{
	static const TMap<FString, FString> Names = {
		{TEXT("ko"), TEXT("한국어")},
		{TEXT("en"), TEXT("English")},
		{TEXT("ja"), TEXT("日本語")},
		{TEXT("zh-Hans"), TEXT("简体中文")},
		{TEXT("es-ES"), TEXT("Español (España)")},
		{TEXT("ru"), TEXT("Русский")},
		{TEXT("pt-BR"), TEXT("Português (Brasil)")},
		{TEXT("de"), TEXT("Deutsch")},
		{TEXT("fr"), TEXT("Français")},
		{TEXT("pl"), TEXT("Polski")},
		{TEXT("zh-Hant"), TEXT("繁體中文")},
		{TEXT("tr"), TEXT("Türkçe")}};
	const FString* Name = Names.Find(Culture);
	return Name ? *Name : FString();
}

FString UiLanguage::FromLegacy(const EGuideLanguage Language)
{
	switch (Language)
	{
	case EGuideLanguage::Korean: return TEXT("ko");
	case EGuideLanguage::English: return TEXT("en");
	case EGuideLanguage::Japanese: return TEXT("ja");
	case EGuideLanguage::SimplifiedChinese: return TEXT("zh-Hans");
	case EGuideLanguage::Spanish: return TEXT("es-ES");
	case EGuideLanguage::Russian: return TEXT("ru");
	case EGuideLanguage::PortugueseBrazil: return TEXT("pt-BR");
	case EGuideLanguage::German: return TEXT("de");
	case EGuideLanguage::French: return TEXT("fr");
	case EGuideLanguage::Polish: return TEXT("pl");
	case EGuideLanguage::TraditionalChinese: return TEXT("zh-Hant");
	case EGuideLanguage::Turkish: return TEXT("tr");
	default: return FString();
	}
}
