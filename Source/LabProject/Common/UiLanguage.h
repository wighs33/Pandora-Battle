#pragma once

#include "CoreMinimal.h"
#include "UiLanguage.generated.h"

// 기존 DA_Guide 애셋이 이 enum을 저장하므로 리플렉션 이름과 숫자 값을 바꾸지 않는다.
UENUM(BlueprintType)
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

namespace UiLanguage
{
	LABPROJECT_API const TArray<EGuideLanguage>& All();
	LABPROJECT_API bool IsSupported(EGuideLanguage Language);
	LABPROJECT_API bool UsesCjkFont(EGuideLanguage Language);
	LABPROJECT_API FString NativeName(EGuideLanguage Language);
}
