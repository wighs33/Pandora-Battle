#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "PlayerPandoraData.generated.h"

USTRUCT(BlueprintType)
struct LABPROJECT_API FPlayerPandoraData
{
	GENERATED_BODY()

	// Persisted Pandora ownership uses stable Primary Asset IDs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora")
	TMap<FPrimaryAssetId, int32> GrantedPandorasById;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora")
	FPrimaryAssetId SelectedPandoraId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora")
	TMap<FName, FPrimaryAssetId> PandoraLoadoutByDirectionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora")
	int32 PandoraPoints = -1;
};
