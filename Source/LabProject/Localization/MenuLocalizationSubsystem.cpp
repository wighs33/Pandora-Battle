#include "Localization/MenuLocalizationSubsystem.h"
#include "Localization/MenuLocalizationSettings.h"
#include "Localization/UiSettingsSaveGame.h"
#include "Common/UiLanguage.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "Misc/ConfigCacheIni.h"

DEFINE_LOG_CATEGORY_STATIC(LogMenuLocalization, Log, All);

namespace
{
	// LOCTABLE_FROMFILE_GAME의 ID와 같아야 한다. 매크로는 수집기가 읽도록 문자열을 그대로 받는다.
	const FName MenuTextTableId(TEXT("MenuText"));
	const FString LegacySettingsSlot(TEXT("MenuLanguageSettings"));
	const FString NativeCulture(TEXT("en"));
}

void UMenuLocalizationSubsystem::RegisterMenuTextTable()
{
	LOCTABLE_FROMFILE_GAME("MenuText", "MenuText", "UI/Localization/StringTables/MenuText.csv");
}

void UMenuLocalizationSubsystem::UnregisterMenuTextTable()
{
	FStringTableRegistry::Get().UnregisterStringTable(MenuTextTableId);
}

void UMenuLocalizationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const UMenuLocalizationSettings* Settings = GetDefault<UMenuLocalizationSettings>();
	// 항상 필요한 작은 UI 리소스라 첫 화면을 만들기 전에 한 번 동기 로드한다.
	CjkFont = Settings->CjkFont.LoadSynchronous();
	LatinFont = Settings->LatinFont.LoadSynchronous();
	if (!FStringTableRegistry::Get().FindStringTable(MenuTextTableId))
	{
		UE_LOG(LogMenuLocalization, Error, TEXT("String table '%s' is not registered."), *MenuTextTableId.ToString());
	}
	CultureChangedHandle = FInternationalization::Get().OnCultureChanged().AddUObject(this, &ThisClass::HandleCultureChanged);
	MoveLegacyLanguageSelection();
}

void UMenuLocalizationSubsystem::Deinitialize()
{
	FInternationalization::Get().OnCultureChanged().Remove(CultureChangedHandle);
	CultureChangedHandle.Reset();
	Super::Deinitialize();
}

// 1.0.2까지는 고른 언어를 세이브 슬롯에 저장했다. GameUserSettings에 고른 언어가 없을 때만 한 번 옮기고 슬롯을 지운다.
void UMenuLocalizationSubsystem::MoveLegacyLanguageSelection()
{
	FString SavedCulture;
	if (GIsEditor || GConfig->GetString(TEXT("Internationalization"), TEXT("Culture"), SavedCulture, GGameUserSettingsIni)
		|| !UGameplayStatics::DoesSaveGameExist(LegacySettingsSlot, 0))
	{
		return;
	}
	if (const UUiSettingsSaveGame* Saved = Cast<UUiSettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(LegacySettingsSlot, 0)))
	{
		SetLanguage(UiLanguage::FromLegacy(Saved->Language));
	}
	UGameplayStatics::DeleteGameInSlot(LegacySettingsSlot, 0);
}

FString UMenuLocalizationSubsystem::GetLanguage() const
{
	const FString Supported = UiLanguage::FindSupported(FInternationalization::Get().GetCurrentLanguage()->GetName());
	return Supported.IsEmpty() ? NativeCulture : Supported;
}

bool UMenuLocalizationSubsystem::SetLanguage(const FString& Culture)
{
	if (!UiLanguage::Cultures().Contains(Culture))
	{
		return false;
	}
	if (!UKismetInternationalizationLibrary::SetCurrentCulture(Culture, true))
	{
		UE_LOG(LogMenuLocalization, Warning, TEXT("Could not switch to culture '%s'."), *Culture);
		return false;
	}
	return true;
}

FText UMenuLocalizationSubsystem::GetText(FName Key)
{
	return GetTextOrFallback(Key, FText::FromName(Key));
}

FText UMenuLocalizationSubsystem::GetTextOrFallback(FName Key, const FText& Fallback)
{
	// 표의 키에는 공백이 없다. 세션이 쓰는 맵 식별자에는 공백이 있을 수 있어 조회 키만 공백을 지워 맞춘다.
	const FString LookupKey = Key.ToString().Replace(TEXT(" "), TEXT(""));
	const FStringTableConstPtr Table = FStringTableRegistry::Get().FindStringTable(MenuTextTableId);
	return Table && Table->FindEntry(LookupKey) ? FText::FromStringTable(MenuTextTableId, LookupKey) : Fallback;
}

UFont* UMenuLocalizationSubsystem::GetFontForLanguage(const FString& Culture) const
{
	return UiLanguage::UsesCjkFont(Culture) ? CjkFont.Get() : LatinFont.Get();
}

void UMenuLocalizationSubsystem::ApplyLanguageFont(UTextBlock& Text) const
{
	UFont* Font = GetCurrentFont();
	if (!Font)
	{
		return;
	}
	FSlateFontInfo Info = Text.GetFont();
	Info.FontObject = Font;
	Info.TypefaceFontName = NAME_None;
	Text.SetFont(Info);
}

void UMenuLocalizationSubsystem::HandleCultureChanged()
{
	OnLanguageChanged.Broadcast();
}
