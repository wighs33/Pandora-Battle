#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotify_CommitEquipment.h"
#include "AnimNotify_CommitRequestedEquip.generated.h"

UCLASS()
class LABPROJECT_API UAnimNotify_CommitRequestedEquip : public UAnimNotify_CommitEquipment
{
	GENERATED_BODY()

protected:
	virtual bool CommitEquipment(UEquipmentComponent& EquipmentComponent) const override;
	virtual FString GetCommitName() const override { return TEXT("CommitRequestedEquip"); }
};
