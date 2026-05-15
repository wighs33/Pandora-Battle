#include "Experience/ExperienceDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExperienceDefinition)

UExperienceDefinition::UExperienceDefinition()
{
}

FPrimaryAssetId UExperienceDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("ExperienceDefinition"), GetFName());
}
