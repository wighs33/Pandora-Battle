#pragma once

#include "CoreMinimal.h"

// 연결된 두 포털 사이의 좌표 변환. 원본 포털 앞의 값을 대상 포털 뒤쪽의 같은 상대 값으로 옮긴다.
// 기준 변환은 포털 평면 위치와 앞 방향이며, 평면 기준으로 앞뒤·좌우를 뒤집는다.
namespace PdPortalSpace
{
	FVector TransformLocation(const FTransform& SourcePortal, const FTransform& TargetPortal, const FVector& WorldLocation);

	// 방향은 단위 벡터로 돌려준다.
	FVector TransformDirection(const FTransform& SourcePortal, const FTransform& TargetPortal, const FVector& WorldDirection);

	FRotator TransformRotation(const FTransform& SourcePortal, const FTransform& TargetPortal, const FRotator& WorldRotation);

	// 속력은 그대로 두고 방향만 옮긴다.
	FVector TransformVelocity(const FTransform& SourcePortal, const FTransform& TargetPortal, const FVector& WorldVelocity);
}
