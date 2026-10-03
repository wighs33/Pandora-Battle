#include "Skill/Actors/SkillProjectileFlight.h"

#include "GameFramework/ProjectileMovementComponent.h"

FVector PdSkillProjectileFlight::CalculateArcLaunchVelocity(const FLaunchParams& Params)
{
	const float GravityScale = FMath::Max(Params.ArcGravityScale, UE_SMALL_NUMBER);
	const float GravityZ = Params.WorldGravityZ * GravityScale;
	const float GravityMagnitude = FMath::Abs(GravityZ);
	if (GravityMagnitude <= UE_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const FVector Delta = Params.TargetLocation - Params.StartLocation;
	const FVector HorizontalDelta(Delta.X, Delta.Y, 0.0);
	const float HorizontalDistance = HorizontalDelta.Size();

	float TravelTime = 0.0f;
	if (Params.ArcHeight > UE_SMALL_NUMBER)
	{
		TravelTime = FMath::Sqrt((8.0f * Params.ArcHeight) / GravityMagnitude);
	}
	else if (Params.Speed > UE_SMALL_NUMBER && HorizontalDistance > UE_SMALL_NUMBER)
	{
		TravelTime = HorizontalDistance / Params.Speed;
	}

	if (TravelTime <= UE_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const FVector HorizontalVelocity = HorizontalDistance > UE_SMALL_NUMBER
		? HorizontalDelta / TravelTime
		: FVector::ZeroVector;
	const float VerticalVelocity = (Delta.Z - (0.5f * GravityZ * FMath::Square(TravelTime))) / TravelTime;
	return HorizontalVelocity + FVector::UpVector * VerticalVelocity;
}

void PdSkillProjectileFlight::Launch(
	UProjectileMovementComponent& Movement,
	USceneComponent* UpdatedComponent,
	const FLaunchParams& Params)
{
	if (Params.Speed <= 0.0f)
	{
		return;
	}

	FVector Direction = (Params.TargetLocation - Params.StartLocation).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = Params.FallbackDirection;
	}

	Movement.SetUpdatedComponent(UpdatedComponent);
	Movement.ProjectileGravityScale = 0.0f;
	Movement.InitialSpeed = Params.Speed;
	Movement.MaxSpeed = Params.Speed;
	Movement.Velocity = Direction * Params.Speed;
	if (Params.bUseArcTrajectory)
	{
		const FVector ArcVelocity = CalculateArcLaunchVelocity(Params);
		if (!ArcVelocity.IsNearlyZero())
		{
			Movement.ProjectileGravityScale = Params.ArcGravityScale;
			Movement.InitialSpeed = ArcVelocity.Size();
			Movement.MaxSpeed = FMath::Max(Params.Speed, ArcVelocity.Size()) * 2.0f;
			Movement.Velocity = ArcVelocity;
		}
	}
	Movement.Activate(true);
	Movement.UpdateComponentVelocity();
}
