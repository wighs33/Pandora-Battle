#include "AI/BTTask_RangedMoveAndAttack.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "Character/EnemyBase.h"
#include "DrawDebugHelpers.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"

namespace
{
	enum class EPdRangedMoveAndAttackPhase : uint8
	{
		Move
	};

	struct FPdRangedMoveAndAttackMemory
	{
		FVector Destination = FVector::ZeroVector;
		float ElapsedTime = 0.0f;
		float NextAttackRequestTime = 0.0f;
		float NextRetreatMoveRequestTime = 0.0f;
		EPdRangedMoveAndAttackPhase Phase = EPdRangedMoveAndAttackPhase::Move;
		uint8 bRetreating : 1;
	};
}

UBTTask_RangedMoveAndAttack::UBTTask_RangedMoveAndAttack()
{
	NodeName = TEXT("Pd Ranged Move And Attack");
	bNotifyTick = true;
	bNotifyTaskFinished = true;

	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, BlackboardKey), AActor::StaticClass());
}

uint16 UBTTask_RangedMoveAndAttack::GetInstanceMemorySize() const
{
	return sizeof(FPdRangedMoveAndAttackMemory);
}

FString UBTTask_RangedMoveAndAttack::GetStaticDescription() const
{
	return FString::Printf(
		TEXT("%s: move around %s while firing, retreat under %.0fcm\nDistance %.0f-%.0f, side %d..%d * %.0f, accept %.0f"),
		*Super::GetStaticDescription(),
		*BlackboardKey.SelectedKeyName.ToString(),
		RetreatDistance,
		MinDistanceFromTarget,
		MaxDistanceFromTarget,
		MinSideStepMultiplier,
		MaxSideStepMultiplier,
		SideStepDistance,
		AcceptanceRadius);
}

EBTNodeResult::Type UBTTask_RangedMoveAndAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FPdRangedMoveAndAttackMemory* Memory = reinterpret_cast<FPdRangedMoveAndAttackMemory*>(NodeMemory);
	*Memory = FPdRangedMoveAndAttackMemory();

	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	AEnemyBase* Enemy = Cast<AEnemyBase>(Pawn);
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* TargetActor = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(BlackboardKey.SelectedKeyName)) : nullptr;

	if (!AIController || !Enemy || !Enemy->IsActorValidAttackTarget(TargetActor))
	{
		return EBTNodeResult::Failed;
	}

	if (!Enemy->IsUsingRangedWeapon())
	{
		return EBTNodeResult::Failed;
	}

	if (!Enemy->IsStatusFrozen())
	{
		AIController->SetFocus(TargetActor, EAIFocusPriority::Gameplay);
	}
	UpdateFacing(AIController, Enemy, TargetActor, 0.0f);
	return RequestMove(OwnerComp, NodeMemory, Enemy, TargetActor);
}

void UBTTask_RangedMoveAndAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FPdRangedMoveAndAttackMemory* Memory = reinterpret_cast<FPdRangedMoveAndAttackMemory*>(NodeMemory);

	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* TargetActor = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(BlackboardKey.SelectedKeyName)) : nullptr;

	if (!AIController || !Pawn)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	Memory->ElapsedTime += DeltaSeconds;
	UpdateFacing(AIController, Pawn, TargetActor, DeltaSeconds);

	const EBTNodeResult::Type Result = TickMove(OwnerComp, NodeMemory, Pawn, TargetActor);

	if (Result != EBTNodeResult::InProgress)
	{
		FinishLatentTask(OwnerComp, Result);
	}
}

void UBTTask_RangedMoveAndAttack::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	if (AAIController* AIController = OwnerComp.GetAIOwner())
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}

	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

