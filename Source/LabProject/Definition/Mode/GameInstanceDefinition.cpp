#include "Definition/Mode/GameInstanceDefinition.h"

#include "Definition/Mode/ProjectBootstrapSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameInstanceDefinition)

FPrimaryAssetId UGameInstanceDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("GameInstanceDefinition"), GetFName());
}

FSoftObjectPath UGameInstanceDefinition::GetDefaultDefinitionPath()
{
	return GetDefault<UProjectBootstrapSettings>()->GetBootstrapDefinition().ToSoftObjectPath();
}

const FProjectDefinitionReferences& UGameInstanceDefinition::GetConfiguredDefinitionReferences()
{
	const UGameInstanceDefinition* Definition = GetDefault<UProjectBootstrapSettings>()
		->GetBootstrapDefinition().LoadSynchronous();
	return Definition ? Definition->Definitions : GetDefault<UGameInstanceDefinition>()->Definitions;
}
