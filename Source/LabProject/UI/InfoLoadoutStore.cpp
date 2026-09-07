#include "UI/InfoLoadoutStore.h"

#include "Character/PdPlayer.h"
#include "Component/Item/InventoryComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Component/Player/PlayerLoadoutComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Item/ItemInstance.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraInstance.h"
#include "Pandora/PandoraLoadoutTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InfoLoadoutStore)

UWorld* UInfoLoadoutStore::GetWorld() const
{
	return IsValid(OwningController) ? OwningController->GetWorld() : Super::GetWorld();
}

void UInfoLoadoutStore::Initialize(APdPlayerController* InController)
{
	if (OwningController != InController)
	{
		Deinitialize();
		OwningController = InController;
	}

	RefreshBindings();
}

void UInfoLoadoutStore::Deinitialize()
{
	UnbindInventoryComponent();
	UnbindPandoraComponent();
	OwningController = nullptr;
	RebuildLoadoutState();
}

void UInfoLoadoutStore::RefreshBindings()
{
	APdPlayerState* PlayerState = IsValid(OwningController)
		? OwningController->GetPlayerState<APdPlayerState>()
		: nullptr;
	UInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	const bool bBindingsChanged = BoundInventoryComponent != Inventory
		|| BoundPandoraComponent != PandoraComponent;

	if (BoundInventoryComponent != Inventory)
	{
		UnbindInventoryComponent();
		BoundInventoryComponent = Inventory;
		if (BoundInventoryComponent)
		{
			InventoryChangedDelegateHandle = BoundInventoryComponent->OnInventoryChanged.AddUObject(
				this,
				&ThisClass::HandleInventoryChanged);
			WeaponLoadoutChangedDelegateHandle =
				BoundInventoryComponent->OnPandoraWeaponLoadoutChanged.AddUObject(
					this,
					&ThisClass::HandleWeaponLoadoutChanged);
		}
	}

	if (BoundPandoraComponent != PandoraComponent)
	{
		UnbindPandoraComponent();
		BoundPandoraComponent = PandoraComponent;
		if (BoundPandoraComponent)
		{
			BoundPandoraComponent->OnPandoraLoadoutChanged.AddUniqueDynamic(
				this,
				&ThisClass::HandlePandoraLoadoutChanged);
		}
	}

	RebuildLoadoutState();
	if (bBindingsChanged)
	{
		OnStateChanged.Broadcast(EInfoLoadoutStateChange::Bindings);
	}
}

UInventoryComponent* UInfoLoadoutStore::GetInventoryComponent() const
{
	return IsValid(BoundInventoryComponent) ? BoundInventoryComponent.Get() : nullptr;
}

UPandoraComponent* UInfoLoadoutStore::GetPandoraComponent() const
{
	return IsValid(BoundPandoraComponent) ? BoundPandoraComponent.Get() : nullptr;
}

UEquipmentComponent* UInfoLoadoutStore::GetEquipmentComponent() const
{
	const APdPlayer* Player = IsValid(OwningController)
		? Cast<APdPlayer>(OwningController->GetPawn())
		: nullptr;
	return Player ? Player->GetEquipmentComponent() : nullptr;
}

UItemInstance* UInfoLoadoutStore::GetSelectedWeapon(const EEnum_Direction Direction) const
{
	switch (Direction)
	{
	case EEnum_Direction::Left:
		return IsValid(LeftWeapon) ? LeftWeapon.Get() : nullptr;
	case EEnum_Direction::Up:
		return IsValid(UpWeapon) ? UpWeapon.Get() : nullptr;
	case EEnum_Direction::Right:
		return IsValid(RightWeapon) ? RightWeapon.Get() : nullptr;
	default:
		return nullptr;
	}
}

