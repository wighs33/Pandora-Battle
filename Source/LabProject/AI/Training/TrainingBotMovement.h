#pragma once

#include "CoreMinimal.h"

class AAIController;
class AActor;
class APawn;
namespace EPathFollowingRequestResult { enum Type : int; }

/** 훈련 봇 BT 태스크가 함께 쓰는 이동 목적지 계산과 대상 바라보기. */
namespace TrainingBotMovement
{
	/** 대상에서 폰 쪽으로 향하는 수평 단위 벡터. 두 위치가 겹치면 대상의 뒤쪽을 쓴다. */
	FVector GetFlatDirectionFromTarget(const APawn& Pawn, const AActor& TargetActor);

	/** 대상에서 거리 범위 안으로 떨어진 뒤 대상 오른쪽으로 무작위 걸음 수만큼 비켜선 위치. 높이는 폰과 같다. */
	FVector BuildSideStepDestination(
		const APawn& Pawn,
		const AActor& TargetActor,
		float MinDistance,
		float MaxDistance,
		int32 MinSideStepMultiplier,
		int32 MaxSideStepMultiplier,
		float SideStepDistance);

	/** 내비메시 위로 옮긴 위치. 내비게이션이 없거나 투영에 실패하면 그대로 돌려준다. */
	FVector ProjectToNavigation(const UWorld* World, const FVector& Location, float ProjectionExtent);

	/**
	 * 폰과 컨트롤러를 대상 쪽 수평 방향으로 돌린다. InterpSpeed와 DeltaSeconds가 양수면 그 속도로, 아니면 바로 돌린다.
	 * 얼어 있는 적은 돌리지 않고 포커스만 푼다.
	 */
	void FaceTarget(AAIController& AIController, APawn& Pawn, AActor& TargetActor, float DeltaSeconds, float InterpSpeed);

	/** 경로 탐색과 부분 경로를 허용한 위치 이동 요청. 훈련 봇 태스크가 모두 같은 옵션으로 움직인다. */
	EPathFollowingRequestResult::Type MoveToLocation(
		AAIController& AIController,
		const FVector& Destination,
		float AcceptanceRadius,
		bool bStopOnOverlap);
}
