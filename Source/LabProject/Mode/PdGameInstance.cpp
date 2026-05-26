#include "Mode/PdGameInstance.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
#include "Pandora/PandoraDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdGameInstance)

DEFINE_LOG_CATEGORY_STATIC(LogPdGameInstance, Log, All);

UPdGameInstance::UPdGameInstance()
{
	SaveGameClass = UPdSaveGame::StaticClass();
}

void UPdGameInstance::Init()
{
	Super::Init();
	LoadSkillDataAssetsToMemory();
	LoadPandoraDataAssetsToMemory();
}

void UPdGameInstance::LoadGame(const FString& PlayerId)
{
	GetOrCreateSaveGame(PlayerId);
}

void UPdGameInstance::SaveGame(const FString& PlayerId)
{
	if (PlayerId.IsEmpty())
	{
		UE_LOG(LogPdGameInstance, Warning, TEXT("SaveGame skipped: PlayerId is empty."));
		return;
	}

	UPdSaveGame* SaveGameObject = SavedGameByPlayerId.FindRef(PlayerId);
	if (!IsValid(SaveGameObject))
	{
		UE_LOG(LogPdGameInstance, Warning, TEXT("SaveGame skipped: no save object for PlayerId=%s."), *PlayerId);
		return;
	}

	const bool bSaved = UGameplayStatics::SaveGameToSlot(SaveGameObject, PlayerId, 0);
	UE_LOG(LogPdGameInstance, Log, TEXT("SaveGame PlayerId=%s success=%s"), *PlayerId, bSaved ? TEXT("true") : TEXT("false"));
}

UPdSaveGame* UPdGameInstance::GetOrCreateSaveGame(const FString& PlayerId)
{
	if (PlayerId.IsEmpty())
	{
		UE_LOG(LogPdGameInstance, Warning, TEXT("GetOrCreateSaveGame failed: PlayerId is empty."));
		return nullptr;
	}

	if (UPdSaveGame* ExistingSaveGame = SavedGameByPlayerId.FindRef(PlayerId))
	{
		return ExistingSaveGame;
	}

	UPdSaveGame* SaveGameObject = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(PlayerId, 0))
	{
		SaveGameObject = Cast<UPdSaveGame>(UGameplayStatics::LoadGameFromSlot(PlayerId, 0));
		if (!SaveGameObject)
		{
			UE_LOG(LogPdGameInstance, Warning, TEXT("Replacing incompatible save slot for PlayerId=%s."), *PlayerId);
			SaveGameObject = CreateConfiguredSaveGameObject();
		}
	}
	else
	{
		SaveGameObject = CreateConfiguredSaveGameObject();
	}

	if (IsValid(SaveGameObject))
	{
		SavedGameByPlayerId.Add(PlayerId, SaveGameObject);
	}

	return SaveGameObject;
}

void UPdGameInstance::LoadSkillDataAssetsToMemory()
{
	SkillDataAssetsByName.Reset();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FAssetData> SkillAssetData;
	AssetRegistryModule.Get().GetAssetsByClass(USkillDataAsset::StaticClass()->GetClassPathName(), SkillAssetData, true);

	for (const FAssetData& AssetData : SkillAssetData)
	{
		USkillDataAsset* Skill = Cast<USkillDataAsset>(AssetData.GetAsset());
		if (!Skill)
		{
			continue;
		}

		const FName SkillName = Skill->Name.IsNone() ? AssetData.AssetName : Skill->Name;
		SkillDataAssetsByName.Add(SkillName, Skill);
	}

	UE_LOG(LogPdGameInstance, Log, TEXT("Cached skill data assets: %d"), SkillDataAssetsByName.Num());
}

void UPdGameInstance::LoadPandoraDataAssetsToMemory()
{
	PandoraDefinitionsByName.Reset();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FAssetData> PandoraAssetData;
	AssetRegistryModule.Get().GetAssetsByClass(UPandoraDefinition::StaticClass()->GetClassPathName(), PandoraAssetData, true);

	for (const FAssetData& AssetData : PandoraAssetData)
	{
		UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(AssetData.GetAsset());
		if (!PandoraDefinition)
		{
			continue;
		}

		PandoraDefinitionsByName.Add(AssetData.AssetName, PandoraDefinition);
	}

	UE_LOG(LogPdGameInstance, Log, TEXT("Cached pandora definitions: %d"), PandoraDefinitionsByName.Num());
}

USkillDataAsset* UPdGameInstance::GetSkillDataAssetByName(FName SkillName) const
{
	return SkillDataAssetsByName.FindRef(SkillName);
}

UPandoraDefinition* UPdGameInstance::GetPandoraDefinitionByName(FName PandoraName) const
{
	return PandoraDefinitionsByName.FindRef(PandoraName);
}

void UPdGameInstance::BuildGrantedPandorasFromNames(
	const TMap<FName, int32>& GrantedPandorasByName,
	TArray<FGrantedPandora>& OutGrantedPandoras) const
{
	OutGrantedPandoras.Reset();

	for (const TPair<FName, int32>& PandoraPair : GrantedPandorasByName)
	{
		UPandoraDefinition* PandoraDefinition = GetPandoraDefinitionByName(PandoraPair.Key);
		if (!PandoraDefinition)
		{
			UE_LOG(LogPdGameInstance, Warning, TEXT("Could not resolve saved pandora: name=%s level=%d"),
				*PandoraPair.Key.ToString(),
				PandoraPair.Value);
			continue;
		}

		OutGrantedPandoras.Add(FGrantedPandora(PandoraDefinition, PandoraPair.Value));
	}
}

UPdSaveGame* UPdGameInstance::CreateConfiguredSaveGameObject() const
{
	UClass* ClassToCreate = SaveGameClass.Get();
	if (!ClassToCreate)
	{
		ClassToCreate = UPdSaveGame::StaticClass();
	}

	return Cast<UPdSaveGame>(UGameplayStatics::CreateSaveGameObject(ClassToCreate));
}
