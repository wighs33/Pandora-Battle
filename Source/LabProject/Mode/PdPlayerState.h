#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "PdPlayerState.generated.h"

class UPandoraComponent;
class UPandoraTreeComponent;
class UPdAbilitySystemComponent;
class UInventoryComponent;
class ULevelingComponent;
class ULobbyPlayerStateComponent;
class UBasicAttributeSet;
class UPlayerLoadoutComponent;
class UPlayerRewardComponent;
class USkinComponent;
class UStatUpgradeComponent;

/**
 * 로비와 경기에서 사용하는 상태, 기본 속성과 기능별 컴포넌트를 소유한다.
 *
 * 세부 게임플레이는 컴포넌트에 위임하고, GAS와 GFCM의 연결을 담당한다.
 */
UCLASS()
class LABPROJECT_API APdPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	APdPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//--------------------------------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void CopyProperties(APlayerState* NewPlayerState) override;

	//--------------------------------------------------------------------------------------------------------------------------------------------
	//--- Components
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UPlayerLoadoutComponent* GetPlayerLoadoutComponent() const { return PlayerLoadoutComponent.Get(); }
	UPlayerRewardComponent* GetPlayerRewardComponent() const;
	UStatUpgradeComponent* GetStatUpgradeComponent() const;
	UPlayerMatchComponent* GetPlayerMatchComponent() const { return PlayerMatchComponent.Get(); }
	ULobbyPlayerStateComponent* GetLobbyPlayerStateComponent() const { return LobbyPlayerStateComponent.Get(); }
	ULevelingComponent* GetLevelingComponent() const { return LevelingComponent.Get(); }
	UInventoryComponent* GetInventoryComponent() const;
	USkinComponent* GetSkinComponent() const { return SkinComponent.Get(); }
	UPandoraComponent* GetPandoraComponent() const;
	UPandoraTreeComponent* GetPandoraTreeComponent() const;

	//--------------------------------------------------------------------------------------------------------------------------------------------
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Loadout", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPlayerLoadoutComponent> PlayerLoadoutComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPdAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, Category = "Abilities")
	TObjectPtr<UBasicAttributeSet> BasicAttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Match", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPlayerMatchComponent> PlayerMatchComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Lobby|State", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULobbyPlayerStateComponent> LobbyPlayerStateComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Leveling", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULevelingComponent> LevelingComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Skin", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkinComponent> SkinComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Inventory")
	TObjectPtr<UInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Pandora")
	TObjectPtr<UPandoraComponent> PandoraComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Reward")
	TObjectPtr<UPlayerRewardComponent> PlayerRewardComponent;

	UPROPERTY(VisibleAnywhere, Category = "!StatUpgrade")
	TObjectPtr<UStatUpgradeComponent> StatUpgradeComponent;

	UPROPERTY(VisibleAnywhere, Category = "!PandoraTree")
	TObjectPtr<UPandoraTreeComponent> PandoraTreeComponent;
};
