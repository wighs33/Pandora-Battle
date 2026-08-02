#include "AI/BTService_UpdateEnemyTarget.h"

#include "AIController.h"
#include "AI/TrainingBotAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "Character/EnemyBase.h"

UBTService_UpdateEnemyTarget::UBTService_UpdateEnemyTarget()
{
	NodeName = TEXT("Update Pd Enemy Target");
	Interval = 0.2f;
	RandomDeviation = 0.05f;
	bCreateNodeInstance = true;

	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, TargetActorKey), AActor::StaticClass());
	LastKnownTargetLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, LastKnownTargetLocationKey));
	DistanceToTargetKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, DistanceToTargetKey));
	AttackRangeKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, AttackRangeKey));
	HasRangedWeaponKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, HasRangedWeaponKey));
	HasLineOfSightKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, HasLineOfSightKey));
}

void UBTService_UpdateEnemyTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIController = OwnerComp.GetAIOwner();
	AEnemyBase* Enemy = AIController ? Cast<AEnemyBase>(AIController->GetPawn()) : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!AIController || !Enemy || !Blackboard)
	{
		return;
	}

	AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName));
	if (!Enemy->IsActorValidAttackTarget(Target) && bUseCachedTargetWhenBlackboardTargetIsInvalid)
	{
		Target = Enemy->GetCachedAttackTarget();
	}

	if (!Enemy->IsActorValidAttackTarget(Target))
	{
		Blackboard->ClearValue(TargetActorKey.SelectedKeyName);
		Blackboard->SetValueAsFloat(DistanceToTargetKey.SelectedKeyName, 0.0f);
		Blackboard->SetValueAsFloat(AttackRangeKey.SelectedKeyName, Enemy->GetAttackStartDistance());
		Blackboard->SetValueAsBool(HasRangedWeaponKey.SelectedKeyName, Enemy->IsUsingRangedWeapon());
		Blackboard->SetValueAsBool(HasLineOfSightKey.SelectedKeyName, false);
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
		Enemy->SetAttackTarget(nullptr);
		return;
	}

	Enemy->SetAttackTarget(Target);

	const float DistanceToTarget = Enemy->GetAttackDistanceToActor(Target);
	const float AttackStartDistance = Enemy->GetAttackStartDistance();
	const bool bHasRangedWeapon = Enemy->IsUsingRangedWeapon();
	const bool bTreatAsLineOfSight = AIController->IsA<ATrainingBotAIController>() || AIController->LineOfSightTo(Target);
	if (Enemy->IsStatusFrozen())
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);

	}
	else
	{
		AIController->SetFocus(Target, EAIFocusPriority::Gameplay);
		if (bHasRangedWeapon)
		{
			const FVector ToTarget = Target->GetActorLocation() - Enemy->GetActorLocation();
			if (!ToTarget.IsNearlyZero())
			{
				FRotator LookAtRotation = ToTarget.Rotation();
				LookAtRotation.Pitch = 0.0f;
				LookAtRotation.Roll = 0.0f;
				AIController->SetControlRotation(LookAtRotation);
				Enemy->SetActorRotation(LookAtRotation);
			}
		}
	}

	Blackboard->SetValueAsObject(TargetActorKey.SelectedKeyName, Target);
	Blackboard->SetValueAsVector(LastKnownTargetLocationKey.SelectedKeyName, Target->GetActorLocation());
	Blackboard->SetValueAsFloat(DistanceToTargetKey.SelectedKeyName, DistanceToTarget);
	Blackboard->SetValueAsFloat(AttackRangeKey.SelectedKeyName, AttackStartDistance);
	Blackboard->SetValueAsBool(HasRangedWeaponKey.SelectedKeyName, bHasRangedWeapon);
	Blackboard->SetValueAsBool(HasLineOfSightKey.SelectedKeyName, bTreatAsLineOfSight);

	if (bAutoAttackWhenInRange && Enemy->IsAttackEnabled() && DistanceToTarget <= AttackStartDistance)
	{
		const double CurrentTime = Enemy->GetWorld() ? Enemy->GetWorld()->GetTimeSeconds() : 0.0;
		if (CurrentTime - LastAutoAttackTime >= AutoAttackInterval)
		{
			LastAutoAttackTime = CurrentTime;
			Enemy->Attack();
		}
	}
}
