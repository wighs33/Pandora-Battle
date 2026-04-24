#include "ItemDefinition.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(ItemDefinition)

DEFINE_LOG_CATEGORY(ItemDefinitionLog);

FPrimaryAssetId UItemDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("ItemDefinition"), GetFName());
}
