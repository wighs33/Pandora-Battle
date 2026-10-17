#pragma once

#include "CoreMinimal.h"

// 연결된 두 포털 사이의 좌표 변환. 원본 포털 앞의 값을 대상 포털 뒤쪽의 같은 상대 값으로 옮긴다.
// 기준 변환은 포털 평면 위치와 앞 방향이며, 평면 기준으로 앞뒤·좌우를 뒤집는다.
namespace PdPortalSpace
{
	FVector TransformLocation(const FTransform& SourcePortal, const FTransform& TargetPortal, const FVector& WorldLocation);

	// 방향이나 속도처럼 위치 없이 방향과 길이만 있는 값을 옮긴다. 길이는 바뀌지 않는다.
	FVector TransformVector(const FTransform& SourcePortal, const FTransform& TargetPortal, const FVector& WorldVector);

	FRotator TransformRotation(const FTransform& SourcePortal, const FTransform& TargetPortal, const FRotator& WorldRotation);
}
