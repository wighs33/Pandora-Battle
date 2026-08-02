#include "Mode/PdGameInstance.h"

#include "Data/ContentDataSubsystem.h"

void UPdGameInstance::LoadSkillDataAssetsToMemory()
{
	if (UContentDataSubsystem* ContentDataSubsystem =
		GetSubsystem<UContentDataSubsystem>())
	{
		ContentDataSubsystem->LoadSkillDataAssetsAsync();
	}
}

void UPdGameInstance::LoadPandoraDataAssetsToMemory()
{
	if (UContentDataSubsystem* ContentDataSubsystem =
		GetSubsystem<UContentDataSubsystem>())
	{
		ContentDataSubsystem->LoadPandoraDataAssetsAsync();
	}
}

void UPdGameInstance::LoadSkinDataAssetsToMemory()
{
	if (UContentDataSubsystem* ContentDataSubsystem =
		GetSubsystem<UContentDataSubsystem>())
	{
		ContentDataSubsystem->LoadSkinDataAssetsAsync();
	}
}

USkillDefinition* UPdGameInstance::GetSkillDataAssetByName(
	const FName SkillName) const
{
	if (const UContentDataSubsystem* ContentDataSubsystem =
		GetSubsystem<UContentDataSubsystem>())
	{
		return ContentDataSubsystem->GetSkillDataAssetByName(SkillName);
	}

	return nullptr;
}

UPandoraDefinition* UPdGameInstance::GetPandoraDefinitionByName(
	const FName PandoraName) const
{
	if (const UContentDataSubsystem* ContentDataSubsystem =
		GetSubsystem<UContentDataSubsystem>())
	{
		return ContentDataSubsystem->GetPandoraDefinitionByName(PandoraName);
	}

	return nullptr;
}

USkinDefinition* UPdGameInstance::GetSkinDefinitionByName(
	const FName SkinName) const
{
	if (const UContentDataSubsystem* ContentDataSubsystem =
		GetSubsystem<UContentDataSubsystem>())
	{
		return ContentDataSubsystem->GetSkinDefinitionByName(SkinName);
	}

	return nullptr;
}

void UPdGameInstance::BuildGrantedPandorasFromNames(
	const TMap<FName, int32>& GrantedPandorasByName,
	TArray<FGrantedPandora>& OutGrantedPandoras) const
{
	if (const UContentDataSubsystem* ContentDataSubsystem =
		GetSubsystem<UContentDataSubsystem>())
	{
		ContentDataSubsystem->BuildGrantedPandorasFromNames(
			GrantedPandorasByName,
			OutGrantedPandoras);
		return;
	}

	OutGrantedPandoras.Reset();
}

void UPdGameInstance::BuildDefaultUnlockedPandoras(
	TArray<FName>& OutOwnedPandoraNames,
	TArray<FPrimaryAssetId>* OutPandoraDefinitionIds)
{
	if (const UContentDataSubsystem* ContentDataSubsystem =
		GetSubsystem<UContentDataSubsystem>())
	{
		ContentDataSubsystem->BuildDefaultUnlockedPandoras(
			OutOwnedPandoraNames,
			OutPandoraDefinitionIds);
		return;
	}

	OutOwnedPandoraNames.Reset();
	if (OutPandoraDefinitionIds)
	{
		OutPandoraDefinitionIds->Reset();
	}
}

void UPdGameInstance::BuildGrantedSkinDefinitionsFromNames(
	const TMap<FName, int32>& GrantedSkinsByName,
	TArray<USkinDefinition*>& OutSkinDefinitions) const
{
	if (const UContentDataSubsystem* ContentDataSubsystem =
		GetSubsystem<UContentDataSubsystem>())
	{
		ContentDataSubsystem->BuildGrantedSkinDefinitionsFromNames(
			GrantedSkinsByName,
			OutSkinDefinitions);
		return;
	}

	OutSkinDefinitions.Reset();
}
