#include "Mode/PdPlayerState.h"

#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Component/Item/InventoryComponent.h"
#include "Lobby/Contents/LobbyPlayerState.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Player/LevelingComponent.h"
#include "Component/Player/PlayerNotificationComponent.h"
#include "Component/Player/PlayerRewardComponent.h"
#include "Component/Player/StatUpgradeComponent.h"
#include "Component/Skin/SkinComponent.h"
#include "Net/UnrealNetwork.h"
#include "Pandora/PandoraLoadoutTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdPlayerState)

namespace
{
	int32 SanitizeWeaponPandoraLoadoutNumber(const int32 LoadoutNumber)
	{
		return PandoraLoadout::GetLoadoutNumberFromDirection(
			PandoraLoadout::GetDirectionFromLoadoutNumber(LoadoutNumber));
	}

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
	PlayerMatchComponent = CreateDefaultSubobject<UPlayerMatchComponent>(TEXT("PlayerMatchComponent"));
	NotificationComponent = CreateDefaultSubobject<UPlayerNotificationComponent>(TEXT("NotificationComponent"));
	LevelingComponent = CreateDefaultSubobject<ULevelingComponent>(TEXT("LevelingComponent"));
	SkinComponent = CreateDefaultSubobject<USkinComponent>(TEXT("SkinComponent"));
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

void APdPlayerState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(
		APdPlayerState,
		SelectedWeaponPandoraLoadoutNumber);
}

void APdPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);

	if (APdPlayerState* PdPlayerState = Cast<APdPlayerState>(NewPlayerState))
	{
		PdPlayerState->SetSelectedWeaponPandoraLoadoutNumberInternal(
			SelectedWeaponPandoraLoadoutNumber);
	}

	if (!PlayerMatchComponent)
	{
		return;
	}

	const FPlayerMatchIdentity MatchIdentityToCopy = GetMatchIdentityForCopyProperties();
	if (ALobbyPlayerState* LobbyPlayerState = Cast<ALobbyPlayerState>(NewPlayerState))
	{
		LobbyPlayerState->ImportPlayerMatchIdentity(MatchIdentityToCopy);
	}
	else if (APdPlayerState* PdPlayerState = Cast<APdPlayerState>(NewPlayerState))
	{
		const bool bCopyMatchStats = !IsA<ALobbyPlayerState>();
		PlayerMatchComponent->CopyMatchStateTo(
			PdPlayerState->GetPlayerMatchComponent(),
			MatchIdentityToCopy,
			bCopyMatchStats);
	}
}

void APdPlayerState::RequestSetSelectedWeaponPandoraLoadoutNumber(
	const int32 LoadoutNumber)
{
	const int32 SanitizedLoadoutNumber =
		SanitizeWeaponPandoraLoadoutNumber(LoadoutNumber);
	SetSelectedWeaponPandoraLoadoutNumberInternal(SanitizedLoadoutNumber);
	if (!HasAuthority())
	{
		ServerSetSelectedWeaponPandoraLoadoutNumber(
			SanitizedLoadoutNumber);
	}
}

void APdPlayerState::ServerSetSelectedWeaponPandoraLoadoutNumber_Implementation(
	const int32 LoadoutNumber)
{
	SetSelectedWeaponPandoraLoadoutNumberInternal(LoadoutNumber);
}

void APdPlayerState::SetSelectedWeaponPandoraLoadoutNumberInternal(
	const int32 LoadoutNumber)
{
	const int32 SanitizedLoadoutNumber =
		SanitizeWeaponPandoraLoadoutNumber(LoadoutNumber);
	if (SelectedWeaponPandoraLoadoutNumber != SanitizedLoadoutNumber)
	{
		SelectedWeaponPandoraLoadoutNumber = SanitizedLoadoutNumber;
		if (HasAuthority())
		{
			ForceNetUpdate();
		}
	}

	if (HasAuthority())
	{
		ApplySelectedWeaponPandoraLoadout();
	}
}

bool APdPlayerState::ApplySelectedWeaponPandoraLoadout()
{
	if (!HasAuthority())
	{
		return false;
	}

	const EEnum_Direction SelectedDirection =
		PandoraLoadout::GetDirectionFromLoadoutNumber(
			SelectedWeaponPandoraLoadoutNumber);
	UInventoryComponent* Inventory = GetInventoryComponent();
	UPandoraComponent* PandoraComponent = GetPandoraComponent();
	UEquipmentComponent* Equipment = GetPawn()
		? GetPawn()->FindComponentByClass<UEquipmentComponent>()
		: nullptr;
	bool bHandled = false;

	if (Equipment)
	{
		UItemInstance* SelectedWeapon =
			SelectedDirection != EEnum_Direction::Center && Inventory
				? Inventory->GetPandoraWeaponLoadoutItem(SelectedDirection)
				: nullptr;
		if (SelectedWeapon)
		{
			Equipment->RequestWeaponSelectionForDirection(
				SelectedDirection,
				SelectedWeapon);
		}
		else if (Equipment->GetCurrentWeaponId().IsValid()
			|| Equipment->GetCurrentWeaponDefinition()
			|| Equipment->GetRequestedWeaponDefinition())
		{
			Equipment->RequestWeaponUnequip();
		}
		bHandled = true;
	}

	if (PandoraComponent)
	{
		const UPandoraDefinition* PandoraDefinition =
			SelectedDirection == EEnum_Direction::Center
				? nullptr
				: PandoraComponent->GetPandoraLoadoutDefinition(SelectedDirection);
		if (PandoraDefinition
			&& (PandoraComponent->GetCurrentPandoraDefinition() != PandoraDefinition
				|| PandoraComponent->GetCurrentPandoraLoadoutDirection()
					!= SelectedDirection))
		{
			PandoraComponent->RequestPandoraSelectionForDirection(
				SelectedDirection,
				PandoraDefinition);
		}
		else if (!PandoraDefinition
			&& (PandoraComponent->GetCurrentPandoraDefinition()
				|| PandoraComponent->GetCurrentPandoraLoadoutDirection()
					!= EEnum_Direction::Center))
		{
			PandoraComponent->RequestPandoraSelection(nullptr);
		}
		bHandled = true;
	}

	return bHandled;
}

//----------------------------------------------------------------------------------------------------------------------
//--- Ability System
UAbilitySystemComponent* APdPlayerState::GetAbilitySystemComponent() const
{
	return GetPdAbilitySystemComponent();
}

UBasicAttributeSet* APdPlayerState::GetPdAttributeSet() const
{
	const UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponent();
	return ASC ? const_cast<UBasicAttributeSet*>(ASC->GetSet<UBasicAttributeSet>()) : nullptr;
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
	if (SkinComponent)
	{
		return SkinComponent.Get();
	}

	return FindPlayerStateComponent<USkinComponent>(this);
}

UPandoraComponent* APdPlayerState::GetPandoraComponent() const
{
	return FindPlayerStateComponent<UPandoraComponent>(this);
}

UPandoraTreeComponent* APdPlayerState::GetPandoraTreeComponent() const
{
	return FindPlayerStateComponent<UPandoraTreeComponent>(this);
}

FPlayerMatchIdentity APdPlayerState::GetMatchIdentityForCopyProperties() const
{
	return PlayerMatchComponent
		? PlayerMatchComponent->GetPlayerMatchIdentity()
		: FPlayerMatchIdentity();
}
