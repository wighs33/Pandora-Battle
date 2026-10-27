#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Common/UiLanguage.h"
#include "UiSettingsSaveGame.generated.h"

/** 1.0.2까지 언어 선택을 저장하던 슬롯. 지금은 GameUserSettings의 Culture가 저장하고, 남아 있는 슬롯은 한 번 옮긴 뒤 지운다. */
UCLASS()
class LABPROJECT_API UUiSettingsSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) EGuideLanguage Language = EGuideLanguage::Korean;
};
