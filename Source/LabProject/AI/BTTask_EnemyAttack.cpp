#include "AI/BTTask_EnemyAttack.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "Character/EnemyBase.h"

UBTTask_EnemyAttack::UBTTask_EnemyAttack()
{
	NodeName = TEXT("Pd Enemy Attack");
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, BlackboardKey), AActor::StaticClass());
}

EBTNodeResult::Type UBTTask_EnemyAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	AEnemyBase* Enemy = AIController ? Cast<AEnemyBase>(AIController->GetPawn()) : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(BlackboardKey.SelectedKeyName)) : nullptr;

	if (!AIController || !Enemy || !Enemy->IsActorValidAttackTarget(Target))
	{
		return EBTNodeResult::Failed;
	}

	const float DistanceToTarget = Enemy->GetAttackDistanceToActor(Target);

	const float AttackStartDistance = Enemy->GetAttackStartDistance();
	if (bRequireTargetInAttackRange && DistanceToTarget > AttackStartDistance)
	{

		if (bMoveToTargetWhenOutOfRange)
		{
			Enemy->RequestMoveToAttackTarget(Target);
			return EBTNodeResult::Succeeded;
		}
		return EBTNodeResult::Failed;
	}

	if (bStopMovementBeforeAttack)
	{
		AIController->StopMovement();
	}

	if (!Enemy->IsAttackEnabled())
	{
		return EBTNodeResult::Succeeded;
	}

	Enemy->SetAttackTarget(Target);

	Enemy->Attack();
	return EBTNodeResult::Succeeded;
}
