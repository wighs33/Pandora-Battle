#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RewardDefinition.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct LABPROJECT_API FRewardExperienceRange
{
	GENERATED_BODY()

public:
	int32 RollReward(const UObject* LogContext, const TCHAR* CategoryName) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Experience")
	bool bGrantRandomExperience = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Experience",
		meta = (ClampMin = "0", UIMin = "0", EditCondition = "bGrantRandomExperience"))
	int32 MinExperienceReward = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Experience",
		meta = (ClampMin = "0", UIMin = "0", EditCondition = "bGrantRandomExperience"))
	int32 MaxExperienceReward = 0;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FRewardSoulDustRange
{
	GENERATED_BODY()

public:
	int32 RollReward(const UObject* LogContext, const TCHAR* CategoryName) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Soul Dust")
	bool bGrantRandomSoulDust = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Soul Dust",
		meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0", EditCondition = "bGrantRandomSoulDust"))
	float SoulDustDropChance = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Soul Dust",
		meta = (ClampMin = "0", UIMin = "0", EditCondition = "bGrantRandomSoulDust"))
	int32 MinSoulDustReward = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Soul Dust",
		meta = (ClampMin = "0", UIMin = "0", EditCondition = "bGrantRandomSoulDust"))
	int32 MaxSoulDustReward = 0;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FRewardGoldRange
{
	GENERATED_BODY()

public:
	int32 RollReward(const UObject* LogContext, const TCHAR* CategoryName) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Gold")
	bool bGrantRandomGold = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Gold",
		meta = (ClampMin = "0", UIMin = "0", EditCondition = "bGrantRandomGold"))
	int32 MinGoldReward = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Gold",
		meta = (ClampMin = "0", UIMin = "0", EditCondition = "bGrantRandomGold"))
	int32 MaxGoldReward = 0;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FRewardChestSpawnCategory
{
	GENERATED_BODY()

public:
	int32 ResolveActiveChestCount(int32 TotalChestCount) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Chest Spawn",
		meta = (ClampMin = "0", UIMin = "0",
			ToolTip = "0 keeps every placed reward chest active. Values greater than 0 keep only this many random placed chests."))
	int32 ActiveChestCount = 0;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FMonsterDefeatRewardCategory
{
	GENERATED_BODY()

public:
	FMonsterDefeatRewardCategory();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Monster Defeat")
	FRewardExperienceRange Experience;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Monster Defeat")
	FRewardSoulDustRange SoulDust;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPlayerKillRewardCategory
{
	GENERATED_BODY()

public:
	FPlayerKillRewardCategory();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Player Kill")
	FRewardExperienceRange Experience;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FGameVictoryRewardCategory
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Game Victory")
	FRewardGoldRange Gold;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FRewardNotificationCategory
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Notification")
	TObjectPtr<UTexture2D> ExperienceIcon = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Notification")
	TObjectPtr<UTexture2D> SoulDustIcon = nullptr;
};

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API URewardDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	URewardDefinition();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	static FSoftObjectPath GetDefaultRewardDefinitionPath();

	UFUNCTION(BlueprintCallable, Category = "!Reward|Monster Defeat")
	int32 RollMonsterDefeatExperienceReward() const;

	UFUNCTION(BlueprintCallable, Category = "!Reward|Monster Defeat")
	int32 RollMonsterDefeatSoulDustReward() const;

	UFUNCTION(BlueprintCallable, Category = "!Reward|Player Kill")
	int32 RollPlayerKillExperienceReward() const;

	UFUNCTION(BlueprintCallable, Category = "!Reward|Game Victory")
	int32 RollGameVictoryGoldReward() const;

	UFUNCTION(BlueprintCallable, Category = "!Reward|Chest Spawn")
	int32 ResolveActiveRewardChestCount(int32 TotalChestCount) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Chest Spawn")
	FRewardChestSpawnCategory ChestSpawn;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Monster Defeat")
	FMonsterDefeatRewardCategory MonsterDefeatReward;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Player Kill")
	FPlayerKillRewardCategory PlayerKillReward;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Game Victory")
	FGameVictoryRewardCategory GameVictoryReward;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Notification")
	FRewardNotificationCategory Notification;
};
