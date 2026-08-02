#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "PlayerSkinData.generated.h"

USTRUCT(BlueprintType)
struct LABPROJECT_API FPlayerSkinData
{
	GENERATED_BODY()

	// Primary source for persisted skin ownership. AssetManager redirects keep renamed assets recoverable.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Skin")
	TMap<FPrimaryAssetId, int32> GrantedSkinsById;

	// Legacy name field retained only for one-time schema migration.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Skin")
	TMap<FName, int32> GrantedSkinsByName;
};
