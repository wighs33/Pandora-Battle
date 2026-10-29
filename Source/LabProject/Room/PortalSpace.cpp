#include "Room/PortalSpace.h"

#include "Kismet/KismetMathLibrary.h"

namespace
{
	// 포털로 들어간 것은 대상 포털에서 돌아서 나온다. 포털의 위 축(Z)으로 반 바퀴 돌리면 앞뒤(X)와 좌우(Y)가 함께 뒤집힌다.
	FVector TurnAroundPortalUp(FVector PortalLocal)
	{
		PortalLocal.X = -PortalLocal.X;
		PortalLocal.Y = -PortalLocal.Y;
		return PortalLocal;
	}
}

FVector PortalSpace::TransformLocation(const FTransform& SourcePortal, const FTransform& TargetPortal, const FVector& WorldLocation)
{
	const FVector PortalLocal = SourcePortal.InverseTransformPositionNoScale(WorldLocation);
	return TargetPortal.TransformPositionNoScale(TurnAroundPortalUp(PortalLocal));
}

FVector PortalSpace::TransformVector(const FTransform& SourcePortal, const FTransform& TargetPortal, const FVector& WorldVector)
{
	const FVector PortalLocal = SourcePortal.InverseTransformVectorNoScale(WorldVector);
	return TargetPortal.TransformVectorNoScale(TurnAroundPortalUp(PortalLocal));
}

FRotator PortalSpace::TransformRotation(const FTransform& SourcePortal, const FTransform& TargetPortal, const FRotator& WorldRotation)
{
	const FRotationMatrix RotationMatrix(WorldRotation);
	const FVector Forward = TransformVector(SourcePortal, TargetPortal, RotationMatrix.GetUnitAxis(EAxis::X));
	const FVector Right = TransformVector(SourcePortal, TargetPortal, RotationMatrix.GetUnitAxis(EAxis::Y));
	const FVector Up = TransformVector(SourcePortal, TargetPortal, RotationMatrix.GetUnitAxis(EAxis::Z));

	return UKismetMathLibrary::MakeRotationFromAxes(Forward, Right, Up);
}
