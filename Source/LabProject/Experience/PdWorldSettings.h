#pragma once

#include "CoreMinimal.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/PrimaryAssetId.h"
#include "PdWorldSettings.generated.h"

UCLASS()
class LABPROJECT_API APdWorldSettings : public AWorldSettings
{
	GENERATED_BODY()

public:
	APdWorldSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Experience Setup
	FPrimaryAssetId GetDefaultExperienceId() const { return DefaultExperienceId; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Experience", meta = (AllowedTypes = "ExperienceDefinition", AllowPrivateAccess = "true"))
	FPrimaryAssetId DefaultExperienceId;
};
