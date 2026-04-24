// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SkinInstance.generated.h"

class USkinDefinition;
DECLARE_LOG_CATEGORY_EXTERN(SkinInstanceLog, Log, All);

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API USkinInstance : public UObject
{
	GENERATED_BODY()
	
public:
	/** 아이템 정의 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Skin")
	TObjectPtr<const USkinDefinition> SkinDefinition;
};
