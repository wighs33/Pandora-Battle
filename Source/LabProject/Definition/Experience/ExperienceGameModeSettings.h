#pragma once

#include "CoreMinimal.h"
#include "Definition/Provision/DefaultProvisionDefinition.h"
#include "UObject/PrimaryAssetId.h"
#include "ExperienceGameModeSettings.generated.h"

class UMatchRuleDefinition;
class URewardDefinition;
class UDefaultProvisionDefinition;
class UWorld;

/** Player-start policy consumed by UExperienceSpawnComponent. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FExperienceSpawnSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Spawn")
	bool bUseLobbySpawnIndexPlayerStarts = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Spawn")
	FName LobbySpawnPlayerStartTagPrefix = TEXT("Spawn_");
};

/**
 * Match timer, result, reward, and exit policy consumed by
 * UExperienceMatchFlowComponent.
 */
USTRUCT(BlueprintType)
struct LABPROJECT_API FExperienceMatchFlowSettings
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
};

/**
 * Experience-only policy used alongside the shared DA_DefaultProvision.
 */
USTRUCT(BlueprintType)
struct LABPROJECT_API FExperiencePlayerProvisioningSettings
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UDefaultProvisionDefinition> DefaultProvisionDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules")
	TSoftObjectPtr<UMatchRuleDefinition> MatchRuleDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Team")
	bool bAssignDefaultTeamWhenLobbyTeamMissing = true;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Team",
		meta = (ClampMin = "0"))
	int32 DefaultLobbyTeamColorIndex = 0;
};
