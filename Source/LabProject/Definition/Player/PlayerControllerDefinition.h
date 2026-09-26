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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Presentation|HealthBar",
		meta = (ClampMin = "0.01", ForceUnits = "s"))
	float HealthBarVisibilityUpdateInterval = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Presentation|HealthBar",
		meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float HealthBarVisibilityDistance = 6000.0f;
};

/**
 * APdPlayerController의 데이터 기반 구성 설정을 제공한다.
 *
 * 컨트롤러의 표시 설정을 정의한다. 입력 설정은 DA_GameInstance에서 가져오며,
 * 프로필 동기화는 고정된 실행 정책을 따른다.
 */
UCLASS(BlueprintType, Const)
class LABPROJECT_API UPlayerControllerDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	// Public API ------------------------------------------------------------------------------------------------------
	static FSoftObjectPath GetDefaultDefinitionPath();

	const FControllerPresentationSettings& GetPresentationSettings() const { return Presentation; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Presentation",
		meta = (AllowPrivateAccess = "true"))
	FControllerPresentationSettings Presentation;
};
