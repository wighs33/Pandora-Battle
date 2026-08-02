#include "Definition/Online/AchievementDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AchievementDefinition)

namespace
{
	FString NormalizeAchievementId(FString AchievementId)
	{
		AchievementId.TrimStartAndEndInline();
		return AchievementId;
	}
}

FPrimaryAssetId UAchievementDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Achievement"), GetFName());
}