UPandoraInstance* UInfoLoadoutStore::GetSelectedPandora(const EEnum_Direction Direction) const
{
	switch (Direction)
	{
	case EEnum_Direction::Left:
		return IsValid(LeftPandora) ? LeftPandora.Get() : nullptr;
	case EEnum_Direction::Up:
		return IsValid(UpPandora) ? UpPandora.Get() : nullptr;
	case EEnum_Direction::Right:
		return IsValid(RightPandora) ? RightPandora.Get() : nullptr;
	default:
		return nullptr;
	}
}

const UPandoraDefinition* UInfoLoadoutStore::GetSelectedPandoraDefinition(
	const EEnum_Direction Direction) const
{
	switch (Direction)
	{
	case EEnum_Direction::Left:
		return IsValid(LeftPandoraDefinition) ? LeftPandoraDefinition.Get() : nullptr;
	case EEnum_Direction::Up:
		return IsValid(UpPandoraDefinition) ? UpPandoraDefinition.Get() : nullptr;
	case EEnum_Direction::Right:
		return IsValid(RightPandoraDefinition) ? RightPandoraDefinition.Get() : nullptr;
	default:
		return nullptr;
	}
}

bool UInfoLoadoutStore::WouldSelectedDirectionChangeLoadout(
	const EEnum_Direction Direction) const
{
	const UEquipmentComponent* Equipment = GetEquipmentComponent();
	const UPandoraComponent* PandoraComponent = GetPandoraComponent();
	const APdPlayerState* PlayerState = IsValid(OwningController)
		? OwningController->GetPlayerState<APdPlayerState>()
		: nullptr;
	const UPlayerLoadoutComponent* LoadoutComponent = PlayerState ? PlayerState->GetPlayerLoadoutComponent() : nullptr;
	if (Direction == EEnum_Direction::Down)
	{
		const bool bWouldChangeSelectedLoadout = LoadoutComponent
			&& LoadoutComponent->GetSelectedLoadoutNumber() != 0;
		const bool bWouldUnequipWeapon = Equipment
			&& (Equipment->GetCurrentWeaponId().IsValid()
				|| Equipment->GetCurrentWeaponDefinition());
		const bool bWouldClearPandora = PandoraComponent
			&& PandoraComponent->GetCurrentPandoraDefinition();
		return bWouldChangeSelectedLoadout
			|| bWouldUnequipWeapon
			|| bWouldClearPandora;
	}
	if (!PandoraLoadout::IsLoadoutDirection(Direction))
	{
		return false;
	}

	const UPandoraDefinition* PandoraDefinition = GetSelectedPandoraDefinition(Direction);
	const int32 RequestedLoadoutNumber =
		PandoraLoadout::GetLoadoutNumberFromDirection(Direction);
	const bool bWouldChangeSelectedLoadout = LoadoutComponent
		&& LoadoutComponent->GetSelectedLoadoutNumber()
			!= RequestedLoadoutNumber;
	const bool bPandoraWouldChange = PandoraComponent
		&& (PandoraComponent->GetCurrentPandoraDefinition() != PandoraDefinition
			|| (PandoraDefinition
				&& PandoraComponent->GetCurrentPandoraLoadoutDirection() != Direction));

	UItemInstance* Weapon = GetSelectedWeapon(Direction);
	bool bWeaponWouldChange = Equipment
		&& !IsValid(Weapon)
		&& (Equipment->GetCurrentWeaponId().IsValid()
			|| Equipment->GetCurrentWeaponDefinition());
	if (Equipment && IsValid(Weapon))
	{
		const FGuid WeaponId = Weapon->GetOrCreateItemId();
		const FGuid CurrentWeaponId = Equipment->GetCurrentWeaponId();
		const bool bSameWeapon = WeaponId.IsValid() && CurrentWeaponId.IsValid()
			? WeaponId == CurrentWeaponId
			: Equipment->GetCurrentWeaponDefinition() == Weapon->ItemDefinition.Get();
		bWeaponWouldChange = !bSameWeapon
			|| Equipment->GetCurrentWeaponLoadoutDirection() != Direction;
	}
	return bWouldChangeSelectedLoadout
		|| bPandoraWouldChange
		|| bWeaponWouldChange;
}

