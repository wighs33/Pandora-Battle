#include "Definition/UI/WidgetDefinitionFragments.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WidgetDefinitionFragments)

FPrimaryAssetId UWidgetStyleDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("WidgetStyleDefinition"), GetFName());
}

FPrimaryAssetId UWidgetInputIconsDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("WidgetInputIconsDefinition"), GetFName());
}

FPrimaryAssetId UWidgetMapUIDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("WidgetMapUIDefinition"), GetFName());
}
