#include "Mode/PdPlayerState.h"

#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/PdAttributeSet.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Item/InventoryComponent.h"
#include "Pandora/PandoraComponent.h"
#include "PlayerComponent/PlayerRewardComponent.h"
#include "PlayerComponent/StatUpgradeComponent.h"
#include "Skin/SkinComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdPlayerState)

namespace
{
	template<typename ComponentType>
	ComponentType* FindPlayerStateComponent(const AActor* Owner)
	{
		return Owner ? Owner->FindComponentByClass<ComponentType>() : nullptr;
	}
}

APdPlayerState::APdPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetNetUpdateFrequency(100.0f);

	AbilitySystemComponent = CreateDefaultSubobject<UPdAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
}

//----------------------------------------------------------------------------------------------------------------------
//--- Engine Callbacks
void APdPlayerState::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

void APdPlayerState::BeginPlay()
{
	Super::BeginPlay();

	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);
}

void APdPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	Super::EndPlay(EndPlayReason);
}

//----------------------------------------------------------------------------------------------------------------------
//--- Ability System
UAbilitySystemComponent* APdPlayerState::GetAbilitySystemComponent() const
{
	return GetPdAbilitySystemComponent();
}

UPdAbilitySystemComponent* APdPlayerState::GetPdAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

UPdAttributeSet* APdPlayerState::GetPdAttributeSet() const
{
	const UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponent();
	return ASC ? const_cast<UPdAttributeSet*>(ASC->GetSet<UPdAttributeSet>()) : nullptr;
}

//----------------------------------------------------------------------------------------------------------------------
//--- Components
UPlayerRewardComponent* APdPlayerState::GetPlayerRewardComponent() const
{
	return FindPlayerStateComponent<UPlayerRewardComponent>(this);
}

UStatUpgradeComponent* APdPlayerState::GetStatUpgradeComponent() const
{
	return FindPlayerStateComponent<UStatUpgradeComponent>(this);
}

UInventoryComponent* APdPlayerState::GetInventoryComponent() const
{
	return FindPlayerStateComponent<UInventoryComponent>(this);
}

USkinComponent* APdPlayerState::GetSkinComponent() const
{
	return FindPlayerStateComponent<USkinComponent>(this);
}

UPandoraComponent* APdPlayerState::GetPandoraComponent() const
{
	return FindPlayerStateComponent<UPandoraComponent>(this);
}
