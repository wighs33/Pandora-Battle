// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemComponent.h"
#include "PdAbilitySystemComponent.generated.h"

class UGameplayEffect;

UENUM(BlueprintType)
enum class EPdStatChangeOperation : uint8
{
	Add UMETA(DisplayName = "Add"),
	Multiply UMETA(DisplayName = "Multiply")
};

UCLASS()
class LABPROJECT_API UPdAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UPdAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "!AbilitySystem|Stat", meta = (GameplayTagFilter = "Status"))
	bool ApplyStatUpEffectByTag(TSubclassOf<UGameplayEffect> GameplayEffectClass, FGameplayTag StatTag, float Magnitude, EPdStatChangeOperation Operation = EPdStatChangeOperation::Add, float Level = 1.f);
};
