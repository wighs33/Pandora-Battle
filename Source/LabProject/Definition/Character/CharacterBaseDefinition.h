#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "CharacterBaseDefinition.generated.h"

class UAnimInstance;

USTRUCT(BlueprintType)
struct LABPROJECT_API FCharacterPresentationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Animation", meta = (AssetBundles = "Client"))
	TSubclassOf<UAnimInstance> DefaultAnimLayer;

};

USTRUCT(BlueprintType)
struct LABPROJECT_API FCharacterDeathSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Death", meta = (ClampMin = "0.0"))
	float ImpulseHorizontalStrength = 35000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Death", meta = (ClampMin = "0.0"))
	float ImpulseUpwardStrength = 12000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Death")
	float ImpulseSideStrength = 8000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Death", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ImpulseLocationZOffset = 80.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Death|Dissolve")
	bool bUseDissolve = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Death|Dissolve")
	FName DissolveScalarParameterName = TEXT("Dissolve");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Death|Dissolve")
	float DissolveInitialValue = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Death|Dissolve")
	float DissolveTargetValue = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Death|Dissolve", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float DissolveFallbackDuration = 1.0f;
};

UCLASS(BlueprintType, Const)
class LABPROJECT_API UCharacterBaseDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	static FSoftObjectPath GetDefaultDefinitionPath();

	const FCharacterPresentationSettings& GetPresentationSettings() const { return Presentation; }
	const FCharacterDeathSettings& GetDeathSettings() const { return Death; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character", meta = (AllowPrivateAccess = "true"))
	FCharacterPresentationSettings Presentation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character", meta = (AllowPrivateAccess = "true"))
	FCharacterDeathSettings Death;
};