bool UInfoLoadoutStore::RequestSetConsumableQuickSlot(
	const int32 SlotIndex,
	UItemInstance* ItemInstance)
{
	if (IsValid(BoundInventoryComponent)
		&& BoundInventoryComponent->SetConsumableQuickSlot(SlotIndex, ItemInstance))
	{
		return true;
	}
	return PublishRejectedCommand(EInfoLoadoutStateChange::Inventory);
}

bool UInfoLoadoutStore::RequestClearConsumableQuickSlot(const int32 SlotIndex)
{
	if (IsValid(BoundInventoryComponent)
		&& BoundInventoryComponent->ClearConsumableQuickSlot(SlotIndex))
	{
		return true;
	}
	return PublishRejectedCommand(EInfoLoadoutStateChange::Inventory);
}

bool UInfoLoadoutStore::RequestSetEquipmentSlot(
	const FGameplayTag SlotTag,
	UItemInstance* ItemInstance)
{
	if (IsValid(BoundInventoryComponent)
		&& BoundInventoryComponent->SetEquipmentSlot(SlotTag, ItemInstance))
	{
		return true;
	}
	return PublishRejectedCommand(EInfoLoadoutStateChange::Inventory);
}

bool UInfoLoadoutStore::RequestClearEquipmentSlot(const FGameplayTag SlotTag)
{
	if (IsValid(BoundInventoryComponent)
		&& BoundInventoryComponent->ClearEquipmentSlot(SlotTag))
	{
		return true;
	}
	return PublishRejectedCommand(EInfoLoadoutStateChange::Inventory);
}

bool UInfoLoadoutStore::RequestSetWeaponLoadoutSlot(
	const EEnum_Direction Direction,
	UItemInstance* ItemInstance)
{
	if (IsValid(BoundInventoryComponent)
		&& BoundInventoryComponent->SetPandoraWeaponLoadoutSlot(Direction, ItemInstance))
	{
		return true;
	}
	return PublishRejectedCommand(EInfoLoadoutStateChange::WeaponLoadout);
}

bool UInfoLoadoutStore::RequestClearWeaponLoadoutSlot(const EEnum_Direction Direction)
{
	if (IsValid(BoundInventoryComponent)
		&& BoundInventoryComponent->ClearPandoraWeaponLoadoutSlot(Direction))
	{
		return true;
	}
	return PublishRejectedCommand(EInfoLoadoutStateChange::WeaponLoadout);
}

bool UInfoLoadoutStore::RequestSetPandoraLoadoutSlot(
	const EEnum_Direction Direction,
	const UPandoraDefinition* PandoraDefinition)
{
	if (IsValid(BoundPandoraComponent)
		&& BoundPandoraComponent->RequestSetPandoraLoadoutSlot(Direction, PandoraDefinition))
	{
		return true;
	}
	return PublishRejectedCommand(EInfoLoadoutStateChange::PandoraLoadout);
}

bool UInfoLoadoutStore::RequestSelectLoadoutDirection(const EEnum_Direction Direction)
{
	APdPlayerState* PlayerState = IsValid(OwningController)
		? OwningController->GetPlayerState<APdPlayerState>()
		: nullptr;
	UPlayerLoadoutComponent* LoadoutComponent = PlayerState ? PlayerState->GetPlayerLoadoutComponent() : nullptr;
	if (!LoadoutComponent)
	{
		return false;
	}

	if (Direction != EEnum_Direction::Down
		&& !PandoraLoadout::IsLoadoutDirection(Direction))
	{
		return false;
	}

	LoadoutComponent->RequestSelectLoadout(
		Direction == EEnum_Direction::Down
			? 0
			: PandoraLoadout::GetLoadoutNumberFromDirection(Direction));
	return true;
}

