#include "Definition/UI/RecordDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RecordDefinition)

namespace
{
	constexpr int32 DefaultTierCount = 9;
}

URecordDefinition::URecordDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WinsPerTier = 20;

	if (TierEntries.IsEmpty())
	{
		TierEntries.Reserve(DefaultTierCount);
		for (int32 TierIndex = 0; TierIndex < DefaultTierCount; ++TierIndex)
		{
			FRecordTierEntry TierEntry;
			TierEntry.TierName = FText::Format(
				NSLOCTEXT("RecordDefinition", "DefaultTierNameFormat", "Tier {0}"),
				FText::AsNumber(TierIndex + 1));
			TierEntry.RankOrder = TierIndex;
			TierEntries.Add(TierEntry);
		}
	}
}

FPrimaryAssetId URecordDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Record"), GetFName());
}

FRecordTierEntry URecordDefinition::ResolveTierForWinCount(const int32 WinCount) const
{
	if (TierEntries.IsEmpty())
	{
		return FRecordTierEntry();
	}

	TArray<int32> SortedTierIndices;
	SortedTierIndices.Reserve(TierEntries.Num());
	for (int32 TierEntryIndex = 0; TierEntryIndex < TierEntries.Num(); ++TierEntryIndex)
	{
		SortedTierIndices.Add(TierEntryIndex);
	}

	SortedTierIndices.Sort([this](const int32 LeftIndex, const int32 RightIndex)
	{
		return TierEntries[LeftIndex].RankOrder < TierEntries[RightIndex].RankOrder;
	});

	const int32 SanitizedWinsPerTier = FMath::Max(WinsPerTier, 1);
	const int32 TierIndex = FMath::Clamp(
		FMath::Max(WinCount, 0) / SanitizedWinsPerTier,
		0,
		SortedTierIndices.Num() - 1);
	return TierEntries[SortedTierIndices[TierIndex]];
}
