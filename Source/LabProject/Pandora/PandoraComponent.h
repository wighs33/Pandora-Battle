// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "Components/PlayerStateComponent.h"
#include "PandoraComponent.generated.h"

class UPandoraInstance;

DECLARE_LOG_CATEGORY_EXTERN(PandoraComponentLog, Log, All);

USTRUCT(BlueprintType, Blueprintable)
struct FPandoraList
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	TArray<TObjectPtr<UPandoraInstance>> Pandoras;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UPandoraComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void AddPandorasByPrimaryAssetIds(const TArray<FPrimaryAssetId>& PandoraDefinitions);

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void ActivatePandoras(const TArray<FPrimaryAssetId>& PandoraDefinitions);

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void FilterPandoras(UPandoraInstance* PandoraInstance);

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void AddValueToMap(FGameplayTag TypeTag, UPandoraInstance* PandoraInstance);

protected:
	void RebuildFilteredPandoraMap();
	static const TArray<FGameplayTag>& GetFilterTypeTags();

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Default", meta = (DisplayName = "All Pandroa Definition"))
	TArray<FPrimaryAssetId> AllPandroaDefinition;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	FPandoraList AllPandoraList;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	TMap<FGameplayTag, FPandoraList> Map_Type_PandoraList;
};
