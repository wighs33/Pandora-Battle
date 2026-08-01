#include "AbilitySystem/SkillGroundProjection.h"

#include "AbilitySystem/EffectActors/EffectAreaBase.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

namespace
{
	constexpr float SkillGroundMinNormalZ = 0.35f;
	constexpr float SkillGroundMinimumTraceDepth = 100.0f;

	bool IsActorOwnedBy(const AActor* Actor, const AActor* OwnerCandidate)
	{
		if (!Actor || !OwnerCandidate)
		{
			return false;
		}

		for (const AActor* CurrentActor = Actor; CurrentActor; CurrentActor = CurrentActor->GetOwner())
		{
			if (CurrentActor == OwnerCandidate)
			{
				return true;
			}
		}

		return false;
	}

	bool IsIgnoredGroundActor(const AActor* Actor)
	{
		return Actor
			&& (Actor->IsA<APawn>()
				|| Actor->IsA<AEffectAreaBase>());
	}

	bool IsRelatedToIgnoredActor(const AActor* Actor, const TArray<AActor*>& ActorsToIgnore)
	{
		if (!Actor)
		{
			return false;
		}

		for (const AActor* IgnoredActor : ActorsToIgnore)
		{
			if (!IsValid(IgnoredActor))
			{
				continue;
			}

			if (Actor == IgnoredActor
				|| IsActorOwnedBy(Actor, IgnoredActor)
				|| IsActorOwnedBy(IgnoredActor, Actor))
			{
				return true;
			}

			if (const APawn* IgnoredPawn = Cast<APawn>(IgnoredActor))
			{
				if (Actor->GetInstigator() == IgnoredPawn)
				{
					return true;
				}
			}
		}

		return false;
	}

	bool IsValidGroundHit(const FHitResult& Hit, const TArray<AActor*>& ActorsToIgnore)
	{
		const AActor* HitActor = Hit.GetActor();
		return Hit.bBlockingHit
			&& Hit.ImpactNormal.Z >= SkillGroundMinNormalZ
			&& !IsIgnoredGroundActor(HitActor)
			&& !IsRelatedToIgnoredActor(HitActor, ActorsToIgnore);
	}

	bool TryResolveGroundHit(
		const TArray<FHitResult>& Hits,
		const TArray<AActor*>& ActorsToIgnore,
		PdSkillGroundProjection::FGroundProjectionResult& OutResult)
	{
		for (const FHitResult& Hit : Hits)
		{
			if (!IsValidGroundHit(Hit, ActorsToIgnore))
			{
				continue;
			}

			OutResult.Location = Hit.ImpactPoint;
			OutResult.Normal = Hit.ImpactNormal.IsNearlyZero() ? FVector::UpVector : Hit.ImpactNormal.GetSafeNormal();
			OutResult.HitActor = Hit.GetActor();
			return true;
		}

		return false;
	}

	bool TryTraceWorldSurfaceToGround(
		UWorld* World,
		const FVector& TraceStart,
		const FVector& TraceEnd,
		const FCollisionQueryParams& QueryParams,
		const TArray<AActor*>& ActorsToIgnore,
		PdSkillGroundProjection::FGroundProjectionResult& OutResult)
	{
		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

		TArray<FHitResult> Hits;
		return World->LineTraceMultiByObjectType(Hits, TraceStart, TraceEnd, ObjectParams, QueryParams)
			&& TryResolveGroundHit(Hits, ActorsToIgnore, OutResult);
	}
}

void PdSkillGroundProjection::AddIgnoredActorAndAttachments(TArray<AActor*>& ActorsToIgnore, AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}

	ActorsToIgnore.AddUnique(Actor);

	TArray<AActor*> AttachedActors;
	Actor->GetAttachedActors(AttachedActors, true, true);
	for (AActor* AttachedActor : AttachedActors)
	{
		if (IsValid(AttachedActor))
		{
			ActorsToIgnore.AddUnique(AttachedActor);
		}
	}
}

bool PdSkillGroundProjection::TryProjectToGround(
	UWorld* World,
	const FVector& SourceLocation,
	const TEnumAsByte<ETraceTypeQuery> TraceType,
	const double TraceStartHeight,
	const double TraceDepth,
	const TArray<AActor*>& ActorsToIgnore,
	FGroundProjectionResult& OutResult)
{
	if (!World)
	{
		return false;
	}

	const FVector TraceStart = SourceLocation
		+ FVector::UpVector * static_cast<float>(FMath::Max(TraceStartHeight, 0.0));
	const FVector TraceEnd = SourceLocation
		- FVector::UpVector * static_cast<float>(FMath::Max(TraceDepth, static_cast<double>(SkillGroundMinimumTraceDepth)));

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SkillGroundProjection), false);
	for (AActor* ActorToIgnore : ActorsToIgnore)
	{
		if (IsValid(ActorToIgnore))
		{
			QueryParams.AddIgnoredActor(ActorToIgnore);
		}
	}

	if (TryTraceWorldSurfaceToGround(
		World,
		TraceStart,
		TraceEnd,
		QueryParams,
		ActorsToIgnore,
		OutResult))
	{
		return true;
	}

	const ECollisionChannel TraceChannel = UEngineTypes::ConvertToCollisionChannel(TraceType);
	if (TraceChannel == ECC_MAX)
	{
		return false;
	}

	TArray<FHitResult> ChannelHits;
	return World->LineTraceMultiByChannel(ChannelHits, TraceStart, TraceEnd, TraceChannel, QueryParams)
		&& TryResolveGroundHit(ChannelHits, ActorsToIgnore, OutResult);
}