EBTNodeResult::Type UBTTask_RangedMoveAndAttack::RequestMove(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	AEnemyBase* Enemy = Cast<AEnemyBase>(Pawn);
	if (!AIController || !Enemy || !Enemy->IsActorValidAttackTarget(TargetActor))
	{
		return EBTNodeResult::Failed;
	}

	FPdRangedMoveAndAttackMemory* Memory = reinterpret_cast<FPdRangedMoveAndAttackMemory*>(NodeMemory);
	Memory->ElapsedTime = 0.0f;
	Memory->NextAttackRequestTime = 0.0f;
	Memory->NextRetreatMoveRequestTime = FMath::Max(RetreatRepathInterval, 0.05f);
	Memory->Phase = EPdRangedMoveAndAttackPhase::Move;
	Memory->bRetreating = false;

	FVector Destination = FVector::ZeroVector;
	const bool bShouldRetreat = Enemy->GetAttackDistanceToActor(TargetActor) <= RetreatDistance;
	const bool bBuiltDestination = bShouldRetreat
		? BuildRetreatDestination(Pawn, TargetActor, Destination)
		: BuildMoveDestination(Pawn, TargetActor, Destination);
	if (!bBuiltDestination)
	{
		return EBTNodeResult::Failed;
	}

	Memory->Destination = Destination;
	Memory->bRetreating = bShouldRetreat;
	UpdateFacing(AIController, Pawn, TargetActor, 0.0f);
	if (!Enemy->IsStatusFrozen())
	{
		AIController->SetFocus(TargetActor, EAIFocusPriority::Gameplay);
	}

	const EPathFollowingRequestResult::Type MoveResult = AIController->MoveToLocation(
		Destination,
		AcceptanceRadius,
		bStopOnOverlap,
		true,
		true,
		true,
		nullptr,
		true);

	if (bDrawDebug)
	{
		DrawDebugSphere(Pawn->GetWorld(), Destination, 35.0f, 16, FColor::Blue, false, MaxMoveTime > 0.0f ? MaxMoveTime : 1.5f);
		DrawDebugLine(Pawn->GetWorld(), Pawn->GetActorLocation(), Destination, FColor::Blue, false, MaxMoveTime > 0.0f ? MaxMoveTime : 1.5f, 0, 2.0f);
	}

	if (MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		TryAttack(AIController, Enemy, TargetActor);
		return EBTNodeResult::Succeeded;
	}

	return MoveResult == EPathFollowingRequestResult::Failed ? EBTNodeResult::Failed : EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_RangedMoveAndAttack::TickMove(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	AEnemyBase* Enemy = Cast<AEnemyBase>(Pawn);
	if (!AIController || !Enemy || !Enemy->IsActorValidAttackTarget(TargetActor))
	{
		return EBTNodeResult::Failed;
	}

	FPdRangedMoveAndAttackMemory* Memory = reinterpret_cast<FPdRangedMoveAndAttackMemory*>(NodeMemory);
	const float DistanceToTarget = Enemy->GetAttackDistanceToActor(TargetActor);
	if (DistanceToTarget <= RetreatDistance && Memory->ElapsedTime >= Memory->NextRetreatMoveRequestTime)
	{
		RequestRetreatMove(OwnerComp, NodeMemory, Pawn, TargetActor);
		Memory->NextRetreatMoveRequestTime = Memory->ElapsedTime + FMath::Max(RetreatRepathInterval, 0.05f);
	}

	if (bAttackWhileMoving && Memory->ElapsedTime >= Memory->NextAttackRequestTime)
	{
		TryAttack(AIController, Enemy, TargetActor);
		Memory->NextAttackRequestTime = Memory->ElapsedTime + FMath::Max(AttackRequestInterval, 0.05f);
	}

	const float DistanceToDestination = FVector::Dist2D(Pawn->GetActorLocation(), Memory->Destination);
	if (DistanceToDestination <= AcceptanceRadius + FinishDistanceTolerance)
	{
		return EBTNodeResult::Succeeded;
	}

	if (MaxMoveTime > 0.0f && Memory->ElapsedTime >= MaxMoveTime)
	{

		return EBTNodeResult::Succeeded;
	}

	const UPathFollowingComponent* PathFollowingComponent = AIController->GetPathFollowingComponent();
	if (PathFollowingComponent && PathFollowingComponent->GetStatus() == EPathFollowingStatus::Idle)
	{
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::InProgress;
}

bool UBTTask_RangedMoveAndAttack::RequestRetreatMove(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	AEnemyBase* Enemy = Cast<AEnemyBase>(Pawn);
	if (!AIController || !Enemy || !Enemy->IsActorValidAttackTarget(TargetActor))
	{
		return false;
	}

	FPdRangedMoveAndAttackMemory* Memory = reinterpret_cast<FPdRangedMoveAndAttackMemory*>(NodeMemory);
	FVector RetreatDestination = FVector::ZeroVector;
	if (!BuildRetreatDestination(Pawn, TargetActor, RetreatDestination))
	{
		return false;
	}

	Memory->Destination = RetreatDestination;
	Memory->bRetreating = true;
	if (!Enemy->IsStatusFrozen())
	{
		AIController->SetFocus(TargetActor, EAIFocusPriority::Gameplay);
	}

	const EPathFollowingRequestResult::Type MoveResult = AIController->MoveToLocation(
		RetreatDestination,
		AcceptanceRadius,
		bStopOnOverlap,
		true,
		true,
		true,
		nullptr,
		true);

	if (bDrawDebug)
	{
		DrawDebugSphere(Pawn->GetWorld(), RetreatDestination, 45.0f, 16, FColor::Orange, false, RetreatRepathInterval + 0.2f);
		DrawDebugLine(Pawn->GetWorld(), Pawn->GetActorLocation(), RetreatDestination, FColor::Orange, false, RetreatRepathInterval + 0.2f, 0, 2.0f);
	}

	return MoveResult != EPathFollowingRequestResult::Failed;
}

bool UBTTask_RangedMoveAndAttack::TryAttack(AAIController* AIController, APawn* Pawn, AActor* TargetActor) const
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(Pawn);
	if (!AIController || !Enemy || !Enemy->IsActorValidAttackTarget(TargetActor))
	{
		return false;
	}

	UpdateFacing(AIController, Enemy, TargetActor, 0.0f);
	Enemy->SetAttackTarget(TargetActor);
	Enemy->Attack();
	return true;
}

bool UBTTask_RangedMoveAndAttack::BuildMoveDestination(APawn* Pawn, AActor* TargetActor, FVector& OutDestination) const
{
	if (!Pawn || !TargetActor)
	{
		return false;
	}

	const FVector PawnLocation = Pawn->GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();

	FVector DirectionFromTargetToPawn = PawnLocation - TargetLocation;
	DirectionFromTargetToPawn.Z = 0.0f;
	if (!DirectionFromTargetToPawn.Normalize())
	{
		DirectionFromTargetToPawn = -TargetActor->GetActorForwardVector();
		DirectionFromTargetToPawn.Z = 0.0f;
		DirectionFromTargetToPawn.Normalize();
	}

	FVector TargetRightVector = TargetActor->GetActorRightVector();
	TargetRightVector.Z = 0.0f;
	TargetRightVector.Normalize();

	const float DistanceFromTarget = FMath::RandRange(
		FMath::Min(MinDistanceFromTarget, MaxDistanceFromTarget),
		FMath::Max(MinDistanceFromTarget, MaxDistanceFromTarget));
	const int32 SideStepMultiplier = FMath::RandRange(
		FMath::Min(MinSideStepMultiplier, MaxSideStepMultiplier),
		FMath::Max(MinSideStepMultiplier, MaxSideStepMultiplier));

	FVector Destination = TargetLocation
		+ DirectionFromTargetToPawn * DistanceFromTarget
		+ TargetRightVector * static_cast<float>(SideStepMultiplier) * SideStepDistance;
	Destination.Z = PawnLocation.Z;

	if (bProjectDestinationToNavigation)
	{
		if (UWorld* World = Pawn->GetWorld())
		{
			if (UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
			{
				FNavLocation ProjectedLocation;
				const FVector ProjectionExtent(NavigationProjectionExtent, NavigationProjectionExtent, NavigationProjectionExtent);
				if (NavigationSystem->ProjectPointToNavigation(Destination, ProjectedLocation, ProjectionExtent))
				{
					Destination = ProjectedLocation.Location;
				}
			}
		}
	}

	OutDestination = Destination;
	return true;
}

bool UBTTask_RangedMoveAndAttack::BuildRetreatDestination(APawn* Pawn, AActor* TargetActor, FVector& OutDestination) const
{
	if (!Pawn || !TargetActor)
	{
		return false;
	}

	const FVector PawnLocation = Pawn->GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();

	FVector AwayFromTarget = PawnLocation - TargetLocation;
	AwayFromTarget.Z = 0.0f;
	if (!AwayFromTarget.Normalize())
	{
		AwayFromTarget = -TargetActor->GetActorForwardVector();
		AwayFromTarget.Z = 0.0f;
		AwayFromTarget.Normalize();
	}

	FVector Destination = PawnLocation + AwayFromTarget * RetreatMoveDistance;
	Destination.Z = PawnLocation.Z;

	if (bProjectDestinationToNavigation)
	{
		if (UWorld* World = Pawn->GetWorld())
		{
			if (UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
			{
				FNavLocation ProjectedLocation;
				const FVector ProjectionExtent(NavigationProjectionExtent, NavigationProjectionExtent, NavigationProjectionExtent);
				if (NavigationSystem->ProjectPointToNavigation(Destination, ProjectedLocation, ProjectionExtent))
				{
					Destination = ProjectedLocation.Location;
				}
			}
		}
	}

	OutDestination = Destination;
	return true;
}

void UBTTask_RangedMoveAndAttack::UpdateFacing(AAIController* AIController, APawn* Pawn, AActor* TargetActor, float DeltaSeconds) const
{
	if (!bFaceTargetWhileMoving || !AIController || !Pawn || !TargetActor)
	{
		return;
	}

	FVector ToTarget = TargetActor->GetActorLocation() - Pawn->GetActorLocation();
	ToTarget.Z = 0.0f;
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	if (const AEnemyBase* Enemy = Cast<AEnemyBase>(Pawn); Enemy && Enemy->IsStatusFrozen())
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);

		return;
	}

	FRotator DesiredRotation = ToTarget.Rotation();
	DesiredRotation.Pitch = 0.0f;
	DesiredRotation.Roll = 0.0f;

	if (FaceTargetRotationInterpSpeed > 0.0f && DeltaSeconds > 0.0f)
	{
		DesiredRotation = FMath::RInterpConstantTo(Pawn->GetActorRotation(), DesiredRotation, DeltaSeconds, FaceTargetRotationInterpSpeed);
		DesiredRotation.Pitch = 0.0f;
		DesiredRotation.Roll = 0.0f;
	}

	AIController->SetFocus(TargetActor, EAIFocusPriority::Gameplay);
	AIController->SetControlRotation(DesiredRotation);
	Pawn->SetActorRotation(DesiredRotation);
}
