#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SavedGameData/PlayerPandoraData.h"
#include "PdSaveGame.generated.h"

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UPdSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora")
	FPlayerPandoraData PlayerPandoraData;
};
