#include "Pandora/PandoraLoadoutTypes.h"

bool PandoraLoadout::IsLoadoutDirection(const EEnum_Direction Direction)
{
	return Direction == EEnum_Direction::Left
		|| Direction == EEnum_Direction::Up
		|| Direction == EEnum_Direction::Right;
}

EEnum_Direction PandoraLoadout::GetDirectionFromLoadoutNumber(const int32 LoadoutNumber)
{
	switch (LoadoutNumber)
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

int32 PandoraLoadout::GetLoadoutNumberFromDirection(const EEnum_Direction Direction)
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

FPandoraLoadoutSlot* PandoraLoadout::FindSlot(TArray<FPandoraLoadoutSlot>& Slots, const EEnum_Direction Direction)
{
	for (FPandoraLoadoutSlot& Slot : Slots)
	{
		if (Slot.Direction == Direction)
		{
			return &Slot;
		}
	}

	return nullptr;
}

const FPandoraLoadoutSlot* PandoraLoadout::FindSlot(const TArray<FPandoraLoadoutSlot>& Slots, const EEnum_Direction Direction)
{
	for (const FPandoraLoadoutSlot& Slot : Slots)
	{
		if (Slot.Direction == Direction)
		{
			return &Slot;
		}
	}

	return nullptr;
}
