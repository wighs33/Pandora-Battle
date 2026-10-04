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

	// 월드가 이 월드 설정을 쓰면 그 기본 Experience를, 아니면 빈 값을 돌려준다.
	static FPrimaryAssetId FindDefaultExperienceId(const UWorld* World);

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Experience", meta = (AllowedTypes = "ExperienceDefinition", AllowPrivateAccess = "true"))
	FPrimaryAssetId DefaultExperienceId;
};
