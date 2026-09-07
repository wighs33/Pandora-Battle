#pragma once

#include "Common/WeaponDefinitionData.h"
#include "CoreMinimal.h"
#include "Definition/Common/CombatSettings.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "PlayerPawnDefinition.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UNiagaraSystem;

USTRUCT(BlueprintType)
struct LABPROJECT_API FPlayerInteractionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Interaction",
		meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ServerValidationDistance = 250.0f;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPlayerCameraPresentationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Camera|Occlusion")
	FName OcclusionEnabledParameterName = TEXT("OcclusionEnabled");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Camera|Occlusion",
		meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float OcclusionDisableDistance = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Camera|Occlusion",
		meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float OcclusionReenableDistance = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Camera|Occlusion")
	TArray<TEnumAsByte<ECollisionChannel>> OcclusionSurfaceObjectTypes;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPlayerAimSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Aim|Network",
		meta = (ClampMin = "0.05", ForceUnits = "s"))
	float ReplicationInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Aim|Movement")
	FRotator DefaultRotationRate = FRotator(0.0f, 500.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Aim|Movement")
	FRotator AimingRotationRate = FRotator(0.0f, 3000.0f, 0.0f);
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPlayerActionPolicySettings
{
	GENERATED_BODY()

	/** Abilities with any of these tags may be canceled when locomotion interrupts a hit reaction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Action",
		meta = (Categories = "Action,GameplayAbility"))
	FGameplayTagContainer MovementHitReactCancelTags;
};

/**
 * Data-driven composition settings for APdPlayer.
 *
 * Components consume their own fragment and APdPlayer remains a compatibility
 * facade for existing Blueprint and native call sites.
 */
UCLASS(BlueprintType, Const)
class LABPROJECT_API UPlayerPawnDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPlayerPawnDefinition();
	virtual void PostLoad() override;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	static FPrimaryAssetId GetDefaultPrimaryAssetId();
	static FSoftObjectPath GetDefaultDefinitionPath();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	const FPlayerInteractionSettings& GetInteractionSettings() const { return Interaction; }
	const FPlayerCameraPresentationSettings& GetCameraSettings() const { return Camera; }
	const FPlayerAimSettings& GetAimSettings() const { return Aim; }
	const FPlayerActionPolicySettings& GetActionPolicySettings() const { return ActionPolicy; }
	const FCombatDamageSettings& GetCombatDamageSettings() const { return CombatDamageSettings; }
	const FUnarmedCombatSettings& GetUnarmedCombatSettings() const { return UnarmedCombatSettings; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Interaction",
		meta = (AllowPrivateAccess = "true"))
	FPlayerInteractionSettings Interaction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Camera",
		meta = (AllowPrivateAccess = "true"))
	FPlayerCameraPresentationSettings Camera;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Aim",
		meta = (AllowPrivateAccess = "true"))
	FPlayerAimSettings Aim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Action",
		meta = (AllowPrivateAccess = "true"))
	FPlayerActionPolicySettings ActionPolicy;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Combat|Unarmed",
		meta = (AllowPrivateAccess = "true"))
	FUnarmedCombatSettings UnarmedCombatSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Combat|Damage", meta = (AllowPrivateAccess = "true"))
	FCombatDamageSettings CombatDamageSettings;
};
