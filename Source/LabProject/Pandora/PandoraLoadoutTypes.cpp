#include "Pandora/PandoraLoadoutTypes.h"

bool PandoraLoadout::IsLoadoutDirection(const EEnum_Direction Direction)
{
	return Direction == EEnum_Direction::Left
		|| Direction == EEnum_Direction::Up
		|| Direction == EEnum_Direction::Right;
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
