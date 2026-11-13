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

	/**
	 * 숫자와 키 표시(수치, 개수, 시간, 퍼센트, 슬롯 번호, Q·E·R 같은 키)처럼 언어와 상관없는 글자에 쓰는 글꼴. 언어 글꼴로 바꾸지 않으므로
	 * 어느 언어에서나 같은 모양 · 같은 높이로 그려진다(언어마다 글꼴이 바뀌면 숫자가 그려지는 높이가 달라져 옆 아이콘과 어긋난다).
	 * 키 이름이 번역되기도 하므로(좌SHIFT) 모든 언어의 글자가 들어 있는 글꼴이어야 한다. 단어가 섞인 문구(1,200 골드)는 언어 글꼴을 쓴다.
	 */
	UPROPERTY(Config, EditAnywhere, Category="!Localization")
	TSoftObjectPtr<UFont> NumberFont;
};
