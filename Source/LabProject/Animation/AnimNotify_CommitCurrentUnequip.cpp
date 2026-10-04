#include "Animation/AnimNotify_CommitCurrentUnequip.h"

#include "Component/Player/EquipmentComponent.h"

bool UAnimNotify_CommitCurrentUnequip::CommitEquipment(UEquipmentComponent& EquipmentComponent) const
{
	return EquipmentComponent.UnequipCurrentWeapon();
}
