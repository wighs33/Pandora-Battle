#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "PdPlayerState.generated.h"

class UPandoraComponent;
class UPandoraTreeComponent;
class UPdAbilitySystemComponent;
class UBasicAttributeSet;
class UInventoryComponent;
class UPlayerNotificationComponent;
class UPlayerRewardComponent;
class USkinComponent;
class UStatUpgradeComponent;

UCLASS()
class LABPROJECT_API APdPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	APdPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//------------------------------------------------------------------------------------------------------------------
	//--- Ability System
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UPdAbilitySystemComponent* GetPdAbilitySystemComponent() const;
	UBasicAttributeSet* GetPdAttributeSet() const;

	//------------------------------------------------------------------------------------------------------------------
	//--- Components
	UPlayerRewardComponent* GetPlayerRewardComponent() const;
	UPlayerNotificationComponent* GetPlayerNotificationComponent() const;
	UStatUpgradeComponent* GetStatUpgradeComponent() const;
	UInventoryComponent* GetInventoryComponent() const;
	USkinComponent* GetSkinComponent() const;
	UPandoraComponent* GetPandoraComponent() const;

	UFUNCTION(BlueprintPure, Category = "!Components")
	UPandoraTreeComponent* GetPandoraTreeComponent() const;

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Ability System
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPdAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Notifications", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPlayerNotificationComponent> NotificationComponent;
};
