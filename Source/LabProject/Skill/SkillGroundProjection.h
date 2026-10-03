#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

class AActor;
class UWorld;

namespace PdSkillGroundProjection
{
	struct FGroundProjectionResult
	{
		FVector Location = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
		TWeakObjectPtr<AActor> HitActor;
	};

	LABPROJECT_API void AddIgnoredActorAndAttachments(TArray<AActor*>& ActorsToIgnore, AActor* Actor);

	/** 캐릭터면 캡슐 바닥, 아니면 액터 위치. */
	LABPROJECT_API FVector ResolveActorFeetLocation(const AActor* Actor);

	LABPROJECT_API bool TryProjectToGround(
		UWorld* World,
		const FVector& SourceLocation,
		TEnumAsByte<ETraceTypeQuery> TraceType,
		double TraceStartHeight,
		double TraceDepth,
		const TArray<AActor*>& ActorsToIgnore,
		FGroundProjectionResult& OutResult);
}
