#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/PandoraTree/PandoraTreeComponent.h"
#include "AbilitySystem/Skills/SkillTypes.h"
#include "Engine/GameInstance.h"
#include "SavedGameData/PdSaveGame.h"
#include "PdGameInstance.generated.h"

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UPdGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UPdGameInstance();

	virtual void Init() override;

	UFUNCTION(BlueprintCallable, Category = "!Save")
	void LoadGame(const FString& PlayerId);

	UFUNCTION(BlueprintCallable, Category = "!Save")
	void SaveGame(const FString& PlayerId);

	UFUNCTION(BlueprintCallable, Category = "!Save")
	UPdSaveGame* GetOrCreateSaveGame(const FString& PlayerId);

	UFUNCTION(BlueprintCallable, Category = "!Skill")
	void LoadSkillDataAssetsToMemory();

	UFUNCTION(BlueprintCallable, Category = "!Pandora")
	void LoadPandoraDataAssetsToMemory();

	UFUNCTION(BlueprintPure, Category = "!Skill")
	USkillDataAsset* GetSkillDataAssetByName(FName SkillName) const;

	UFUNCTION(BlueprintPure, Category = "!Pandora")
	UPandoraDefinition* GetPandoraDefinitionByName(FName PandoraName) const;

	UFUNCTION(BlueprintCallable, Category = "!Pandora", meta = (AutoCreateRefTerm = "GrantedPandorasByName"))
	void BuildGrantedPandorasFromNames(const TMap<FName, int32>& GrantedPandorasByName, TArray<FGrantedPandora>& OutGrantedPandoras) const;

	UPROPERTY(BlueprintReadWrite, Category = "!Save")
	TMap<FString, TObjectPtr<UPdSaveGame>> SavedGameByPlayerId;

	UPROPERTY(BlueprintReadWrite, Category = "!Skill")
	TMap<FName, TObjectPtr<USkillDataAsset>> SkillDataAssetsByName;

	UPROPERTY(BlueprintReadWrite, Category = "!Pandora")
	TMap<FName, TObjectPtr<UPandoraDefinition>> PandoraDefinitionsByName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Save")
	TSubclassOf<UPdSaveGame> SaveGameClass;

private:
	UPdSaveGame* CreateConfiguredSaveGameObject() const;
};
