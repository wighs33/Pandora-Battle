#include "Definition/UI/GuideDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GuideDefinition)

FText FGuidePageEntry::GetContent(const EGuideLanguage Language) const
{
	if (const FText* Translation = LocalizedContent.Find(Language); Translation && !Translation->IsEmptyOrWhitespace())
	{
		return *Translation;
	}
	if (const FText* English = LocalizedContent.Find(EGuideLanguage::English); English && !English->IsEmptyOrWhitespace())
	{
		return *English;
	}
	return Content;
}

FPrimaryAssetId UGuideDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Guide"), GetFName());
}
