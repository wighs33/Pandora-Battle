#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Settings/UiLanguage.h"
#include "UiSettingsSaveGame.generated.h"

UCLASS()
class LABPROJECT_API UUiSettingsSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) EGuideLanguage Language = EGuideLanguage::Korean;
};
