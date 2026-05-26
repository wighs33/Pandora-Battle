#include "UI/PandoraLoadoutUiModel.h"

#include "Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Pandora/PandoraComponent.h"
#include "Pandora/PandoraDefinition.h"

EEnum_Direction FPandoraLoadoutUiModel::GetDirectionFromSelectSlotNumber(const int32 SlotNumber)
{
	switch (SlotNumber)
	{
	case 1:
		return EEnum_Direction::Left;
	case 2:
		return EEnum_Direction::Up;
	case 3:
		return EEnum_Direction::Right;
	default:
		return EEnum_Direction::Center;
	}
}

int32 FPandoraLoadoutUiModel::GetSelectSlotNumberFromDirection(const EEnum_Direction Direction)
{
	switch (Direction)
	{
	case EEnum_Direction::Left:
		return 1;
	case EEnum_Direction::Up:
		return 2;
	case EEnum_Direction::Right:
		return 3;
	default:
		return 0;
	}
}

TArray<FPandoraSelectSlotUiData> FPandoraLoadoutUiModel::BuildSelectSlots(
	const UPandoraComponent* PandoraComponent,
	const UItemInstance* LeftWeapon,
	const UItemInstance* UpWeapon,
	const UItemInstance* RightWeapon)
{
	TArray<FPandoraSelectSlotUiData> Slots;
	Slots.Reserve(3);

	for (const EEnum_Direction Direction : { EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right })
	{
		FPandoraSelectSlotUiData& Slot = Slots.AddDefaulted_GetRef();
		Slot.Direction = Direction;
		Slot.SlotNumber = GetSelectSlotNumberFromDirection(Direction);
		Slot.PandoraDefinition = PandoraComponent ? PandoraComponent->GetPandoraLoadoutDefinition(Direction) : nullptr;
		Slot.IconTexture = Slot.PandoraDefinition ? Slot.PandoraDefinition->IconTexture.Get() : nullptr;
		Slot.bCompatibleWithWeapon = IsPandoraCompatibleWithWeapon(
			Slot.PandoraDefinition,
			GetWeaponForDirection(Direction, LeftWeapon, UpWeapon, RightWeapon));
	}

	return Slots;
}

bool FPandoraLoadoutUiModel::IsPandoraCompatibleWithWeapon(
	const UPandoraDefinition* PandoraDefinition,
	const UItemInstance* WeaponInstance)
{
	if (!PandoraDefinition)
	{
		return true;
	}

	const UItemDefinition* ItemDefinition = IsValid(WeaponInstance) ? WeaponInstance->ItemDefinition.Get() : nullptr;
	return PandoraDefinition->IsCompatibleWithWeaponDefinition(ItemDefinition);
}

const UItemInstance* FPandoraLoadoutUiModel::GetWeaponForDirection(
	const EEnum_Direction Direction,
	const UItemInstance* LeftWeapon,
	const UItemInstance* UpWeapon,
	const UItemInstance* RightWeapon)
{
	switch (Direction)
	{
	case EEnum_Direction::Left:
		return LeftWeapon;
	case EEnum_Direction::Up:
		return UpWeapon;
	case EEnum_Direction::Right:
		return RightWeapon;
	default:
		return nullptr;
	}
}
