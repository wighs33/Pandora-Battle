#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "ExperienceGameModeSettings.generated.h"

class UMatchRuleDefinition;
class URewardDefinition;
class UWorld;

/**
 * Serializable item grant used by training-room provisioning.
 *
 * Kept as a standalone definition so GameMode policy, provisioning components,
 * and future Experience data assets can share the same schema.
 */
USTRUCT(BlueprintType)
struct LABPROJECT_API FTrainingRoomItemStackGrant
{
	GENERATED_BODY()

	FTrainingRoomItemStackGrant() = default;

	FTrainingRoomItemStackGrant(
		const FPrimaryAssetId& InItemDefinitionId,
		const int32 InQuantity)
		: ItemDefinitionId(InItemDefinitionId)
		, Quantity(InQuantity)
	{
	}

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Training",
		meta = (AllowedTypes = "ItemDefinition"))
	FPrimaryAssetId ItemDefinitionId;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Training",
		meta = (ClampMin = "0"))
	int32 Quantity = 100;
};

/** Serializable starter inventory entry for gameplay maps. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FGameplayItemStackGrant
{
	GENERATED_BODY()

	FGameplayItemStackGrant() = default;

	FGameplayItemStackGrant(
		const FPrimaryAssetId& InItemDefinitionId,
		const int32 InQuantity,
		const int32 InQuickSlotIndex = INDEX_NONE)
		: ItemDefinitionId(InItemDefinitionId)
		, Quantity(InQuantity)
		, QuickSlotIndex(InQuickSlotIndex)
	{
	}

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Gameplay|Inventory",
		meta = (AllowedTypes = "ItemDefinition"))
	FPrimaryAssetId ItemDefinitionId;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Gameplay|Inventory",
		meta = (ClampMin = "0"))
	int32 Quantity = 1;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Gameplay|Inventory",
		meta = (
			ClampMin = "-1",
			ClampMax = "3",
			ToolTip = "-1 leaves the item unassigned; 0-3 map to quick-slot keys 1-4."))
	int32 QuickSlotIndex = INDEX_NONE;
};

/** Serializable default gesture-to-slot assignment for gameplay maps. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FGameplayGestureSlotGrant
{
	GENERATED_BODY()

	FGameplayGestureSlotGrant() = default;

	FGameplayGestureSlotGrant(
		const FPrimaryAssetId& InSkinDefinitionId,
		const int32 InGestureSlotIndex)
		: SkinDefinitionId(InSkinDefinitionId)
		, GestureSlotIndex(InGestureSlotIndex)
	{
	}

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Gameplay|Gesture",
		meta = (AllowedTypes = "SkinDefinition"))
	FPrimaryAssetId SkinDefinitionId;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Gameplay|Gesture",
		meta = (
			ClampMin = "0",
			ClampMax = "3",
			ToolTip = "0-3 map to gesture keys 5-8."))
	int32 GestureSlotIndex = 0;
};

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Flow")
	TSoftObjectPtr<UWorld> TitleMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Flow")
	FString TitleTravelMapName = TEXT("/Game/Map/LV_Title");
};

/**
 * Player-provisioning policy distributed by
 * UExperiencePlayerProvisioningComponent to its domain provisioners.
 */
USTRUCT(BlueprintType)
struct LABPROJECT_API FExperiencePlayerProvisioningSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Training")
	bool bGrantAllItemsInTrainingRoom = true;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Training",
		meta = (TitleProperty = "ItemDefinitionId"))
	TArray<FTrainingRoomItemStackGrant> TrainingRoomItemStackGrants =
	{
		FTrainingRoomItemStackGrant(
			FPrimaryAssetId(
				FPrimaryAssetType(TEXT("ItemDefinition")),
				TEXT("DA_HealPotion")),
			100),
		FTrainingRoomItemStackGrant(
			FPrimaryAssetId(
				FPrimaryAssetType(TEXT("ItemDefinition")),
				TEXT("DA_ManaPotion")),
			100),
		FTrainingRoomItemStackGrant(
			FPrimaryAssetId(
				FPrimaryAssetType(TEXT("ItemDefinition")),
				TEXT("DA_StaminaPotion")),
			100)
	};

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Gameplay|Inventory",
		meta = (TitleProperty = "ItemDefinitionId"))
	TArray<FGameplayItemStackGrant> DefaultGameplayItemStackGrants =
	{
		FGameplayItemStackGrant(
			FPrimaryAssetId(
				FPrimaryAssetType(TEXT("ItemDefinition")),
				TEXT("DA_HealPotion")),
			3,
			0),
		FGameplayItemStackGrant(
			FPrimaryAssetId(
				FPrimaryAssetType(TEXT("ItemDefinition")),
				TEXT("DA_ManaPotion")),
			3,
			1),
		FGameplayItemStackGrant(
			FPrimaryAssetId(
				FPrimaryAssetType(TEXT("ItemDefinition")),
				TEXT("DA_StaminaPotion")),
			3,
			2)
	};

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Gameplay|Inventory",
		meta = (ClampMin = "1"))
	int32 DefaultGameplayItemGrantMaxAttempts = 50;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Gameplay|Inventory",
		meta = (ClampMin = "0.01", ForceUnits = "s"))
	float DefaultGameplayItemGrantRetryDelay = 0.1f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Gameplay|Gesture",
		meta = (TitleProperty = "SkinDefinitionId"))
	TArray<FGameplayGestureSlotGrant> DefaultGameplayGestureSlotGrants =
	{
		FGameplayGestureSlotGrant(
			FPrimaryAssetId(
				FPrimaryAssetType(TEXT("SkinDefinition")),
				TEXT("DA_HandRaising")),
			0)
	};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Training")
	bool bInitializeStatusPointsInTrainingRoom = true;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Training",
		meta = (ClampMin = "0.0"))
	float TrainingRoomStatusPointValue = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Training")
	bool bInitializeSoulDustInTrainingRoom = true;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Training",
		meta = (ClampMin = "0"))
	int32 TrainingRoomSoulDustValue = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Training")
	TArray<FName> TrainingRoomMapNames = { TEXT("LV_TrainingRoom") };

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Team")
	bool bAssignDefaultTeamWhenLobbyTeamMissing = true;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!Team",
		meta = (ClampMin = "0"))
	int32 DefaultLobbyTeamColorIndex = 0;
};
