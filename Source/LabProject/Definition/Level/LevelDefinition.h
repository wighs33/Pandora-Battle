#pragma once

#include "CoreMinimal.h"
#include "Common/GameSessionConstants.h"
#include "Engine/DataAsset.h"
#include "Map/PdMapTypes.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "LevelDefinition.generated.h"

class UMapWidget;
class UTexture2D;
class UWorld;

USTRUCT(BlueprintType, meta = (DisplayName = "Ingame Level Option"))
struct LABPROJECT_API FLobbyMatchMapOption
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ingame Level")
	FName MapKey;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ingame Level")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ingame Level")
	TSoftObjectPtr<UWorld> Map;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ingame Level")
	FString TravelMapName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ingame Level", meta = (ClampMin = "1"))
	int32 MaxPlayerCount = LabGameSession::MaxPlayerCount;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ingame Level")
	TObjectPtr<UTexture2D> Thumbnail = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ingame Level|Gameplay Map")
	TSoftClassPtr<UMapWidget> GameplayMapWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ingame Level|Gameplay Map",
		meta = (FormerlySerializedAs = "InitialPlayerMapLayer"))
	EPlayerMapRegion InitialPlayerMapRegion = EPlayerMapRegion::Dome;
};

/** Central catalog for playable, travel, and training levels. */
UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API ULevelDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	static FSoftObjectPath GetDefaultDefinitionPath();
	static const ULevelDefinition* ResolveDefaultDefinition();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UFUNCTION(BlueprintPure, Category = "!Ingame Level")
	bool GetIngameLevelAtIndex(int32 Index, FLobbyMatchMapOption& OutLevel) const;

	UFUNCTION(BlueprintPure, Category = "!Ingame Level")
	bool FindIngameLevel(FName LevelKey, FLobbyMatchMapOption& OutLevel) const;

	UFUNCTION(BlueprintPure, Category = "!Ingame Level")
	FName ResolveIngameLevelKey(FName LevelKey) const;

	FString GetTitleTravelMapName() const;
	FString GetLobbyTravelMapName() const;
	FString GetRoomTravelMapName() const;
	FString GetTrainingRoomTravelMapName() const;
	bool IsLobbyMapName(const FString& LevelName) const;
	bool IsTrainingRoomMapName(const FString& LevelName) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ingame Level",
		meta = (TitleProperty = "DisplayName"))
	TArray<FLobbyMatchMapOption> IngameLevels;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Travel Level")
	TSoftObjectPtr<UWorld> TitleLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Travel Level")
	TSoftObjectPtr<UWorld> LobbyLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Travel Level")
	TSoftObjectPtr<UWorld> RoomLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Training Level")
	TSoftObjectPtr<UWorld> TrainingLevel;
};
