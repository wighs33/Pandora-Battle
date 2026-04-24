// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "Components/PlayerStateComponent.h"
#include "SkinComponent.generated.h"

class USkinInstance;

DECLARE_LOG_CATEGORY_EXTERN(SkinComponentLog, Log, All);

USTRUCT(BlueprintType, Blueprintable)
struct FSkinList
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	TArray<TObjectPtr<USkinInstance>> Skins;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API USkinComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!Inventory", meta = (DisplayName = "Make&AddSkins"))
	void MakeAndAddSkins(const TArray<FPrimaryAssetId>& SkinDefinitions);

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void AddSkinsByPrimaryAssetIds(const TArray<FPrimaryAssetId>& SkinDefinitions);

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void FilterSkin(USkinInstance* SkinInstance);

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void AddValueToMap(FGameplayTag TypeTag, USkinInstance* SkinInstance);

protected:
	static const TArray<FGameplayTag>& GetFilterTypeTags();

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	FSkinList AllSkinList;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	TMap<FGameplayTag, FSkinList> Map_Type_SkinList;
};
