#include "Item/RewardChestLoot.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RewardChestLoot)

namespace
{
	struct FWeightedItemIds
	{
		TArray<FPrimaryAssetId> Ids;
		TArray<float> Weights;
		float TotalWeight = 0.0f;

		void Add(const FPrimaryAssetId& Id, const float Weight)
		{
			Ids.Add(Id);
			Weights.Add(Weight);
			TotalWeight += Weight;
		}

		void RemoveAt(const int32 Index)
		{
			TotalWeight -= Weights[Index];
			Ids.RemoveAt(Index);
			Weights.RemoveAt(Index);
		}

		int32 Pick() const
		{
			return PdRewardChestLoot::SelectWeightedIndex(Weights, TotalWeight);
		}
	};
}

int32 PdRewardChestLoot::SelectWeightedIndex(const TConstArrayView<float> Weights, const float TotalWeight)
{
	if (Weights.IsEmpty() || TotalWeight <= 0.0f)
	{
		return INDEX_NONE;
	}

	float RemainingWeight = FMath::FRandRange(0.0f, TotalWeight);
	for (int32 Index = 0; Index < Weights.Num(); ++Index)
	{
		RemainingWeight -= FMath::Max(0.0f, Weights[Index]);
		if (RemainingWeight <= 0.0f)
		{
			return Index;
		}
	}

	return Weights.Num() - 1;
}

int32 PdRewardChestLoot::RollItemCount(const FRewardChestItemCountRule& Rule)
{
	const int32 FallbackCount = FMath::Max(1, Rule.FallbackCount);
	if (!Rule.bUseChances)
	{
		return FallbackCount;
	}

	TArray<int32> CandidateCounts;
	TArray<float> CandidateWeights;
	float TotalChance = 0.0f;

	for (const FRewardChestItemCountChance& CountChance : Rule.Chances)
	{
		const float Chance = FMath::Max(0.0f, CountChance.Chance);
		if (Chance <= 0.0f)
		{
			continue;
		}

		CandidateCounts.Add(FMath::Max(1, CountChance.ItemCount));
		CandidateWeights.Add(Chance);
		TotalChance += Chance;
	}

	const int32 SelectedIndex = SelectWeightedIndex(CandidateWeights, TotalChance);
	return CandidateCounts.IsValidIndex(SelectedIndex) ? CandidateCounts[SelectedIndex] : FallbackCount;
}

void PdRewardChestLoot::RollRandomItems(
	const TConstArrayView<FRewardChestLootCandidate> Candidates,
	const FRewardChestItemCountRule& CountRule,
	const bool bAllowDuplicates,
	TArray<FPrimaryAssetId>& OutItemIds)
{
	FWeightedItemIds Weapons;
	FWeightedItemIds Others;
	for (const FRewardChestLootCandidate& Candidate : Candidates)
	{
		(Candidate.bWeapon ? Weapons : Others).Add(Candidate.ItemId, Candidate.Weight);
	}

	const int32 WeaponIndex = Weapons.Pick();
	const int32 DropCount = RollItemCount(CountRule);
	const bool bAddedWeapon = Weapons.Ids.IsValidIndex(WeaponIndex);
	if (bAddedWeapon)
	{
		OutItemIds.Add(Weapons.Ids[WeaponIndex]);
	}

	const int32 OtherDropCount = FMath::Max(0, DropCount - (bAddedWeapon ? 1 : 0));
	for (int32 DropIndex = 0;
		DropIndex < OtherDropCount
			&& !Others.Ids.IsEmpty()
			&& Others.TotalWeight > 0.0f;
		++DropIndex)
	{
		const int32 SelectedIndex = Others.Pick();
		if (!Others.Ids.IsValidIndex(SelectedIndex))
		{
			break;
		}

		OutItemIds.Add(Others.Ids[SelectedIndex]);
		if (!bAllowDuplicates)
		{
			Others.RemoveAt(SelectedIndex);
		}
	}
}

void PdRewardChestLoot::PickConfiguredItems(
	const TConstArrayView<FRewardChestLootCandidate> Candidates,
	TArray<FPrimaryAssetId>& OutItemIds)
{
	FWeightedItemIds Weapons;
	TArray<FPrimaryAssetId> Others;
	for (const FRewardChestLootCandidate& Candidate : Candidates)
	{
		if (!Candidate.bWeapon)
		{
			Others.Add(Candidate.ItemId);
		}
		else if (!Weapons.Ids.Contains(Candidate.ItemId))
		{
			Weapons.Add(Candidate.ItemId, Candidate.Weight);
		}
	}

	const int32 WeaponIndex = Weapons.Pick();
	if (Weapons.Ids.IsValidIndex(WeaponIndex))
	{
		OutItemIds.Add(Weapons.Ids[WeaponIndex]);
	}
	OutItemIds.Append(Others);
}
