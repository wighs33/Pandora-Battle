#include "Localization/MenuLocalizationSubsystem.h"
#include "Localization/MenuLocalizationSettings.h"
#include "Localization/UiSettingsSaveGame.h"
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
	// 항상 필요한 작은 UI 리소스라 첫 화면을 만들기 전에 한 번 동기 로드한다.
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

bool UMenuLocalizationSubsystem::SetLanguage(EGuideLanguage NewLanguage)
{
	if (!UiLanguage::IsSupported(NewLanguage)) return false;
	const bool bChanged = Language != NewLanguage;
	Language = NewLanguage;
	UUiSettingsSaveGame* Saved = Cast<UUiSettingsSaveGame>(UGameplayStatics::CreateSaveGameObject(UUiSettingsSaveGame::StaticClass()));
	Saved->Language = Language;
	const bool bSaved = UGameplayStatics::SaveGameToSlot(Saved, SettingsSlot, 0);
	if (!bSaved) UE_LOG(LogTemp, Warning, TEXT("Menu Localization: could not save language selection."));
	if (bChanged) OnLanguageChanged.Broadcast();
	return bSaved;
}

FText UMenuLocalizationSubsystem::GetText(FName Key) const
{
	return GetTextOrFallback(Key, FText::FromName(Key));
}

FText UMenuLocalizationSubsystem::GetTextOrFallback(FName Key, const FText& Fallback) const
{
	// 데이터테이블 CSV 임포트는 행 이름의 공백을 지운다. 세션이 쓰는 맵 식별자에는 공백이 있을 수 있어
	// 조회 키만 공백을 지워 맞춘다.
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
	// 짧은 이름이 같은 상품이 번역을 나눠 쓰지 않도록 애셋 전체 경로를 키로 쓴다.
	const FName Key(*FString::Printf(TEXT("Product.%s.%s"), *Product->GetPathName(), *Field.ToString()));
	return GetTextOrFallback(Key, Fallback);
}
