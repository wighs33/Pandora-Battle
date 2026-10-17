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
	const TOptional<FVector> LastPosition = State.LastPosition;
	State.LastPosition = Point;
	if (!LastPosition)
	{
		return false;
	}

	// 직전에는 평면 앞쪽 허용 거리 밖에, 지금은 평면 뒤에 있어야 넘은 것이다.
	// 평면 바로 앞에서 서성이거나 뒤에서 다가오는 움직임은 넘은 것으로 보지 않는다.
	const double LastDistance = FVector::DotProduct(*LastPosition - PlaneLocation, PlaneNormal);
	const double CurrentDistance = FVector::DotProduct(Point - PlaneLocation, PlaneNormal);
	const bool bCrossedPlane = LastDistance > CrossingTolerance && CurrentDistance < 0.0;
	const bool bCoolingDown = State.LastTeleportTime && Now - *State.LastTeleportTime < Cooldown;
	return bCrossedPlane && !bCoolingDown;
}

void FPortalTraversalTracker::Prime(const AActor* Actor, const FVector& Location, const double Now)
{
	if (!Actor)
	{
		return;
	}

	FState& State = States.FindOrAdd(TObjectKey<AActor>(Actor));
	State.LastPosition = Location;
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
