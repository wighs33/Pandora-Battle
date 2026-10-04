#include "Animation/AnimNotify_CommitRequestedEquip.h"

#include "Component/Player/EquipmentComponent.h"

bool UAnimNotify_CommitRequestedEquip::CommitEquipment(UEquipmentComponent& EquipmentComponent) const
{
	return EquipmentComponent.EquipWeapon();
}
