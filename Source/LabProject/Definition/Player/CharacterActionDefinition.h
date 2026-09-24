#pragma once

#include "CoreMinimal.h"
#include "Common/WeaponDefinitionData.h"
#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "CharacterActionDefinition.generated.h"

class UInputAction;

UENUM(BlueprintType)
enum class ECharacterActionType : uint8
{
	PandoraWeaponSwap UMETA(DisplayName = "Pandora / Weapon Swap"),
	GrappleHook UMETA(DisplayName = "Grapple Hook")
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FCharacterActionConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character Action")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character Action",
		meta = (AssetBundles = "Client", AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TSoftObjectPtr<UObject> IconResource;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character Action|Cooldown", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double CooldownDuration = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character Action|Input", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UInputAction> InputAction;

	// Public API ------------------------------------------------------------------------------------------------------
	UObject* LoadIconResource() const;
	UInputAction* LoadInputAction() const;
	UObject* GetLoadedIconResource() const;
	UInputAction* GetLoadedInputAction() const;
	void GetRuntimePreloadAssetPaths(TArray<FSoftObjectPath>& OutAssetPaths) const;
};

UCLASS(BlueprintType, Const)
class LABPROJECT_API UCharacterActionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	// Public API ------------------------------------------------------------------------------------------------------
	UCharacterActionDefinition();
	static FSoftObjectPath GetDefaultDefinitionPath();

	UFUNCTION(BlueprintPure, Category = "!Character Action")
	FText GetDisplayName(ECharacterActionType ActionType) const;

	UFUNCTION(BlueprintPure, Category = "!Character Action")
	UObject* GetIconResource(ECharacterActionType ActionType) const;

	UObject* GetLoadedIconResource(ECharacterActionType ActionType) const;

	UFUNCTION(BlueprintPure, Category = "!Character Action|Cooldown")
	double GetCooldownDuration(ECharacterActionType ActionType) const;

	UFUNCTION(BlueprintPure, Category = "!Character Action|Aim Camera")
	FWeaponAimCameraSettings GetAimCameraSettings(ECharacterActionType ActionType) const;
	UInputAction* LoadInputAction(ECharacterActionType ActionType) const;
	UInputAction* GetLoadedInputAction(ECharacterActionType ActionType) const;
	void GetRuntimePreloadAssetPaths(TArray<FSoftObjectPath>& OutAssetPaths) const;

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	const FCharacterActionConfig& FindActionConfig(ECharacterActionType ActionType) const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character Action|Pandora / Weapon Swap", meta = (AllowPrivateAccess = "true"))
	FCharacterActionConfig PandoraWeaponSwap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character Action|Grapple Hook", meta = (AllowPrivateAccess = "true"))
	FCharacterActionConfig GrappleHook;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character Action|Grapple Hook|Aim Camera", meta = (AllowPrivateAccess = "true"))
	FWeaponAimCameraSettings GrappleAimCameraSettings;
};
