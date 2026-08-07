#include "Definition/Mode/PdGameInstanceDefinition.h"

#include "Settings/ProjectBootstrapSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdGameInstanceDefinition)

FPrimaryAssetId UPdGameInstanceDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("GameInstanceDefinition"), GetFName());
}

FSoftObjectPath UPdGameInstanceDefinition::GetDefaultDefinitionPath()
{
	return GetDefault<UProjectBootstrapSettings>()
		->GetBootstrapDefinition().ToSoftObjectPath();
}

const FProjectDefinitionReferences&
UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
{
	const UPdGameInstanceDefinition* Definition =
		GetDefault<UProjectBootstrapSettings>()
			->GetBootstrapDefinition().LoadSynchronous();
	return Definition
		? Definition->Definitions
		: GetDefault<UPdGameInstanceDefinition>()->Definitions;
}
