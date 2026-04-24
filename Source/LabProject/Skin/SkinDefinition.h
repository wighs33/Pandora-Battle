// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "SkinDefinition.generated.h"

class UTexture2D;

DECLARE_LOG_CATEGORY_EXTERN(SkinDefinitionLog, Log, All);

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API USkinDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

public:
	/** 아이템 표시 이름 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skin")
	FText DisplayName;

	/** 아이템 설명 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skin")
	FText Description;

	/** 아이콘 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skin")
	TObjectPtr<UTexture2D> IconTexture = nullptr;
	
	/** 게임 플레이 태그 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skin")
	FGameplayTag IdTag;
};