void UInfoLoadoutStore::NotifyPresentationAssetsReady()
{
	PublishStateChange(EInfoLoadoutStateChange::PresentationAssets);
}

void UInfoLoadoutStore::UnbindInventoryComponent()
{
	if (IsValid(BoundInventoryComponent))
	{
		if (InventoryChangedDelegateHandle.IsValid())
		{
			BoundInventoryComponent->OnInventoryChanged.Remove(InventoryChangedDelegateHandle);
		}
		if (WeaponLoadoutChangedDelegateHandle.IsValid())
		{
			BoundInventoryComponent->OnPandoraWeaponLoadoutChanged.Remove(
				WeaponLoadoutChangedDelegateHandle);
		}
	}
	BoundInventoryComponent = nullptr;
	InventoryChangedDelegateHandle.Reset();
	WeaponLoadoutChangedDelegateHandle.Reset();
}

void UInfoLoadoutStore::UnbindPandoraComponent()
{
	if (IsValid(BoundPandoraComponent))
	{
		BoundPandoraComponent->OnPandoraLoadoutChanged.RemoveDynamic(
			this,
			&ThisClass::HandlePandoraLoadoutChanged);
	}
	BoundPandoraComponent = nullptr;
}

void UInfoLoadoutStore::RebuildLoadoutState()
{
	LeftWeapon = IsValid(BoundInventoryComponent)
		? BoundInventoryComponent->GetPandoraWeaponLoadoutItem(EEnum_Direction::Left)
		: nullptr;
	UpWeapon = IsValid(BoundInventoryComponent)
		? BoundInventoryComponent->GetPandoraWeaponLoadoutItem(EEnum_Direction::Up)
		: nullptr;
	RightWeapon = IsValid(BoundInventoryComponent)
		? BoundInventoryComponent->GetPandoraWeaponLoadoutItem(EEnum_Direction::Right)
		: nullptr;

	LeftPandora = IsValid(BoundPandoraComponent)
		? BoundPandoraComponent->GetPandoraLoadoutInstance(EEnum_Direction::Left)
		: nullptr;
	UpPandora = IsValid(BoundPandoraComponent)
		? BoundPandoraComponent->GetPandoraLoadoutInstance(EEnum_Direction::Up)
		: nullptr;
	RightPandora = IsValid(BoundPandoraComponent)
		? BoundPandoraComponent->GetPandoraLoadoutInstance(EEnum_Direction::Right)
		: nullptr;

	LeftPandoraDefinition = IsValid(BoundPandoraComponent)
		? BoundPandoraComponent->GetPandoraLoadoutDefinition(EEnum_Direction::Left)
		: nullptr;
	UpPandoraDefinition = IsValid(BoundPandoraComponent)
		? BoundPandoraComponent->GetPandoraLoadoutDefinition(EEnum_Direction::Up)
		: nullptr;
	RightPandoraDefinition = IsValid(BoundPandoraComponent)
		? BoundPandoraComponent->GetPandoraLoadoutDefinition(EEnum_Direction::Right)
		: nullptr;
}

void UInfoLoadoutStore::PublishStateChange(const EInfoLoadoutStateChange Change)
{
	RebuildLoadoutState();
	OnStateChanged.Broadcast(Change);
}

bool UInfoLoadoutStore::PublishRejectedCommand(const EInfoLoadoutStateChange Change)
{
	PublishStateChange(Change);
	return false;
}

void UInfoLoadoutStore::HandleInventoryChanged()
{
	PublishStateChange(EInfoLoadoutStateChange::Inventory);
}

void UInfoLoadoutStore::HandleWeaponLoadoutChanged()
{
	PublishStateChange(EInfoLoadoutStateChange::WeaponLoadout);
}

void UInfoLoadoutStore::HandlePandoraLoadoutChanged()
{
	PublishStateChange(EInfoLoadoutStateChange::PandoraLoadout);
}
