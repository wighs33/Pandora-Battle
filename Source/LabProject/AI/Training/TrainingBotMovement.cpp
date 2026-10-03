#include "AI/Training/TrainingBotMovement.h"

#include "AIController.h"
#include "Character/EnemyBase.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"

FVector TrainingBotMovement::GetFlatDirectionFromTarget(const APawn& Pawn, const AActor& TargetActor)
{
	FVector Direction = Pawn.GetActorLocation() - TargetActor.GetActorLocation();
	Direction.Z = 0.0f;
	if (!Direction.Normalize())
	{
		Direction = -TargetActor.GetActorForwardVector();
		Direction.Z = 0.0f;
		Direction.Normalize();
	}
	return Direction;
}

FVector TrainingBotMovement::BuildSideStepDestination(
	const APawn& Pawn,
	const AActor& TargetActor,
	const float MinDistance,
	const float MaxDistance,
	const int32 MinSideStepMultiplier,
	const int32 MaxSideStepMultiplier,
	const float SideStepDistance)
{
	FVector TargetRightVector = TargetActor.GetActorRightVector();
	TargetRightVector.Z = 0.0f;
	TargetRightVector.Normalize();

	const float DistanceFromTarget = FMath::RandRange(
		FMath::Min(MinDistance, MaxDistance),
		FMath::Max(MinDistance, MaxDistance));
	const int32 SideStepMultiplier = FMath::RandRange(
		FMath::Min(MinSideStepMultiplier, MaxSideStepMultiplier),
		FMath::Max(MinSideStepMultiplier, MaxSideStepMultiplier));

	FVector Destination = TargetActor.GetActorLocation()
		+ GetFlatDirectionFromTarget(Pawn, TargetActor) * DistanceFromTarget
		+ TargetRightVector * static_cast<float>(SideStepMultiplier) * SideStepDistance;
	Destination.Z = Pawn.GetActorLocation().Z;
	return Destination;
}

FVector TrainingBotMovement::ProjectToNavigation(const UWorld* World, const FVector& Location, const float ProjectionExtent)
{
	const UNavigationSystemV1* NavigationSystem = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	FNavLocation ProjectedLocation;
	return NavigationSystem
		&& NavigationSystem->ProjectPointToNavigation(Location, ProjectedLocation, FVector(ProjectionExtent))
		? ProjectedLocation.Location
		: Location;
}

void TrainingBotMovement::FaceTarget(
	AAIController& AIController,
	APawn& Pawn,
	AActor& TargetActor,
	const float DeltaSeconds,
	const float InterpSpeed)
{
	FVector ToTarget = TargetActor.GetActorLocation() - Pawn.GetActorLocation();
	ToTarget.Z = 0.0f;
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	if (const AEnemyBase* Enemy = Cast<AEnemyBase>(&Pawn); Enemy && Enemy->IsStatusFrozen())
	{
		AIController.ClearFocus(EAIFocusPriority::Gameplay);
		return;
	}

	FRotator DesiredRotation(0.0f, ToTarget.Rotation().Yaw, 0.0f);
	if (InterpSpeed > 0.0f && DeltaSeconds > 0.0f)
	{
		DesiredRotation = FMath::RInterpConstantTo(Pawn.GetActorRotation(), DesiredRotation, DeltaSeconds, InterpSpeed);
		DesiredRotation.Pitch = 0.0f;
		DesiredRotation.Roll = 0.0f;
	}

	AIController.SetFocus(&TargetActor, EAIFocusPriority::Gameplay);
	AIController.SetControlRotation(DesiredRotation);
	Pawn.SetActorRotation(DesiredRotation);
}

EPathFollowingRequestResult::Type TrainingBotMovement::MoveToLocation(
	AAIController& AIController,
	const FVector& Destination,
	const float AcceptanceRadius,
	const bool bStopOnOverlap)
{
	return AIController.MoveToLocation(
		Destination,
		AcceptanceRadius,
		bStopOnOverlap,
		/*bUsePathfinding=*/true,
		/*bProjectDestinationToNavigation=*/true,
		/*bCanStrafe=*/true,
		/*FilterClass=*/nullptr,
		/*bAllowPartialPath=*/true);
}
