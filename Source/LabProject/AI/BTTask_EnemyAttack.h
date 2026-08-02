#pragma once

#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_EnemyAttack.generated.h"

class AEnemyBase;

UCLASS()
class LABPROJECT_API UBTTask_EnemyAttack : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_EnemyAttack();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	bool bRequireTargetInAttackRange = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	bool bMoveToTargetWhenOutOfRange = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	bool bStopMovementBeforeAttack = true;
};
