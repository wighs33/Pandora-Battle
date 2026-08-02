#include "AI/PetAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"
#include "Pet/PetCharacter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PetAIController)

APetAIController::APetAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void APetAIController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDirectFollowFallbackActive)
	{
		UpdateDirectFollowFallback(DeltaSeconds);
	}
}

void APetAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (APetCharacter* PetCharacter = Cast<APetCharacter>(InPawn))
	{
		if (!PetCharacter->GetFollowTargetActor())
		{
			PetCharacter->SetFollowTargetActor(PetCharacter->GetInstigator());
		}
	}

	bBehaviorTreeRunning = false;
	if (BehaviorTreeAsset)
	{
		UBlackboardComponent* LocalBlackboard = nullptr;
		if (BehaviorTreeAsset->BlackboardAsset
			&& UseBlackboard(BehaviorTreeAsset->BlackboardAsset, LocalBlackboard))
		{
			InitializeBlackboardValues(InPawn);
			bBehaviorTreeRunning = RunBehaviorTree(BehaviorTreeAsset);
		}
	}

	RefreshFollowTarget();
	StartFollowTargetRefreshTimer();
}

void APetAIController::OnUnPossess()
{
	StopFollowTargetRefreshTimer();
	bBehaviorTreeRunning = false;
	bDirectFollowFallbackActive = false;
	SetActorTickEnabled(false);
	StopMovement();

	if (UBlackboardComponent* LocalBlackboard = GetBlackboardComponent())
	{
		LocalBlackboard->ClearValue(FollowTargetActorKeyName);
	}

	Super::OnUnPossess();
}

void APetAIController::RefreshFollowTarget()
{
	AActor* FollowTarget = ResolveFollowTarget();
	UBlackboardComponent* LocalBlackboard = GetBlackboardComponent();
	if (LocalBlackboard)
	{
		if (FollowTarget)
		{
			LocalBlackboard->SetValueAsObject(FollowTargetActorKeyName, FollowTarget);
		}
		else
		{
			LocalBlackboard->ClearValue(FollowTargetActorKeyName);
		}
	}

	if (!bBehaviorTreeRunning && FollowTarget)
	{
		EPathFollowingRequestResult::Type MoveResult = MoveToActor(
			FollowTarget,
			DirectMoveAcceptanceRadius,
			true,
			bUsePathfindingForFollow,
			true);
		if (MoveResult == EPathFollowingRequestResult::Failed && bUsePathfindingForFollow)
		{
			MoveResult = MoveToActor(
				FollowTarget,
				DirectMoveAcceptanceRadius,
				true,
				false,
				true);
		}

		bDirectFollowFallbackActive = MoveResult == EPathFollowingRequestResult::Failed;
		SetActorTickEnabled(bDirectFollowFallbackActive);
	}
	else
	{
		bDirectFollowFallbackActive = false;
		SetActorTickEnabled(false);
	}
}

void APetAIController::InitializeBlackboardValues(APawn* InPawn)
{
	UBlackboardComponent* LocalBlackboard = GetBlackboardComponent();
	if (!LocalBlackboard)
	{
		return;
	}

	if (AActor* FollowTarget = ResolveFollowTarget())
	{
		LocalBlackboard->SetValueAsObject(FollowTargetActorKeyName, FollowTarget);
	}
	else
	{
		LocalBlackboard->ClearValue(FollowTargetActorKeyName);
	}
}

AActor* APetAIController::ResolveFollowTarget() const
{
	const APetCharacter* PetCharacter = Cast<APetCharacter>(GetPawn());
	if (!PetCharacter)
	{
		return nullptr;
	}

	if (AActor* FollowTarget = PetCharacter->GetFollowTargetActor())
	{
		return FollowTarget;
	}

	return PetCharacter->GetInstigator();
}

void APetAIController::UpdateDirectFollowFallback(const float DeltaSeconds)
{
	APawn* ControlledPawn = GetPawn();
	AActor* FollowTarget = ResolveFollowTarget();
	if (!ControlledPawn || !FollowTarget || DeltaSeconds <= 0.0f)
	{
		return;
	}

	const FVector FollowDestination =
		FollowTarget->GetActorLocation()
		+ FollowTarget->GetActorRotation().RotateVector(DirectFollowOffset);
	const FVector ToDestination = FollowDestination - ControlledPawn->GetActorLocation();
	const float DistanceToDestination = ToDestination.Size2D();
	if (DirectFollowTeleportDistance > 0.0f
		&& ToDestination.Size() > DirectFollowTeleportDistance)
	{
		ControlledPawn->SetActorLocation(
			FollowDestination,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		return;
	}

	if (DistanceToDestination > DirectMoveAcceptanceRadius)
	{
		ControlledPawn->AddMovementInput(ToDestination.GetSafeNormal2D(), 1.0f, true);
	}
}

void APetAIController::StartFollowTargetRefreshTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(FollowTargetRefreshTimerHandle);
	World->GetTimerManager().SetTimer(
		FollowTargetRefreshTimerHandle,
		this,
		&ThisClass::RefreshFollowTarget,
		FMath::Max(FollowTargetRefreshInterval, 0.05f),
		true);
}

void APetAIController::StopFollowTargetRefreshTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FollowTargetRefreshTimerHandle);
	}

	FollowTargetRefreshTimerHandle.Invalidate();
}
