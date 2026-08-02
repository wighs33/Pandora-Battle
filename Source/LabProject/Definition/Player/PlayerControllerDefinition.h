#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "PlayerControllerDefinition.generated.h"

class UControllerInputDefinition;

/**
 * Defines the trust boundary for cosmetics submitted by a remote player.
 *
 * TrustLocalCosmeticProfile is intended for local-only progression in an
 * unranked listen-server game. It preserves each player's local purchases, but
 * cannot make a client-owned SaveGame tamper-proof. DefaultUnlocksOnly is the
 * fail-closed option for a future ranked or paid entitlement model.
 */
UENUM(BlueprintType)
enum class EPdRemoteSkinClaimPolicy : uint8
{
	TrustLocalCosmeticProfile,
	DefaultUnlocksOnly
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPdControllerInputSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Input",
		meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UControllerInputDefinition> DefaultInputDefinition;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPdControllerPresentationSettings
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

USTRUCT(BlueprintType)
struct LABPROJECT_API FPdControllerProfileSyncSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Profile",
		meta = (ClampMin = "0.01", ForceUnits = "s"))
	float LocalShopSaveSyncInterval = 0.50f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Profile",
		meta = (ClampMin = "1"))
	int32 LocalShopSaveSyncMaxAttempts = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Profile",
		meta = (ClampMin = "1"))
	int32 MaxClientSyncedSkinNameCount = 512;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Profile")
	EPdRemoteSkinClaimPolicy RemoteSkinClaimPolicy =
		EPdRemoteSkinClaimPolicy::TrustLocalCosmeticProfile;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Profile",
		meta = (ClampMin = "0.0", ForceUnits = "s"))
	float RemoteSkinSyncMinInterval = 0.20f;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPdControllerDebugGrantSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Debug",
		meta = (ClampMin = "0"))
	int32 SoulDustGrantAmount = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Debug",
		meta = (ClampMin = "0.0"))
	float StatusPointGrantAmount = 50.0f;
};

/**
 * Data-driven composition settings for APdPlayerController.
 *
 * Each controller component consumes only its own fragment. APdPlayerController
 * remains the stable RPC and Blueprint facade.
 */
UCLASS(BlueprintType, Const)
class LABPROJECT_API UPlayerControllerDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPlayerControllerDefinition();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	static FSoftObjectPath GetDefaultDefinitionPath();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	const FPdControllerInputSettings& GetInputSettings() const { return Input; }
	const FPdControllerPresentationSettings& GetPresentationSettings() const { return Presentation; }
	const FPdControllerProfileSyncSettings& GetProfileSyncSettings() const { return ProfileSync; }
	const FPdControllerDebugGrantSettings& GetDebugGrantSettings() const { return DebugGrant; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Input",
		meta = (AllowPrivateAccess = "true"))
	FPdControllerInputSettings Input;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Presentation",
		meta = (AllowPrivateAccess = "true"))
	FPdControllerPresentationSettings Presentation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Profile",
		meta = (AllowPrivateAccess = "true"))
	FPdControllerProfileSyncSettings ProfileSync;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Debug",
		meta = (AllowPrivateAccess = "true"))
	FPdControllerDebugGrantSettings DebugGrant;
};
