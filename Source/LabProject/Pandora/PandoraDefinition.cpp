#include "PandoraDefinition.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraDefinition)

DEFINE_LOG_CATEGORY(PandoraDefinitionLog);

FPrimaryAssetId UPandoraDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("PandoraDefinition"), GetFName());
}
