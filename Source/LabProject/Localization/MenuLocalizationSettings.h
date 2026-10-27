#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MenuLocalizationSettings.generated.h"

class UFont;

/** 언어에 따라 바꿔 끼우는 메뉴 글꼴. 기본 언어는 [Internationalization] Culture가 정한다. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Menu Localization"))
class LABPROJECT_API UMenuLocalizationSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(Config, EditAnywhere, Category="Localization")
	TSoftObjectPtr<UFont> CjkFont;

	UPROPERTY(Config, EditAnywhere, Category="Localization")
	TSoftObjectPtr<UFont> LatinFont;
};
