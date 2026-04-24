// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "PdPlayerState.generated.h"

class UPdAbilitySystemComponent;
class UPdAttributeSet;

UCLASS()
class LABPROJECT_API APdPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	APdPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** CommonAbilitySystemComponent를 반환합니다 */
	UPdAbilitySystemComponent* GetPdAbilitySystemComponent() const { return AbilitySystemComponent; }

	/** 플레이어의 기본 AttributeSet을 반환합니다 */
	UPdAttributeSet* GetPdAttributeSet() const { return AttributeSet; }

private:
	/** Ability System Component */
	UPROPERTY(VisibleAnywhere, Category = "Abilities")
	TObjectPtr<UPdAbilitySystemComponent> AbilitySystemComponent;

	/** 기본 AttributeSet */
	UPROPERTY(VisibleAnywhere, Category = "Abilities")
	TObjectPtr<UPdAttributeSet> AttributeSet;
};
