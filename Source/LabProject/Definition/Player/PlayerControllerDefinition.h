#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "PlayerControllerDefinition.generated.h"

USTRUCT(BlueprintType)
struct LABPROJECT_API FControllerPresentationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Presentation|Travel",
		meta = (ClampMin = "0.01", ForceUnits = "s"))
	float TravelLoadingReadyCheckInterval = 0.10f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Presentation|Travel",
		meta = (ClampMin = "0"))
	int32 TravelLoadingReadyCheckMaxAttempts = 50;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Presentation|Respawn",
		meta = (ClampMin = "0.0", ForceUnits = "s"))
	float RespawnStateResetRetryDelay = 0.20f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Presentation|HealthBar",
		meta = (ClampMin = "0.01", ForceUnits = "s"))
	float HealthBarVisibilityUpdateInterval = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Presentation|HealthBar",
		meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float HealthBarVisibilityDistance = 6000.0f;
};

/**
 * Data-driven composition settings for APdPlayerController.
 *
 * Presentation settings for APdPlayerController. Input comes from DA_GameInstance,
 * while profile synchronization uses a fixed runtime policy.
 */
UCLASS(BlueprintType, Const)
class LABPROJECT_API UPlayerControllerDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	static FSoftObjectPath GetDefaultDefinitionPath();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	const FControllerPresentationSettings& GetPresentationSettings() const { return Presentation; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Presentation",
		meta = (AllowPrivateAccess = "true"))
	FControllerPresentationSettings Presentation;

};
