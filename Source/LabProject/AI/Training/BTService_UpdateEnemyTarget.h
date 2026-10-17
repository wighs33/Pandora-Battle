#pragma once

#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTService_UpdateEnemyTarget.generated.h"

class UBlackboardComponent;

UCLASS()
class LABPROJECT_API UBTService_UpdateEnemyTarget : public UBTService
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UBTService_UpdateEnemyTarget();

protected:
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

	UPROPERTY(EditAnywhere, Category = "Auto Attack")
	bool bAutoAttackWhenInRange = true;

	UPROPERTY(EditAnywhere, Category = "Auto Attack", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float AutoAttackInterval = 1.0f;
private:
	/** 표적 정보를 Blackboard에 적는다. 표적이 없으면 표적 키를 비운다. */
	void WriteTargetKeys(UBlackboardComponent& Blackboard, AActor* Target, float DistanceToTarget, float AttackRange,
		bool bHasRangedWeapon, bool bHasLineOfSight) const;

	/** 마지막으로 자동 공격한 시각. 비어 있으면 아직 공격하지 않았다. */
	TOptional<double> LastAutoAttackTime;
};
