#pragma once

#include "CoreMinimal.h"
#include "UiLanguage.generated.h"

/** 1.0.2까지 언어 선택을 저장하던 슬롯의 값. 그 슬롯을 Culture 설정으로 옮길 때만 읽는다. */
UENUM()
enum class EGuideLanguage : uint8
{
	Korean,
	English,
	Japanese,
	SimplifiedChinese,
	Spanish,
	Russian,
	PortugueseBrazil,
	German,
	French,
	Polish,
	TraditionalChinese,
	Turkish
};

/** 게임이 번역을 갖춘 언어. 언어는 UE Culture 이름(예: "ko", "zh-Hans")으로 다룬다. */
namespace UiLanguage
{
	/** 설정 화면에 보이는 순서의 Culture 이름. */
	LABPROJECT_API const TArray<FString>& Cultures();
	/** Culture와 그 상위 Culture(예: "ko-KR" → "ko") 가운데 지원하는 첫 이름. 없으면 빈 문자열. */
	LABPROJECT_API FString FindSupported(const FString& Culture);
	LABPROJECT_API bool UsesCjkFont(const FString& Culture);
	/** 그 언어로 적은 언어 이름. 설정 화면이 어느 언어에서든 같은 이름을 보여 주도록 번역하지 않는다. */
	LABPROJECT_API FString NativeName(const FString& Culture);
	LABPROJECT_API FString FromLegacy(EGuideLanguage Language);
}
