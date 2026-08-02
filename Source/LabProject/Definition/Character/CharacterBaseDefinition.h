#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "CharacterBaseDefinition.generated.h"

class UAnimInstance;
class UGameplayEffect;
class UMatchRuleDefinition;

USTRUCT(BlueprintType)
struct LABPROJECT_API FCharacterAbilityRuntimeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Ability", meta = (ClampMin = "0"))
	int32 ActorInfoMaxRetryAttempts = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Ability", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float ActorInfoRetryInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Stamina", meta = (AssetBundles = "Server"))
	TSubclassOf<UGameplayEffect> StaminaRegenEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Stamina", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float StaminaRegenDelay = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Stamina", meta = (ClampMin = "1.0"))
	float StaminaRegenEffectLevel = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Movement", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float MinimumMaxWalkSpeed = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Movement", meta = (ClampMin = "0"))
	int32 MovementSpeedAttributeMaxRetryAttempts = 50;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FCharacterHealthBarSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Health Bar", meta = (ClampMin = "0.01"))
	float WorldScale = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Health Bar")
	bool bHideWhenCharacterNotVisible = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Health Bar")
	bool bUseLineOfSightCheck = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Health Bar")
	bool bShowLocalPlayerHealthBar = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Health Bar", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float VisibilityTargetZOffset = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Health Bar", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float HideGraceTime = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Health Bar", meta = (ClampMin = "0"))
	int32 ViewModelMaxRetryAttempts = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Health Bar", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float ViewModelRetryInterval = 0.1f;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FCharacterPresentationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Animation", meta = (AssetBundles = "Client"))
	TSubclassOf<UAnimInstance> DefaultAnimLayer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Aura", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxBodyAuraRelativeOffsetDistance = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Aura", meta = (ClampMin = "0.01"))
	float MaxBodyAuraRelativeScale = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Team")
	bool bApplyTeamOverlayMaterial = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Team", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UMatchRuleDefinition> MatchRuleDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Team", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float TeamOverlayMaterialRetryInterval = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Team", meta = (ClampMin = "0"))
	int32 TeamOverlayMaterialRetryAttempts = 20;
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
	static FSoftObjectPath GetHumanoidDefinitionPath();

	const FCharacterAbilityRuntimeSettings& GetAbilityRuntimeSettings() const { return AbilityRuntime; }
	const FCharacterHealthBarSettings& GetHealthBarSettings() const { return HealthBar; }
	const FCharacterPresentationSettings& GetPresentationSettings() const { return Presentation; }
	const FCharacterDeathSettings& GetDeathSettings() const { return Death; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character", meta = (AllowPrivateAccess = "true"))
	FCharacterAbilityRuntimeSettings AbilityRuntime;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character", meta = (AllowPrivateAccess = "true"))
	FCharacterHealthBarSettings HealthBar;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character", meta = (AllowPrivateAccess = "true"))
	FCharacterPresentationSettings Presentation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character", meta = (AllowPrivateAccess = "true"))
	FCharacterDeathSettings Death;
};
