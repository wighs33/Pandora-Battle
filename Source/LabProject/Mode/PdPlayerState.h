#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "GameFramework/PlayerState.h"
#include "PdPlayerState.generated.h"

class UPandoraComponent;
class UPandoraTreeComponent;
class UPdAbilitySystemComponent;
class UBasicAttributeSet;
class UInventoryComponent;
class ULevelingComponent;
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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* NewPlayerState) override;

	//------------------------------------------------------------------------------------------------------------------
	//--- Weapon/Pandora Loadout Selection
	UFUNCTION(BlueprintPure, Category = "!Loadout")
	int32 GetSelectedWeaponPandoraLoadoutNumber() const
	{
		return SelectedWeaponPandoraLoadoutNumber;
	}

	UFUNCTION(BlueprintCallable, Category = "!Loadout")
	void RequestSetSelectedWeaponPandoraLoadoutNumber(int32 LoadoutNumber);

	/** Applies both the weapon and Pandora assigned to the selected loadout number. Authority only. */
	bool ApplySelectedWeaponPandoraLoadout();

	//------------------------------------------------------------------------------------------------------------------
	//--- Ability System
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	FORCEINLINE UPdAbilitySystemComponent* GetPdAbilitySystemComponent() const { return AbilitySystemComponent.Get(); }
	UBasicAttributeSet* GetPdAttributeSet() const;

	//------------------------------------------------------------------------------------------------------------------
	//--- Components
	UPlayerRewardComponent* GetPlayerRewardComponent() const;
	FORCEINLINE UPlayerNotificationComponent* GetPlayerNotificationComponent() const { return NotificationComponent.Get(); }
	UStatUpgradeComponent* GetStatUpgradeComponent() const;
	FORCEINLINE UPlayerMatchComponent* GetPlayerMatchComponent() const { return PlayerMatchComponent.Get(); }
	UFUNCTION()
	FORCEINLINE ULevelingComponent* GetLevelingComponent() const { return LevelingComponent.Get(); }
	UInventoryComponent* GetInventoryComponent() const;
	virtual USkinComponent* GetSkinComponent() const;
	UPandoraComponent* GetPandoraComponent() const;
	UPandoraTreeComponent* GetPandoraTreeComponent() const;

protected:
	virtual FPlayerMatchIdentity GetMatchIdentityForCopyProperties() const;

	UFUNCTION(Server, Reliable)
	void ServerSetSelectedWeaponPandoraLoadoutNumber(int32 LoadoutNumber);

private:
	void SetSelectedWeaponPandoraLoadoutNumberInternal(int32 LoadoutNumber);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "!Loadout", meta = (AllowPrivateAccess = "true"))
	int32 SelectedWeaponPandoraLoadoutNumber = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPdAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Match", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPlayerMatchComponent> PlayerMatchComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Notifications", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPlayerNotificationComponent> NotificationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Leveling", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULevelingComponent> LevelingComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Skin", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkinComponent> SkinComponent;
};
