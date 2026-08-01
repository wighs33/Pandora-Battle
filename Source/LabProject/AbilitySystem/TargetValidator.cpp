#include "AbilitySystem/TargetValidator.h"

#include "AbilitySystem/SkillGroundProjection.h"
#include "CollisionQueryParams.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

namespace
{
	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X)
			&& FMath::IsFinite(Value.Y)
			&& FMath::IsFinite(Value.Z);
	}

	bool AreFiniteNonNegative(const double First, const double Second)
	{
		return FMath::IsFinite(First)
			&& FMath::IsFinite(Second)
			&& First >= 0.0
			&& Second >= 0.0;
	}

	bool IsWithinRange(
		const FVector& SourceLocation,
		const FVector& TargetLocation,
		const double MaxRange,
		const double RangeTolerance)
	{
		const double AllowedRange = MaxRange + RangeTolerance;
		return AllowedRange > 0.0
			&& FVector::DistSquared(SourceLocation, TargetLocation) <= FMath::Square(AllowedRange);
	}

	bool IsBlockingCollisionProfile(const FName TraceProfileName)
	{
		if (TraceProfileName.IsNone() || TraceProfileName == UCollisionProfile::NoCollision_ProfileName)
		{
			return false;
		}

		ECollisionChannel ObjectType = ECC_WorldStatic;
		FCollisionResponseParams ResponseParams;
		if (!UCollisionProfile::GetChannelAndResponseParams(TraceProfileName, ObjectType, ResponseParams))
		{
			return false;
		}

		for (int32 ChannelIndex = 0; ChannelIndex < ECC_MAX; ++ChannelIndex)
		{
			if (ResponseParams.CollisionResponse.GetResponse(
				static_cast<ECollisionChannel>(ChannelIndex)) == ECR_Block)
			{
				return true;
			}
		}

		return false;
	}

	bool HasServerLineOfSight(
		UWorld* World,
		const FVector& SourceLocation,
		const FVector& GroundLocation,
		const FVector& GroundNormal,
		const FName TraceProfileName,
		const double SurfaceOffset,
		const double SurfaceTolerance,
		const int32 MaxIgnoredPawnCount,
		const TArray<AActor*>& ActorsToIgnore)
	{
		if (!World
			|| !IsBlockingCollisionProfile(TraceProfileName)
			|| !IsFiniteVector(SourceLocation)
			|| !IsFiniteVector(GroundLocation)
			|| !IsFiniteVector(GroundNormal))
		{
			return false;
		}

		const FVector SafeGroundNormal = GroundNormal.IsNearlyZero()
			? FVector::UpVector
			: GroundNormal.GetSafeNormal();
		const FVector TraceEnd = GroundLocation + SafeGroundNormal * FMath::Max(SurfaceOffset, 0.0);
		const FVector TraceDirection = (TraceEnd - SourceLocation).GetSafeNormal();
		if (TraceDirection.IsNearlyZero())
		{
			return true;
		}

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ServerTargetLineOfSight), false);
		for (AActor* ActorToIgnore : ActorsToIgnore)
		{
			if (IsValid(ActorToIgnore))
			{
				QueryParams.AddIgnoredActor(ActorToIgnore);
			}
		}

		FVector TraceStart = SourceLocation;
		const double SurfaceToleranceSquared = FMath::Square(FMath::Max(SurfaceTolerance, 0.0));
		const int32 MaxTraceCount = FMath::Max(MaxIgnoredPawnCount, 0) + 1;
		for (int32 TraceIndex = 0; TraceIndex < MaxTraceCount; ++TraceIndex)
		{
			FHitResult BlockingHit;
			if (!World->LineTraceSingleByProfile(
				BlockingHit,
				TraceStart,
				TraceEnd,
				TraceProfileName,
				QueryParams))
			{
				return true;
			}

			if (FVector::DistSquared(BlockingHit.ImpactPoint, TraceEnd) <= SurfaceToleranceSquared)
			{
				return true;
			}

			APawn* HitPawn = Cast<APawn>(BlockingHit.GetActor());
			if (!HitPawn || TraceIndex >= MaxIgnoredPawnCount)
			{
				return false;
			}

			QueryParams.AddIgnoredActor(HitPawn);
			TraceStart = BlockingHit.ImpactPoint + TraceDirection;
		}

		return false;
	}
}

