#pragma once

#include "CoreMinimal.h"
#include "Common/CollisionChannels.h"
#include "Engine/EngineTypes.h"

class AActor;
class UWorld;

namespace PdTargetValidator
{
	struct FTraceRequestValidationParams
	{
		double TraceStartOffset = 0.0;
		double MaxTraceDistance = 0.0;
		double TraceLengthTolerance = 100.0;
		double MaxViewDistanceFromSource = 1200.0;
	};

	struct FValidatedTraceView
	{
		FVector ViewLocation = FVector::ZeroVector;
		FVector ViewDirection = FVector::ForwardVector;
	};

	struct FGroundTargetValidationParams
	{
		double MaxRange = 0.0;
		double RangeTolerance = 100.0;
		double GroundTraceStartHeight = 500.0;
		double GroundTraceDepth = 1000.0;
		double LineOfSightSurfaceOffset = 10.0;
		double LineOfSightSurfaceTolerance = 30.0;
		int32 MaxIgnoredPawnCount = 8;
		TEnumAsByte<ETraceTypeQuery> GroundTraceType = LabCollisionChannels::VisibilityTrace();
		FName LineOfSightProfileName = NAME_None;
	};

	struct FPointTargetValidationParams
	{
		double MaxRange = 0.0;
		double RangeTolerance = 100.0;
		FName LineOfSightProfileName = NAME_None;
	};

	struct FValidatedGroundTarget
	{
		FVector Location = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
		TWeakObjectPtr<AActor> GroundActor;
	};

	struct FValidatedPointTarget
	{
		FVector Location = FVector::ZeroVector;
		TWeakObjectPtr<AActor> BlockingActor;
	};

	LABPROJECT_API bool TryResolveTargetDataLocation(
		const FHitResult& ClientHitResult,
		const FVector& TargetDataEndPoint,
		FVector& OutRequestedLocation);

	LABPROJECT_API bool ValidateClientTraceRequest(
		const AActor* AuthoritySourceActor,
		const FHitResult& ClientHitResult,
		const FTraceRequestValidationParams& Params,
		FValidatedTraceView& OutValidatedView);

	LABPROJECT_API bool ValidateGroundTarget(
		UWorld* World,
		AActor* AuthoritySourceActor,
		const FVector& AuthoritySourceLocation,
		const FVector& RequestedLocation,
		const FGroundTargetValidationParams& Params,
		FValidatedGroundTarget& OutValidatedTarget);

	LABPROJECT_API bool ValidatePointTarget(
		UWorld* World,
		AActor* AuthoritySourceActor,
		const FVector& AuthorityRangeOrigin,
		const FVector& AuthorityTraceStart,
		const FVector& RequestedLocation,
		const FPointTargetValidationParams& Params,
		FValidatedPointTarget& OutValidatedTarget);
}
