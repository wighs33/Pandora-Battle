#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "PlayerPandoraData.generated.h"

USTRUCT(BlueprintType)
struct LABPROJECT_API FPlayerPandoraData
{
	GENERATED_BODY()

	// Primary source for persisted Pandora ownership. AssetManager redirects keep renamed assets recoverable.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora")
	TMap<FPrimaryAssetId, int32> GrantedPandorasById;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora")
	FPrimaryAssetId SelectedPandoraId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora")
	TMap<FName, FPrimaryAssetId> PandoraLoadoutByDirectionId;

	// Legacy name fields are read only during schema migration.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora")
	TMap<FName, int32> GrantedPandorasByName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora")
	int32 PandoraPoints = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora")
	FName SelectedPandoraName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora")
	TMap<FName, FName> PandoraLoadoutByDirection;
};