bool PdTargetValidator::TryResolveTargetDataLocation(
	const FHitResult& ClientHitResult,
	const FVector& TargetDataEndPoint,
	FVector& OutRequestedLocation)
{
	if (ClientHitResult.bBlockingHit && IsFiniteVector(ClientHitResult.ImpactPoint))
	{
		OutRequestedLocation = ClientHitResult.ImpactPoint;
		return true;
	}

	if (!ClientHitResult.Location.IsNearlyZero() && IsFiniteVector(ClientHitResult.Location))
	{
		OutRequestedLocation = ClientHitResult.Location;
		return true;
	}

	if (IsFiniteVector(TargetDataEndPoint))
	{
		OutRequestedLocation = TargetDataEndPoint;
		return true;
	}

	return false;
}

bool PdTargetValidator::ValidateClientTraceRequest(
	const AActor* AuthoritySourceActor,
	const FHitResult& ClientHitResult,
	const FTraceRequestValidationParams& Params,
	FValidatedTraceView& OutValidatedView)
{
	if (!IsValid(AuthoritySourceActor)
		|| !AuthoritySourceActor->HasAuthority()
		|| !IsFiniteVector(ClientHitResult.TraceStart)
		|| !IsFiniteVector(ClientHitResult.TraceEnd)
		|| !AreFiniteNonNegative(Params.TraceStartOffset, Params.TraceLengthTolerance)
		|| !FMath::IsFinite(Params.MaxTraceDistance)
		|| Params.MaxTraceDistance <= 0.0
		|| !FMath::IsFinite(Params.MaxViewDistanceFromSource)
		|| Params.MaxViewDistanceFromSource < 0.0)
	{
		return false;
	}

	FVector ViewDirection = ClientHitResult.TraceEnd - ClientHitResult.TraceStart;
	const double ClientTraceLength = ViewDirection.Size();
	if (!FMath::IsFinite(ClientTraceLength)
		|| ClientTraceLength <= UE_SMALL_NUMBER
		|| ClientTraceLength > Params.MaxTraceDistance + Params.TraceLengthTolerance)
	{
		return false;
	}

	ViewDirection /= ClientTraceLength;
	const FVector ViewLocation = ClientHitResult.TraceStart - ViewDirection * Params.TraceStartOffset;
	if (!IsFiniteVector(ViewLocation)
		|| FVector::DistSquared(ViewLocation, AuthoritySourceActor->GetActorLocation())
			> FMath::Square(Params.MaxViewDistanceFromSource))
	{
		return false;
	}

	OutValidatedView.ViewLocation = ViewLocation;
	OutValidatedView.ViewDirection = ViewDirection;
	return true;
}

