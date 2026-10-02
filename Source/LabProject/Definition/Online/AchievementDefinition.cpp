#include "Definition/Online/AchievementDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AchievementDefinition)

FPrimaryAssetId UAchievementDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Achievement"), GetFName());
}

FString UAchievementDefinition::NormalizeAchievementId(FString AchievementId)
{
	AchievementId.TrimStartAndEndInline();
	return AchievementId;
}

int32 UAchievementDefinition::FindEnabledAchievementIndex(const FName AchievementId) const
{
	if (AchievementId.IsNone())
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < Achievements.Num(); ++Index)
	{
		const FAchievementEntry& Achievement = Achievements[Index];
		const FString CanonicalId = NormalizeAchievementId(Achievement.AchievementId);
		if (Achievement.bEnabled && !CanonicalId.IsEmpty() && FName(*CanonicalId) == AchievementId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

const FAchievementEntry* UAchievementDefinition::FindEnabledAchievement(const FName AchievementId) const
{
	const int32 Index = FindEnabledAchievementIndex(AchievementId);
	return Achievements.IsValidIndex(Index) ? &Achievements[Index] : nullptr;
}
