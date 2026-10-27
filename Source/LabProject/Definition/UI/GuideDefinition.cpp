#include "Definition/UI/GuideDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GuideDefinition)

FPrimaryAssetId UGuideDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Guide"), GetFName());
}
