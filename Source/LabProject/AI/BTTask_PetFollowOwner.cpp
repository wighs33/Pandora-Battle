#include "AI/BTTask_PetFollowOwner.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BTTask_PetFollowOwner)

UBTTask_PetFollowOwner::UBTTask_PetFollowOwner()
{
	NodeName = TEXT("Pet Follow Owner");
	bNotifyTick = true;
	bNotifyTaskFinished = true;

	BlackboardKey.SelectedKeyName = TEXT("FollowTargetActor");
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_PetFollowOwner, BlackboardKey), AActor::StaticClass());
}

EBTNodeResult::Type UBTTask_PetFollowOwner::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FPdPetFollowOwnerTaskMemory& TaskMemory = *reinterpret_cast<FPdPetFollowOwnerTaskMemory*>(NodeMemory);
	TaskMemory.TimeSinceLastMoveRequest = RepathInterval;

	return UpdateFollowMove(OwnerComp, TaskMemory, 0.0f)
		? EBTNodeResult::InProgress
		: EBTNodeResult::Failed;
}

void UBTTask_PetFollowOwner::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, const float DeltaSeconds)
{
	FPdPetFollowOwnerTaskMemory& TaskMemory = *reinterpret_cast<FPdPetFollowOwnerTaskMemory*>(NodeMemory);
	if (!UpdateFollowMove(OwnerComp, TaskMemory, DeltaSeconds))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
	}
}

void UBTTask_PetFollowOwner::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, const EBTNodeResult::Type TaskResult)
{
	static_cast<void>(NodeMemory);
	static_cast<void>(TaskResult);

	if (AAIController* AIController = OwnerComp.GetAIOwner())
	{
		AIController->StopMovement();
	}
}

uint16 UBTTask_PetFollowOwner::GetInstanceMemorySize() const
{
	return sizeof(FPdPetFollowOwnerTaskMemory);
}

bool UBTTask_PetFollowOwner::UpdateFollowMove(
	UBehaviorTreeComponent& OwnerComp,
	FPdPetFollowOwnerTaskMemory& TaskMemory,
	const float DeltaSeconds) const
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	UBlackboardComponent* BlackboardComponent = OwnerComp.GetBlackboardComponent();
	AActor* FollowTarget = BlackboardComponent
		? Cast<AActor>(BlackboardComponent->GetValueAsObject(BlackboardKey.SelectedKeyName))
		: nullptr;
	if (!AIController || !Pawn || !FollowTarget)
	{
		return false;
	}

	const FVector FollowDestination = ResolveFollowDestination(Pawn, FollowTarget);
	const float DistanceToDestination = FVector::Dist2D(Pawn->GetActorLocation(), FollowDestination);
	if (TeleportDistance > 0.0f && FVector::Dist(Pawn->GetActorLocation(), FollowDestination) > TeleportDistance)
	{
		AIController->StopMovement();
		Pawn->SetActorLocation(FollowDestination, false, nullptr, ETeleportType::TeleportPhysics);
		TaskMemory.TimeSinceLastMoveRequest = 0.0f;
		return true;
	}

	if (DistanceToDestination <= AcceptanceRadius)
	{
		if (bStopMovementInsideAcceptanceRadius)
		{
			AIController->StopMovement();
		}
		TaskMemory.TimeSinceLastMoveRequest += DeltaSeconds;
		return true;
	}

	TaskMemory.TimeSinceLastMoveRequest += DeltaSeconds;
	if (TaskMemory.TimeSinceLastMoveRequest < RepathInterval)
	{
		return true;
	}

	const EPathFollowingRequestResult::Type MoveResult = AIController->MoveToLocation(
		FollowDestination,
		AcceptanceRadius,
		true,
		true,
		false,
		true,
		nullptr,
		true);
	TaskMemory.TimeSinceLastMoveRequest = 0.0f;

	return MoveResult != EPathFollowingRequestResult::Failed;
}

FVector UBTTask_PetFollowOwner::ResolveFollowDestination(const APawn* Pawn, const AActor* FollowTarget) const
{
	static_cast<void>(Pawn);

	if (!FollowTarget)
	{
		return FVector::ZeroVector;
	}

	const FVector ResolvedOffset = bUseTargetRotationForOffset
		? FollowTarget->GetActorRotation().RotateVector(FollowOffset)
		: FollowOffset;
	return FollowTarget->GetActorLocation() + ResolvedOffset;
}
