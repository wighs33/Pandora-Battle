#pragma once

#include "CoreMinimal.h"
#include "PlayerPandoraData.generated.h"

USTRUCT(BlueprintType)
struct LABPROJECT_API FPlayerPandoraData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora")
	TMap<FName, int32> GrantedPandorasByName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora")
	int32 PandoraPoints = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora")
	FName SelectedPandoraName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora")
	TMap<FName, FName> PandoraLoadoutByDirection;
};
