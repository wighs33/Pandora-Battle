#include "Room/PortalTraversal.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

bool FPortalTraversalTracker::UpdateCrossing(const AActor* Actor, const FVector& Point, const FVector& PlaneLocation,
	const FVector& PlaneNormal, const double Now, const float Cooldown, const float CrossingTolerance)
{
	if (!Actor)
	{
		return false;
	}

	FState& State = States.FindOrAdd(TObjectKey<AActor>(Actor));
	const float CurrentDistance = FVector::DotProduct(Point - PlaneLocation, PlaneNormal);
	const bool bIsInFront = CurrentDistance >= 0.0f;

	if (!State.bInitialized)
	{
		State.LastPosition = Point;
		State.bLastInFront = bIsInFront;
		State.bInitialized = true;
		return false;
	}

	const bool bCoolingDown = Now - State.LastTeleportTime < Cooldown;
	const float LastDistance = FVector::DotProduct(State.LastPosition - PlaneLocation, PlaneNormal);

	FVector PlaneIntersection = FVector::ZeroVector;
	const bool bSegmentHitsPlane = FMath::SegmentPlaneIntersection(State.LastPosition, Point, FPlane(PlaneLocation, PlaneNormal), PlaneIntersection);
	const bool bCrossedPlane = State.bLastInFront && !bIsInFront && bSegmentHitsPlane && LastDistance > CrossingTolerance && CurrentDistance <= CrossingTolerance;

	State.LastPosition = Point;
	State.bLastInFront = bIsInFront;

	return bCrossedPlane && !bCoolingDown;
}

void FPortalTraversalTracker::Prime(const AActor* Actor, const FVector& Location, const FVector& PlaneLocation,
	const FVector& PlaneForward, const double Now)
{
	if (!Actor)
	{
		return;
	}

	FState& State = States.FindOrAdd(TObjectKey<AActor>(Actor));
	State.LastPosition = Location;
	State.bLastInFront = FVector::DotProduct(Location - PlaneLocation, PlaneForward) >= 0.0f;
	State.bInitialized = true;
	State.LastTeleportTime = Now;
}

void FPortalTraversalTracker::Remove(const AActor* Actor)
{
	States.Remove(TObjectKey<AActor>(Actor));
}

void FPortalTraversalTracker::Reset()
{
	States.Reset();
}

void FPortalOverlapList::Seed(const UPrimitiveComponent* OverlapComponent, const FCandidateFilter IsCandidate)
{
	Actors.Reset();
	if (!OverlapComponent)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	OverlapComponent->GetOverlappingActors(OverlappingActors);
	for (AActor* Actor : OverlappingActors)
	{
		Track(Actor, IsCandidate);
	}
}

bool FPortalOverlapList::Track(AActor* Actor, const FCandidateFilter IsCandidate)
{
	if (!IsCandidate(Actor))
	{
		return false;
	}

	for (const TWeakObjectPtr<AActor>& ExistingActor : Actors)
	{
		if (ExistingActor.Get() == Actor)
		{
			return true;
		}
	}

	Actors.Add(Actor);
	return true;
}

void FPortalOverlapList::Untrack(AActor* Actor, const UPrimitiveComponent* OverlapComponent)
{
	if (!Actor)
	{
		Actors.RemoveAllSwap([](const TWeakObjectPtr<AActor>& ExistingActor)
			{
				return !ExistingActor.IsValid();
			});
		return;
	}
	if (OverlapComponent && OverlapComponent->IsOverlappingActor(Actor))
	{
		return;
	}

	Actors.RemoveAllSwap([Actor](const TWeakObjectPtr<AActor>& ExistingActor)
		{
			return !ExistingActor.IsValid() || ExistingActor.Get() == Actor;
		});
}

bool FPortalOverlapList::HasAny(const UPrimitiveComponent* OverlapComponent, const FCandidateFilter IsCandidate)
{
	for (int32 ActorIndex = Actors.Num() - 1; ActorIndex >= 0; --ActorIndex)
	{
		AActor* Actor = Actors[ActorIndex].Get();
		if (!IsCandidate(Actor) || !OverlapComponent || !OverlapComponent->IsOverlappingActor(Actor))
		{
			Actors.RemoveAtSwap(ActorIndex);
			continue;
		}

		return true;
	}

	return false;
}

void FPortalOverlapList::Collect(const UPrimitiveComponent* OverlapComponent, const FCandidateFilter IsCandidate,
	TArray<AActor*>& OutActors, const TFunctionRef<void(AActor*)> OnDropped)
{
	OutActors.Reset();
	for (int32 ActorIndex = Actors.Num() - 1; ActorIndex >= 0; --ActorIndex)
	{
		AActor* Actor = Actors[ActorIndex].Get();
		if (IsCandidate(Actor) && OverlapComponent && OverlapComponent->IsOverlappingActor(Actor))
		{
			OutActors.Add(Actor);
			continue;
		}

		Actors.RemoveAtSwap(ActorIndex);
		OnDropped(Actor);
	}
}

void FPortalOverlapList::Reset()
{
	Actors.Reset();
}
