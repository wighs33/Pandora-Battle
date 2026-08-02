#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "LobbyModeDefinition.generated.h"

class ULobbyPreviewDefinition;
class UMatchRuleDefinition;
class UWorld;

USTRUCT(BlueprintType)
struct LABPROJECT_API FLobbyFlowSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Flow",
		meta = (ClampMin = "0.0", ForceUnits = "s"))
	float FullLobbyAutoStartDelay = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Flow",
		meta = (ClampMin = "0.0", ForceUnits = "s"))
	float KickDisconnectDelay = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Respawn",
		meta = (ClampMin = "0.0", ForceUnits = "s"))
	float LobbyRespawnDelay = 3.0f;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FLobbyDedicatedSessionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Dedicated Session")
	bool bAutoCreateDedicatedServerSession = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Dedicated Session")
	FString DedicatedServerRoomName = TEXT("Dedicated Server");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Dedicated Session")
	bool bDedicatedServerSessionLAN = false;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FLobbyTravelSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Travel")
	TSoftObjectPtr<UWorld> RoomMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Travel")
	FString RoomTravelMapName = TEXT("/Game/Map/LV_Room");
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FLobbyContentSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Content",
		meta = (AllowedTypes = "MatchRuleDefinition"))
	TSoftObjectPtr<UMatchRuleDefinition> MatchRuleDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Content",
		meta = (AllowedTypes = "LobbyPreviewDefinition"))
	TSoftObjectPtr<ULobbyPreviewDefinition> LobbyPreviewDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Experience",
		meta = (AllowedTypes = "ExperienceDefinition"))
	FPrimaryAssetId DefaultExperienceId;
};

/**
 * Stable, cookable policy shared by the lobby GameMode runtime components.
 */
UCLASS(BlueprintType, Const)
class LABPROJECT_API ULobbyModeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	ULobbyModeDefinition();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	static FSoftObjectPath GetDefaultDefinitionPath();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	const FLobbyFlowSettings& GetFlowSettings() const { return Flow; }
	const FLobbyDedicatedSessionSettings& GetDedicatedSessionSettings() const { return DedicatedSession; }
	const FLobbyTravelSettings& GetTravelSettings() const { return Travel; }
	const FLobbyContentSettings& GetContentSettings() const { return Content; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Flow",
		meta = (AllowPrivateAccess = "true"))
	FLobbyFlowSettings Flow;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Dedicated Session",
		meta = (AllowPrivateAccess = "true"))
	FLobbyDedicatedSessionSettings DedicatedSession;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Travel",
		meta = (AllowPrivateAccess = "true"))
	FLobbyTravelSettings Travel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Content",
		meta = (AllowPrivateAccess = "true"))
	FLobbyContentSettings Content;
};
