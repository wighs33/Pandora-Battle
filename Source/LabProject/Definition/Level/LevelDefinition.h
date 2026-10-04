#pragma once

#include "CoreMinimal.h"
#include "Common/GameSessionConstants.h"
#include "Engine/DataAsset.h"
#include "Common/PlayerMapRegion.h"

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

/** 플레이·이동·훈련에 사용하는 레벨의 공통 목록을 제공한다. */
UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API ULevelDefinition : public UPrimaryDataAsset
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
	static const ULevelDefinition* ResolveDefaultDefinition();

	UFUNCTION(BlueprintPure, Category = "!Ingame Level")
	bool GetIngameLevelAtIndex(int32 Index, FLobbyMatchMapOption& OutLevel) const;

	UFUNCTION(BlueprintPure, Category = "!Ingame Level")
	bool FindIngameLevel(FName LevelKey, FLobbyMatchMapOption& OutLevel) const;

	FString GetTitleTravelMapName() const;
	FString GetLobbyTravelMapName() const;
	FString GetRoomTravelMapName() const;
	FString GetTrainingRoomTravelMapName() const;
	FString GetRpgTravelMapName() const;
	bool IsLobbyMapName(const FString& LevelName) const;
	bool IsTrainingRoomMapName(const FString& LevelName) const;

private:
	FName ResolveIngameLevelKey(FName LevelKey) const;

public:
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

	/**
	 * 타이틀의 보스 레이드 맵. 전용 서버가 rpg 모드 게임 세션을 받으면 이 맵으로 이동해 경기 끝 없이 열어 둔다.
	 * 서버 쿡 목록(MapsToCook)에 있어야 한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Rpg Level")
	TSoftObjectPtr<UWorld> RpgLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Map/LV_Colosseum.LV_Colosseum")));
};
