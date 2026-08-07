#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "PlayerSkinData.generated.h"

USTRUCT(BlueprintType)
struct LABPROJECT_API FPlayerSkinData
{
	GENERATED_BODY()

	// Persisted skin ownership uses stable Primary Asset IDs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Skin")
	TMap<FPrimaryAssetId, int32> GrantedSkinsById;
};
