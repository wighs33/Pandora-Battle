#pragma once

#include "CoreMinimal.h"
#include "Common/GameSessionConstants.h"
#include "Engine/DataAsset.h"
#include "Map/PdMapTypes.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "MatchRuleDefinition.generated.h"

class UMaterialInterface;
class UMapWidget;
class UTexture2D;
class UWorld;

UENUM(BlueprintType)
enum class EPdTeamColor : uint8
{
	Red UMETA(DisplayName = "Red"),
	Blue UMETA(DisplayName = "Blue"),
	Yellow UMETA(DisplayName = "Yellow"),
	Purple UMETA(DisplayName = "Purple"),
	Green UMETA(DisplayName = "Green"),
	Orange UMETA(DisplayName = "Orange")
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPdTeamOverlayMaterial
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Team")
	EPdTeamColor TeamColor = EPdTeamColor::Red;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Team")
	TObjectPtr<UMaterialInterface> OverlayMaterial;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FLobbyMatchMapOption
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Lobby")
	FName MapKey;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Lobby")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Lobby")
	TSoftObjectPtr<UWorld> Map;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Lobby")
	FString TravelMapName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Lobby", meta = (ClampMin = "1"))
	int32 MaxPlayerCount = LabGameSession::MaxPlayerCount;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Lobby", meta = (ClampMin = "0"))
	int32 DefaultMaxBotCount = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Lobby")
	TObjectPtr<UTexture2D> Thumbnail = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Lobby|Gameplay Map")
	TSoftClassPtr<UMapWidget> GameplayMapWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Lobby|Gameplay Map", meta = (FormerlySerializedAs = "InitialPlayerMapLayer"))
	EPdPlayerMapRegion InitialPlayerMapRegion = EPdPlayerMapRegion::Windmill;
};

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UMatchRuleDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UMatchRuleDefinition();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	static const FSoftObjectPath& GetDefaultDefinitionPath();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	static bool TryGetTeamColorForIndex(int32 TeamColorIndex, EPdTeamColor& OutTeamColor);

	UFUNCTION(BlueprintPure, Category = "!Match Rules|Team")
	UMaterialInterface* GetTeamOverlayMaterial(int32 TeamColorIndex) const;

	UFUNCTION(BlueprintPure, Category = "!Match Rules|Team")
	UMaterialInterface* GetTeamOverlayMaterialByTeamColor(EPdTeamColor TeamColor) const;

	UFUNCTION(BlueprintPure, Category = "!Match Rules|Lobby")
	bool GetLobbyMapOptionAtIndex(int32 Index, FLobbyMatchMapOption& OutMapOption) const;

	UFUNCTION(BlueprintPure, Category = "!Match Rules|Lobby")
	bool FindLobbyMapOption(FName MapKey, FLobbyMatchMapOption& OutMapOption) const;

	UFUNCTION(BlueprintPure, Category = "!Match Rules|Lobby")
	FName ResolveLobbyMapKey(FName MapKey) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Lobby", meta = (TitleProperty = "DisplayName"))
	TArray<FLobbyMatchMapOption> LobbyMapOptions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Lobby|Countdown",
		meta = (ClampMin = "0.0", ForceUnits = "s"))
	float LobbyStartCountdownSeconds = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Timer", meta = (ClampMin = "0.0"))
	float MatchTimerSeconds = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Timer")
	bool bEnableServerMatchTimer = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Timer")
	TArray<FName> MapsWithoutMatchTimer = { TEXT("LV_TrainingRoom") };

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Victory",
		meta = (DisplayName = "Golden Kill Enable", FormerlySerializedAs = "bForceMoveOnTimerTie"))
	bool bGoldenKillEnabled = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Team", meta = (TitleProperty = "TeamColor"))
	TArray<FPdTeamOverlayMaterial> TeamOverlayMaterials;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Respawn", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float PlayerRespawnDelay = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Respawn|Random Spawn")
	bool bUseRandomPlayerStartRespawns = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Respawn|Random Spawn")
	bool bAvoidLastRandomRespawnPlayerStart = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Respawn|Random Spawn")
	TArray<FName> RandomRespawnPlayerStartTags = { TEXT("Respawn") };

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Hud Timer")
	bool bHudCountDown = true;

	// Legacy asset field. The HUD now follows the replicated GameState phase and ignores this value.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Hud Timer|Legacy")
	bool bHudAutoStart = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Hud Timer")
	bool bHudHideWhenFinished = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Hud Timer", meta = (ClampMin = "0.01"))
	float HudTickInterval = 0.1f;

	// Legacy asset fields. Force movement is owned by the authoritative GameMode and these values are ignored.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Hud Timer|Legacy")
	bool bEnableHudForceMoveAtTime = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules|Hud Timer|Legacy")
	bool bTeleportOwningPawnOnHudForceMoveTime = true;

};
