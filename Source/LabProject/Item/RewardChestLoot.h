#pragma once

#include "CoreMinimal.h"
#include "RewardChestLoot.generated.h"

USTRUCT(BlueprintType)
struct FRewardChestItemCountChance
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	FRewardChestItemCountChance() = default;

	FRewardChestItemCountChance(const int32 InItemCount, const float InChance)
		: ItemCount(InItemCount)
		, Chance(InChance)
	{
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Reward Chest|Reward", meta = (ClampMin = "1", UIMin = "1"))
	int32 ItemCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Reward Chest|Reward", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Chance = 1.0f;
};

// 상자가 고를 수 있는 아이템 하나. 무기인지는 상자가 아이템 정의를 보고 정한다.
struct FRewardChestLootCandidate
{
	FPrimaryAssetId ItemId;
	float Weight = 0.0f;
	bool bWeapon = false;
};

// 무작위 보상 개수 규칙. 확률 표를 쓰지 않거나 표가 비어 있으면 FallbackCount개다.
struct FRewardChestItemCountRule
{
	bool bUseChances = true;
	int32 FallbackCount = 1;
	TConstArrayView<FRewardChestItemCountChance> Chances;
};

// 보상 상자의 뽑기 규칙. 무기는 언제나 하나만 고르고, 나머지는 무기가 아닌 아이템으로 채운다.
namespace PdRewardChestLoot
{
	// 가중치 비율로 하나를 고른다. 후보가 없거나 가중치 합이 0 이하면 INDEX_NONE이다.
	int32 SelectWeightedIndex(TConstArrayView<float> Weights, float TotalWeight);

	// 보상 개수를 정한다. 무기 몫 하나도 이 개수에 들어간다.
	int32 RollItemCount(const FRewardChestItemCountRule& Rule);

	// 무기 하나를 고른 뒤, 남은 개수만큼 무기가 아닌 아이템을 가중치로 고른다.
	void RollRandomItems(
		TConstArrayView<FRewardChestLootCandidate> Candidates,
		const FRewardChestItemCountRule& CountRule,
		bool bAllowDuplicates,
		TArray<FPrimaryAssetId>& OutItemIds);

	// 설정된 무기 중 하나만 가중치로 고르고, 무기가 아닌 아이템은 설정된 대로 모두 넣는다.
	void PickConfiguredItems(TConstArrayView<FRewardChestLootCandidate> Candidates, TArray<FPrimaryAssetId>& OutItemIds);
}
