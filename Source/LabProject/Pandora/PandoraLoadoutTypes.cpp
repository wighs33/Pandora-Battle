#include "Pandora/PandoraLoadoutTypes.h"

#include "Pandora/PandoraDefinition.h"

bool PandoraLoadout::IsLoadoutDirection(const EEnum_Direction Direction)
{
	return Direction == EEnum_Direction::Left
		|| Direction == EEnum_Direction::Up
		|| Direction == EEnum_Direction::Right;
}

FName PandoraLoadout::GetDirectionSaveName(const EEnum_Direction Direction)
{
	switch (Direction)
	{
	case EEnum_Direction::Left:
		return TEXT("Left");
	case EEnum_Direction::Up:
		return TEXT("Up");
	case EEnum_Direction::Right:
		return TEXT("Right");
	default:
		return NAME_None;
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

TMap<FName, FName> PandoraLoadout::MakeSaveNames(const TArray<FPandoraLoadoutSlot>& Slots)
{
	TMap<FName, FName> LoadoutSaveNames;
	for (const FPandoraLoadoutSlot& Slot : Slots)
	{
		const FName DirectionName = GetDirectionSaveName(Slot.Direction);
		if (DirectionName.IsNone() || !Slot.PandoraDefinition)
		{
			continue;
		}

		LoadoutSaveNames.Add(DirectionName, Slot.PandoraDefinition->GetFName());
	}

	return LoadoutSaveNames;
}
