#pragma once

#include "CoreMinimal.h"
#include "Definition/Provision/DefaultProvisionDefinition.h"
#include "UObject/PrimaryAssetId.h"
#include "MatchRuntimeSettings.generated.h"

class ULevelDefinition;
class UMatchRuleDefinition;
class URewardDefinition;
class UDefaultProvisionDefinition;
class UWorld;

/** 경기의 PlayerStart 선택 규칙. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FMatchSpawnSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Spawn")
	bool bUseLobbySpawnIndexPlayerStarts = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Spawn")
	FName LobbySpawnPlayerStartTagPrefix = TEXT("Spawn_");
};

/** 경기 타이머·결과·보상·퇴장 설정. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FMatchFlowSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Gold")
	TSoftObjectPtr<URewardDefinition> GameVictoryRewardDefinition;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Reward|Gold",
		meta = (ClampMin = "0"))
	int32 VictoryGoldPerKill = 100;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Reward|Gold",
		meta = (ClampMin = "0"))
	int32 VictoryGoldPenaltyPerDeath = 50;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Reward|Gold",
		meta = (ClampMin = "0"))
	int32 VictoryGoldPerWinningTeamMember = 3;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Reward|Chest Spawn",
		meta = (
			ToolTip = "Controls how many placed reward chests stay active; if unset, the first placed chest definition is used."))
	TSoftObjectPtr<URewardDefinition> ChestSpawnRewardDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules")
	TSoftObjectPtr<UMatchRuleDefinition> MatchRuleDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Level")
	TSoftObjectPtr<ULevelDefinition> LevelDefinition;
};

/** 공통 기본 지급과 함께 사용하는 경기 플레이어 준비 설정. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FMatchPlayerSetupSettings
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UDefaultProvisionDefinition> DefaultProvisionDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Level")
	TSoftObjectPtr<ULevelDefinition> LevelDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Team")
	bool bAssignDefaultTeamWhenLobbyTeamMissing = true;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Team",
		meta = (ClampMin = "0"))
	int32 DefaultLobbyTeamColorIndex = 0;
};
