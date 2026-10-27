#include "Tests/Integration/IntegrationTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Definition/AbilitySystem/SkillDefinition.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Engine/GameInstance.h"
#include "Localization/MenuLocalizationSubsystem.h"
#include "UObject/StrongObjectPtr.h"

// 메뉴 문구(문자열 테이블 MenuText)와 데이터 애셋의 텍스트가 같은 Game 지역화 타깃에서 번역을 받는지 -game 프로세스에서 본다.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdLanguageSwitchTest, "LabProject.Integration.Localization.LanguageSwitch",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

// 게임은 [Internationalization] Culture=ko로 시작하고, 언어를 고르면 Culture가 바뀌어 메뉴 문구와 애셋 텍스트가 함께 바뀐다.
bool FPdLanguageSwitchTest::RunTest(const FString& Parameters)
{
	const UWorld* World = PdIntegrationTest::GetGameWorld();
	UMenuLocalizationSubsystem* Localization = World && World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<UMenuLocalizationSubsystem>() : nullptr;
	const TStrongObjectPtr<const UPandoraDefinition> Angel(
		LoadObject<UPandoraDefinition>(nullptr, TEXT("/Game/Pandora/Data/DA_Pandora_Angel.DA_Pandora_Angel")));
	const TStrongObjectPtr<const USkillDefinition> BlackHole(
		LoadObject<USkillDefinition>(nullptr, TEXT("/Game/Pandora/Skill/Darkness/DA_Skill_BlackHole.DA_Skill_BlackHole")));
	if (!TestNotNull(TEXT("메뉴 언어 서브시스템"), Localization) || !TestNotNull(TEXT("판도라 정의"), Angel.Get())
		|| !TestNotNull(TEXT("스킬 정의"), BlackHole.Get()))
	{
		return false;
	}

	const FText Settings = UMenuLocalizationSubsystem::GetText(TEXT("Common.Settings"));
	const auto Expect = [this, &Settings, &Angel, &BlackHole](const TCHAR* Culture, const TCHAR* InSettings,
		const TCHAR* InAngel, const TCHAR* InBlackHole)
	{
		TestEqual(FString::Printf(TEXT("%s 메뉴 문구"), Culture), Settings.ToString(), FString(InSettings));
		TestEqual(FString::Printf(TEXT("%s 판도라 이름"), Culture), Angel->GetDisplayName().ToString(), FString(InAngel));
		TestEqual(FString::Printf(TEXT("%s 스킬 이름"), Culture), BlackHole->GetDisplayName().ToString(), FString(InBlackHole));
	};

	TestEqual(TEXT("처음 언어는 한국어다"), Localization->GetLanguage(), FString(TEXT("ko")));
	Expect(TEXT("ko"), TEXT("게임 설정"), TEXT("천사"), TEXT("블랙홀"));
	TestEqual(TEXT("표에 없는 키는 대체 문구를 쓴다"),
		UMenuLocalizationSubsystem::GetTextOrFallback(TEXT("Map.Missing Key"), FText::AsCultureInvariant(TEXT("Fallback"))).ToString(),
		FString(TEXT("Fallback")));

	TestTrue(TEXT("영어로 바꾼다"), Localization->SetLanguage(TEXT("en")));
	Expect(TEXT("en"), TEXT("Game settings"), TEXT("Angel"), TEXT("Blackhole"));
	TestTrue(TEXT("일본어로 바꾼다"), Localization->SetLanguage(TEXT("ja")));
	Expect(TEXT("ja"), TEXT("ゲーム設定"), TEXT("天使"), TEXT("ブラックホール"));

	TestTrue(TEXT("한국어로 되돌린다"), Localization->SetLanguage(TEXT("ko")));
	Expect(TEXT("ko"), TEXT("게임 설정"), TEXT("천사"), TEXT("블랙홀"));
	return true;
}

#endif
