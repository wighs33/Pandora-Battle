#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"
#include "ExperienceDefinition.generated.h"

class APawn;

UCLASS(BlueprintType, Const)
class LABPROJECT_API UExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UExperienceDefinition();

	//------------------------------------------------------------------------------------------------------------------
	//--- Primary Asset
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

public:
	//------------------------------------------------------------------------------------------------------------------
	//--- Gameplay Setup
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Experience|Gameplay")
	TSubclassOf<APawn> DefaultPawnClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Experience|Game Features", meta = (AllowedTypes = "GameFeatureData"))
	TArray<FPrimaryAssetId> GameFeaturesToEnable;
};
