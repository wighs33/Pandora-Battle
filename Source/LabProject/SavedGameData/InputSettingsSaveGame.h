#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "InputSettingsSaveGame.generated.h"

UCLASS()
class LABPROJECT_API UInputSettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	bool bHasMouseSensitivitySetting = false;

	UPROPERTY()
	int32 MouseSensitivityPercent = 100;
};