bool PdTargetValidator::ValidateGroundTarget(
	UWorld* World,
	AActor* AuthoritySourceActor,
	const FVector& AuthoritySourceLocation,
	const FVector& RequestedLocation,
	const FGroundTargetValidationParams& Params,
	FValidatedGroundTarget& OutValidatedTarget)
{
	if (!World
		|| !IsValid(AuthoritySourceActor)
		|| !AuthoritySourceActor->HasAuthority()
		|| AuthoritySourceActor->GetWorld() != World
		|| !IsFiniteVector(AuthoritySourceLocation)
		|| !IsFiniteVector(RequestedLocation)
		|| !FMath::IsFinite(Params.MaxRange)
		|| Params.MaxRange <= 0.0
		|| !AreFiniteNonNegative(Params.RangeTolerance, Params.GroundTraceStartHeight)
		|| !FMath::IsFinite(Params.GroundTraceDepth)
		|| Params.GroundTraceDepth <= 0.0
		|| !AreFiniteNonNegative(Params.LineOfSightSurfaceOffset, Params.LineOfSightSurfaceTolerance)
		|| !IsWithinRange(AuthoritySourceLocation, RequestedLocation, Params.MaxRange, Params.RangeTolerance))
	{
		return false;
	}

	TArray<AActor*> ActorsToIgnore;
	PdSkillGroundProjection::AddIgnoredActorAndAttachments(ActorsToIgnore, AuthoritySourceActor);

	PdSkillGroundProjection::FGroundProjectionResult GroundResult;
	if (!PdSkillGroundProjection::TryProjectToGround(
		World,
		RequestedLocation,
		Params.GroundTraceType,
		Params.GroundTraceStartHeight,
		Params.GroundTraceDepth,
		ActorsToIgnore,
		GroundResult)
		|| !IsFiniteVector(GroundResult.Location)
		|| !IsFiniteVector(GroundResult.Normal)
		|| !IsWithinRange(AuthoritySourceLocation, GroundResult.Location, Params.MaxRange, Params.RangeTolerance)
		|| !HasServerLineOfSight(
			World,
			AuthoritySourceLocation,
			GroundResult.Location,
			GroundResult.Normal,
			Params.LineOfSightProfileName,
			Params.LineOfSightSurfaceOffset,
			Params.LineOfSightSurfaceTolerance,
			Params.MaxIgnoredPawnCount,
			ActorsToIgnore))
	{
		return false;
	}

	OutValidatedTarget.Location = GroundResult.Location;
	OutValidatedTarget.Normal = GroundResult.Normal;
	OutValidatedTarget.GroundActor = GroundResult.HitActor;
	return true;
}

bool PdTargetValidator::ValidatePointTarget(
	UWorld* World,
	AActor* AuthoritySourceActor,
	const FVector& AuthorityRangeOrigin,
	const FVector& AuthorityTraceStart,
	const FVector& RequestedLocation,
	const FPointTargetValidationParams& Params,
	FValidatedPointTarget& OutValidatedTarget)
{
	if (!World
		|| !IsValid(AuthoritySourceActor)
		|| !AuthoritySourceActor->HasAuthority()
		|| AuthoritySourceActor->GetWorld() != World
		|| !IsFiniteVector(AuthorityRangeOrigin)
		|| !IsFiniteVector(AuthorityTraceStart)
		|| !IsFiniteVector(RequestedLocation)
		|| !FMath::IsFinite(Params.MaxRange)
		|| Params.MaxRange <= 0.0
		|| !FMath::IsFinite(Params.RangeTolerance)
		|| Params.RangeTolerance < 0.0
		|| !IsBlockingCollisionProfile(Params.LineOfSightProfileName)
		|| !IsWithinRange(AuthorityRangeOrigin, RequestedLocation, Params.MaxRange, Params.RangeTolerance)
		|| FVector::DistSquared(AuthorityTraceStart, RequestedLocation) <= UE_SMALL_NUMBER)
	{
		return false;
	}

	TArray<AActor*> ActorsToIgnore;
	PdSkillGroundProjection::AddIgnoredActorAndAttachments(ActorsToIgnore, AuthoritySourceActor);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ServerPointTargetLineOfSight), false);
	for (AActor* ActorToIgnore : ActorsToIgnore)
	{
		if (IsValid(ActorToIgnore))
		{
			QueryParams.AddIgnoredActor(ActorToIgnore);
		}
	}

	FHitResult BlockingHit;
	const bool bBlockingHit = World->LineTraceSingleByProfile(
		BlockingHit,
		AuthorityTraceStart,
		RequestedLocation,
		Params.LineOfSightProfileName,
		QueryParams);

	const FVector ValidatedLocation = bBlockingHit
		? BlockingHit.ImpactPoint
		: RequestedLocation;
	if (!IsFiniteVector(ValidatedLocation)
		|| !IsWithinRange(AuthorityRangeOrigin, ValidatedLocation, Params.MaxRange, Params.RangeTolerance))
	{
		return false;
	}

	OutValidatedTarget.Location = ValidatedLocation;
	OutValidatedTarget.BlockingActor = bBlockingHit ? BlockingHit.GetActor() : nullptr;
	return true;
}
