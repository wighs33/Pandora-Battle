#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"
#include "ExperienceDefinition.generated.h"

class APawn;
class UStateTree;

UCLASS(BlueprintType, Const)
class LABPROJECT_API UExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UExperienceDefinition();

	//------------------------------------------------------------------------------------------------------------------
	//--- Primary Asset
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

public:
	//------------------------------------------------------------------------------------------------------------------
	//--- Gameplay Setup
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Experience|Gameplay")
	TSubclassOf<APawn> DefaultPawnClass;

	/** Required server-side behavior used by monster AI controllers in this Experience. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Experience|Gameplay")
	TObjectPtr<UStateTree> MonsterStateTree;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Experience|Game Features", meta = (AllowedTypes = "GameFeatureData"))
	TArray<FPrimaryAssetId> GameFeaturesToEnable;
};
