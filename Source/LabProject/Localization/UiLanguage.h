#pragma once

#include "CoreMinimal.h"
#include "UiLanguage.generated.h"

// Keep the reflected name and numeric values: existing DA_Guide assets serialize this enum.
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
