#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace LabSkillDebug
{
	/** 범위 피해 반경을 바닥 원과 반구 선으로 그린다. 그리기 허용 여부는 호출하는 쪽이 판단한다. */
	LABPROJECT_API void DrawAreaRadius(
		const UWorld* World,
		const FVector& Center,
		float Radius,
		const FColor& CircleColor,
		const FColor& SphereColor,
		float DrawTime);
}
