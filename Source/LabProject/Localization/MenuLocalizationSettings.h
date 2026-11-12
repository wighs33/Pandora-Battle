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
	UPROPERTY(Config, EditAnywhere, Category="!Localization")
	TSoftObjectPtr<UFont> CjkFont;

	UPROPERTY(Config, EditAnywhere, Category="!Localization")
	TSoftObjectPtr<UFont> LatinFont;

	/**
	 * 모든 언어의 글자가 들어 있는 표시용 글꼴(없는 글자는 대체 글꼴이 채운다). 이 글꼴로 정한 글자는 언어 글꼴로 바꾸지 않는다.
	 * 게임 시작처럼 힘을 줘야 하는 큰 글자에 쓴다.
	 */
	UPROPERTY(Config, EditAnywhere, Category="!Localization")
	TArray<TSoftObjectPtr<UFont>> DisplayFonts;
};
