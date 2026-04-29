// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "PdPlayerState.generated.h"

class UPdAbilitySystemComponent;
class UPdAttributeSet;
class UPlayerRewardComponent;
class UInventoryComponent;
class USkinComponent;
class UPandoraComponent;
class AActor;

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
	bool ApplyInteractRewards(AActor* InteractableActor);

	/** CommonAbilitySystemComponent를 반환합니다 */
	UPdAbilitySystemComponent* GetPdAbilitySystemComponent() const { return AbilitySystemComponent; }

	/** 플레이어의 기본 AttributeSet을 반환합니다 */
	UPdAttributeSet* GetPdAttributeSet() const { return AttributeSet; }
	UPlayerRewardComponent* GetPlayerRewardComponent() const { return PlayerRewardComponent; }
	UInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
	USkinComponent* GetSkinComponent() const { return SkinComponent; }
	UPandoraComponent* GetPandoraComponent() const { return PandoraComponent; }

private:
	/** Ability System Component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPdAbilitySystemComponent> AbilitySystemComponent;

	/** 기본 AttributeSet */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPdAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPlayerRewardComponent> PlayerRewardComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skin", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkinComponent> SkinComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pandora", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPandoraComponent> PandoraComponent;
};
