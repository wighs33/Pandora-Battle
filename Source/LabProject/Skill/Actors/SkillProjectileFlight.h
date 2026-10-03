#pragma once

#include "CoreMinimal.h"

class UProjectileMovementComponent;
class USceneComponent;

/** 투사체 이동 규칙. 목표 지점과 속도로 직선 또는 포물선 발사 속도를 정해 ProjectileMovement를 시작한다. */
namespace PdSkillProjectileFlight
{
	struct FLaunchParams
	{
		FVector StartLocation = FVector::ZeroVector;
		FVector TargetLocation = FVector::ZeroVector;
		/** 목표가 시작 지점과 겹쳐 방향을 정할 수 없을 때 날아갈 방향. */
		FVector FallbackDirection = FVector::ForwardVector;
		float Speed = 0.0f;
		bool bUseArcTrajectory = false;
		float ArcHeight = 0.0f;
		float ArcGravityScale = 1.0f;
		float WorldGravityZ = -980.0f;
	};

	/**
	 * 포물선을 그려 목표에 닿는 초기 속도.
	 * ArcHeight가 있으면 그 높이를 지나는 시간으로, 없으면 Speed로 수평 거리를 나는 시간으로 비행 시간을 정한다.
	 * 계산할 수 없으면 0 벡터를 돌려준다.
	 */
	LABPROJECT_API FVector CalculateArcLaunchVelocity(const FLaunchParams& Params);

	/** Speed가 0보다 클 때만 발사한다. 포물선 속도를 구할 수 없으면 직선으로 날린다. */
	LABPROJECT_API void Launch(UProjectileMovementComponent& Movement, USceneComponent* UpdatedComponent, const FLaunchParams& Params);
}
