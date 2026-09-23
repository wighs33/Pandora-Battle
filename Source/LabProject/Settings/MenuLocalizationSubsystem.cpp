#include "Settings/MenuLocalizationSubsystem.h"
#include "Settings/MenuLocalizationSettings.h"
#include "Settings/UiSettingsSaveGame.h"
#include "Definition/UI/MenuTextRow.h"
#include "Engine/DataTable.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"

namespace { const FString SettingsSlot(TEXT("MenuLanguageSettings")); }

void UMenuLocalizationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const UMenuLocalizationSettings* Settings = GetDefault<UMenuLocalizationSettings>();
	Language = UiLanguage::IsSupported(Settings->DefaultLanguage) ? Settings->DefaultLanguage : EGuideLanguage::Korean;
	// These small, always-needed UI resources are loaded once, before the entry screen is constructed.
	TextTable = Settings->TextTable.LoadSynchronous();
	CjkFont = Settings->CjkFont.LoadSynchronous();
	LatinFont = Settings->LatinFont.LoadSynchronous();
	if (!TextTable || TextTable->GetRowStruct() != FMenuTextRow::StaticStruct())
	{
		UE_LOG(LogTemp, Error, TEXT("Menu Localization: configure a MenuTextRow table in Project Settings."));
		TextTable = nullptr;
	}
	if (const UUiSettingsSaveGame* Saved = Cast<UUiSettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(SettingsSlot, 0));
		Saved && UiLanguage::IsSupported(Saved->Language))
	{
		Language = Saved->Language;
	}
}

bool UMenuLocalizationSubsystem::SetLanguage(EGuideLanguage NewLanguage, bool bSaveImmediately)
{
	if (!UiLanguage::IsSupported(NewLanguage)) return false;
	const bool bChanged = Language != NewLanguage;
	Language = NewLanguage;
	bool bSaved = true;
	if (bSaveImmediately)
	{
		UUiSettingsSaveGame* Saved = Cast<UUiSettingsSaveGame>(UGameplayStatics::CreateSaveGameObject(UUiSettingsSaveGame::StaticClass()));
		Saved->Language = Language;
		bSaved = UGameplayStatics::SaveGameToSlot(Saved, SettingsSlot, 0);
		if (!bSaved) UE_LOG(LogTemp, Warning, TEXT("Menu Localization: could not save language selection."));
	}
	if (bChanged) OnLanguageChanged.Broadcast();
	return bSaved;
}

FText UMenuLocalizationSubsystem::GetText(FName Key) const
{
	return GetTextOrFallback(Key, FText::FromName(Key));
}

FText UMenuLocalizationSubsystem::GetTextOrFallback(FName Key, const FText& Fallback) const
{
	// DataTable CSV imports strip spaces from row names. Map identifiers used
	// by the session system may contain spaces; only normalize the lookup key.
	const FName LookupKey(*Key.ToString().Replace(TEXT(" "), TEXT("")));
	const FMenuTextRow* Row = TextTable ? TextTable->FindRow<FMenuTextRow>(LookupKey, TEXT("Menu localization"), false) : nullptr;
	if (Row)
	{
		const FText Result = Row->Resolve(Language);
		if (!Result.IsEmptyOrWhitespace()) return Result;
	}
	return Fallback;
}

UFont* UMenuLocalizationSubsystem::GetFontForLanguage(EGuideLanguage FontLanguage) const
{
	return UiLanguage::UsesCjkFont(FontLanguage) ? CjkFont.Get() : LatinFont.Get();
}

FText UMenuLocalizationSubsystem::GetProductText(const UObject* Product, FName Field, const FText& Fallback) const
{
	if (!Product) return Fallback;
	// Full asset path prevents products with the same short name from sharing translations.
	const FName Key(*FString::Printf(TEXT("Product.%s.%s"), *Product->GetPathName(), *Field.ToString()));
	return GetTextOrFallback(Key, Fallback);
}
