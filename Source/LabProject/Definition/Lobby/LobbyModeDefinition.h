#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "LobbyModeDefinition.generated.h"

class UWorld;

USTRUCT(BlueprintType)
struct LABPROJECT_API FLobbyTravelSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Travel")
	TSoftObjectPtr<UWorld> TitleMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Travel")
	TSoftObjectPtr<UWorld> LobbyMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Travel")
	TSoftObjectPtr<UWorld> RoomMap;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FLobbyContentSettings
{
	GENERATED_BODY()

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
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	static FSoftObjectPath GetDefaultDefinitionPath();
	static const ULobbyModeDefinition* ResolveDefaultDefinition();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	const FLobbyTravelSettings& GetTravelSettings() const { return Travel; }
	const FLobbyContentSettings& GetContentSettings() const { return Content; }
	FString GetTitleTravelMapName() const;
	FString GetLobbyTravelMapName() const;
	FString GetRoomTravelMapName() const;
	bool IsLobbyMapName(const FString& LevelName) const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Travel",
		meta = (AllowPrivateAccess = "true"))
	FLobbyTravelSettings Travel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|Content",
		meta = (AllowPrivateAccess = "true"))
	FLobbyContentSettings Content;
};
