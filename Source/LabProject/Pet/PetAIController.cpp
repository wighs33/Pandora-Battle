#include "Pet/PetAIController.h"

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

	RefreshFollowTarget();
	StartFollowTargetRefreshTimer();
}

void APetAIController::OnUnPossess()
{
	StopFollowTargetRefreshTimer();
	bDirectFollowFallbackActive = false;
	SetActorTickEnabled(false);
	StopMovement();

	Super::OnUnPossess();
}

void APetAIController::RefreshFollowTarget()
{
	const APetCharacter* Pet = Cast<APetCharacter>(GetPawn());
	AActor* FollowTarget = Pet ? Pet->GetFollowTargetActor() : nullptr;
	if (FollowTarget)
	{
		EPathFollowingRequestResult::Type MoveResult = MoveToActor(FollowTarget, DirectMoveAcceptanceRadius, true,
			bUsePathfindingForFollow, true);
		if (MoveResult == EPathFollowingRequestResult::Failed && bUsePathfindingForFollow)
		{
			MoveResult = MoveToActor(FollowTarget, DirectMoveAcceptanceRadius, true, false, true);
		}

		bDirectFollowFallbackActive = MoveResult == EPathFollowingRequestResult::Failed;
		SetActorTickEnabled(bDirectFollowFallbackActive);
	}
	else
	{
		bDirectFollowFallbackActive = false;
		SetActorTickEnabled(false);
		StopMovement();
	}
}

void APetAIController::UpdateDirectFollowFallback(const float DeltaSeconds)
{
	APawn* ControlledPawn = GetPawn();
	const APetCharacter* Pet = Cast<APetCharacter>(GetPawn());
	AActor* FollowTarget = Pet ? Pet->GetFollowTargetActor() : nullptr;
	if (!ControlledPawn || !FollowTarget || DeltaSeconds <= 0.0f)
	{
		return;
	}

	const FVector FollowDestination = FollowTarget->GetActorLocation()
		+ FollowTarget->GetActorRotation().RotateVector(DirectFollowOffset);
	const FVector ToDestination = FollowDestination - ControlledPawn->GetActorLocation();
	const float DistanceToDestination = ToDestination.Size2D();
	if (DirectFollowTeleportDistance > 0.0f && ToDestination.Size() > DirectFollowTeleportDistance)
	{
		ControlledPawn->SetActorLocation(FollowDestination, false, nullptr, ETeleportType::TeleportPhysics);
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
	World->GetTimerManager().SetTimer(FollowTargetRefreshTimerHandle, this, &ThisClass::RefreshFollowTarget,
		FMath::Max(FollowTargetRefreshInterval, 0.05f), true);
}

void APetAIController::StopFollowTargetRefreshTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FollowTargetRefreshTimerHandle);
	}

	FollowTargetRefreshTimerHandle.Invalidate();
}
