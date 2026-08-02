#pragma once

#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTService_UpdateEnemyTarget.generated.h"

UCLASS()
class LABPROJECT_API UBTService_UpdateEnemyTarget : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdateEnemyTarget();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector LastKnownTargetLocationKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector DistanceToTargetKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector AttackRangeKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector HasRangedWeaponKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector HasLineOfSightKey;

	UPROPERTY(EditAnywhere, Category = "Target")
	bool bUseCachedTargetWhenBlackboardTargetIsInvalid = false;

	UPROPERTY(EditAnywhere, Category = "Auto Attack")
	bool bAutoAttackWhenInRange = true;

	UPROPERTY(EditAnywhere, Category = "Auto Attack", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float AutoAttackInterval = 1.0f;
double LastAutoAttackTime = -1000000.0;
};
