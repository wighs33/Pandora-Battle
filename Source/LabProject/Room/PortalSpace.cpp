#include "Room/PortalSpace.h"

#include "Kismet/KismetMathLibrary.h"

FVector PdPortalSpace::TransformLocation(const FTransform& SourcePortal, const FTransform& TargetPortal, const FVector& WorldLocation)
{
	FVector LocalLocation = SourcePortal.InverseTransformPositionNoScale(WorldLocation);
	LocalLocation.X *= -1.0f;
	LocalLocation.Y *= -1.0f;
	return TargetPortal.TransformPositionNoScale(LocalLocation);
}

FVector PdPortalSpace::TransformDirection(const FTransform& SourcePortal, const FTransform& TargetPortal, const FVector& WorldDirection)
{
	FVector LocalDirection = SourcePortal.InverseTransformVectorNoScale(WorldDirection);
	LocalDirection.X *= -1.0f;
	LocalDirection.Y *= -1.0f;
	return TargetPortal.TransformVectorNoScale(LocalDirection).GetSafeNormal();
}

FRotator PdPortalSpace::TransformRotation(const FTransform& SourcePortal, const FTransform& TargetPortal, const FRotator& WorldRotation)
{
	const FRotationMatrix RotationMatrix(WorldRotation);
	const FVector Forward = TransformDirection(SourcePortal, TargetPortal, RotationMatrix.GetScaledAxis(EAxis::X));
	const FVector Right = TransformDirection(SourcePortal, TargetPortal, RotationMatrix.GetScaledAxis(EAxis::Y));
	const FVector Up = TransformDirection(SourcePortal, TargetPortal, RotationMatrix.GetScaledAxis(EAxis::Z));

	return UKismetMathLibrary::MakeRotationFromAxes(Forward, Right, Up);
}

FVector PdPortalSpace::TransformVelocity(const FTransform& SourcePortal, const FTransform& TargetPortal, const FVector& WorldVelocity)
{
	const double Speed = WorldVelocity.Size();
	if (Speed <= UE_KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	return TransformDirection(SourcePortal, TargetPortal, WorldVelocity / Speed) * Speed;
}
