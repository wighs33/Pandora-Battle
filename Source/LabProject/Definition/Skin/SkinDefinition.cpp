#include "Definition/Skin/SkinDefinition.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinDefinition)

DEFINE_LOG_CATEGORY(SkinDefinitionLog);

FPrimaryAssetId USkinDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("SkinDefinition"), GetFName());
}
