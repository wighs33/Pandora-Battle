// Fill out your copyright notice in the Description page of Project Settings.


#include "SkinDefinition.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinDefinition)

DEFINE_LOG_CATEGORY(SkinDefinitionLog);

FPrimaryAssetId USkinDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("SkinDefinition"), GetFName());
}
