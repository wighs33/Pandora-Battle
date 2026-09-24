#pragma once

#include "CoreMinimal.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/PrimaryAssetId.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "PdWorldSettings.generated.h"

UCLASS()
class LABPROJECT_API APdWorldSettings : public AWorldSettings
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	// Public API ------------------------------------------------------------------------------------------------------
	APdWorldSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	FPrimaryAssetId GetDefaultExperienceId() const { return DefaultExperienceId; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Experience", meta = (AllowedTypes = "ExperienceDefinition", AllowPrivateAccess = "true"))
	FPrimaryAssetId DefaultExperienceId;
};
