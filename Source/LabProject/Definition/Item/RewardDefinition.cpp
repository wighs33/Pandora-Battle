#include "Definition/Item/RewardDefinition.h"

#include "Definition/Item/ItemDefinition.h"
#include "Definition/Mode/PdGameInstanceDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RewardDefinition)

int32 FRewardExperienceRange::RollReward(const UObject* LogContext, const TCHAR* CategoryName) const
{
	if (!bGrantRandomExperience)
	{
		return 0;
	}

	const int32 MinReward = FMath::Max(0, FMath::Min(MinExperienceReward, MaxExperienceReward));
	const int32 MaxReward = FMath::Max(0, FMath::Max(MinExperienceReward, MaxExperienceReward));
	const int32 Reward = MaxReward > MinReward ? FMath::RandRange(MinReward, MaxReward) : MinReward;

	return Reward;
}

int32 FRewardSoulDustRange::RollReward(const UObject* LogContext, const TCHAR* CategoryName) const
{
	if (!bGrantRandomSoulDust)
	{
		return 0;
	}

	const float ClampedChance = FMath::Clamp(SoulDustDropChance, 0.0f, 100.0f);
	if (ClampedChance <= 0.0f || (ClampedChance < 100.0f && FMath::FRandRange(0.0f, 100.0f) >= ClampedChance))
	{

		return 0;
	}

	const int32 MinReward = FMath::Max(0, FMath::Min(MinSoulDustReward, MaxSoulDustReward));
	const int32 MaxReward = FMath::Max(0, FMath::Max(MinSoulDustReward, MaxSoulDustReward));
	const int32 Reward = MaxReward > MinReward ? FMath::RandRange(MinReward, MaxReward) : MinReward;

	return Reward;
}

int32 FRewardGoldRange::RollReward(const UObject* LogContext, const TCHAR* CategoryName) const
{
	if (!bGrantRandomGold)
	{
		return 0;
	}

	const int32 MinReward = FMath::Max(0, FMath::Min(MinGoldReward, MaxGoldReward));
	const int32 MaxReward = FMath::Max(0, FMath::Max(MinGoldReward, MaxGoldReward));
	const int32 Reward = MaxReward > MinReward ? FMath::RandRange(MinReward, MaxReward) : MinReward;

	return Reward;
}

FPrimaryAssetId FRewardRandomPotionDrop::RollReward() const
{
	if (!bGrantRandomPotion)
	{
		return FPrimaryAssetId();
	}

	const FPrimaryAssetType ItemDefinitionType(TEXT("ItemDefinition"));
	TArray<FPrimaryAssetId, TInlineAllocator<3>> ValidPotionDefinitionIds;
	for (const TSoftObjectPtr<UItemDefinition>& PotionDefinition : PotionDefinitions)
	{
		const FSoftObjectPath PotionDefinitionPath = PotionDefinition.ToSoftObjectPath();
		if (!PotionDefinitionPath.IsValid())
		{
			continue;
		}

		const FPrimaryAssetId PotionDefinitionId(
			ItemDefinitionType,
			PotionDefinitionPath.GetAssetFName());
		ValidPotionDefinitionIds.AddUnique(PotionDefinitionId);
	}

	if (ValidPotionDefinitionIds.IsEmpty())
	{
		return FPrimaryAssetId();
	}

	const float ClampedChance = FMath::Clamp(PotionDropChance, 0.0f, 100.0f);
	if (ClampedChance <= 0.0f
		|| (ClampedChance < 100.0f && FMath::FRandRange(0.0f, 100.0f) >= ClampedChance))
	{
		return FPrimaryAssetId();
	}

	return ValidPotionDefinitionIds[FMath::RandHelper(ValidPotionDefinitionIds.Num())];
}

FPlayerKillRewardCategory::FPlayerKillRewardCategory()
{
	Experience.bGrantRandomExperience = true;
	Experience.MinExperienceReward = 25;
	Experience.MaxExperienceReward = 25;
}

FMonsterDefeatRewardCategory::FMonsterDefeatRewardCategory()
{
	Experience.bGrantRandomExperience = true;
	Experience.MinExperienceReward = 10;
	Experience.MaxExperienceReward = 10;

	SoulDust.bGrantRandomSoulDust = true;
	SoulDust.SoulDustDropChance = 100.0f;
	SoulDust.MinSoulDustReward = 1;
	SoulDust.MaxSoulDustReward = 1;

	PotionDrop.bGrantRandomPotion = true;
	PotionDrop.PotionDropChance = 20.0f;
	PotionDrop.PotionDefinitions =
	{
		TSoftObjectPtr<UItemDefinition>(FSoftObjectPath(
			TEXT("/Game/Item/Consumable/DA_HealPotion.DA_HealPotion"))),
		TSoftObjectPtr<UItemDefinition>(FSoftObjectPath(
			TEXT("/Game/Item/Consumable/DA_ManaPotion.DA_ManaPotion"))),
		TSoftObjectPtr<UItemDefinition>(FSoftObjectPath(
			TEXT("/Game/Item/Consumable/DA_StaminaPotion.DA_StaminaPotion")))
	};
}

int32 FRewardChestSpawnCategory::ResolveActiveChestCount(const int32 TotalChestCount) const
{
	if (TotalChestCount <= 0)
	{
		return 0;
	}

	if (ActiveChestCount <= 0)
	{
		return TotalChestCount;
	}

	return FMath::Clamp(ActiveChestCount, 0, TotalChestCount);
}

URewardDefinition::URewardDefinition()
{
}

FPrimaryAssetId URewardDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("RewardDefinition"), GetFName());
}

FSoftObjectPath URewardDefinition::GetDefaultRewardDefinitionPath()
{
	return UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
		.Reward.ToSoftObjectPath();
}

int32 URewardDefinition::RollMonsterDefeatExperienceReward() const
{
	return MonsterDefeatReward.Experience.RollReward(this, TEXT("MonsterDefeat"));
}

int32 URewardDefinition::RollMonsterDefeatSoulDustReward() const
{
	return MonsterDefeatReward.SoulDust.RollReward(this, TEXT("MonsterDefeat"));
}

FPrimaryAssetId URewardDefinition::RollMonsterDefeatPotionReward() const
{
	return MonsterDefeatReward.PotionDrop.RollReward();
}

int32 URewardDefinition::RollPlayerKillExperienceReward() const
{
	return PlayerKillReward.Experience.RollReward(this, TEXT("PlayerKill"));
}

int32 URewardDefinition::RollGameVictoryGoldReward() const
{
	return GameVictoryReward.Gold.RollReward(this, TEXT("GameVictory"));
}

int32 URewardDefinition::ResolveActiveRewardChestCount(const int32 TotalChestCount) const
{
	const int32 ActiveCount = ChestSpawn.ResolveActiveChestCount(TotalChestCount);

	return ActiveCount;
}
