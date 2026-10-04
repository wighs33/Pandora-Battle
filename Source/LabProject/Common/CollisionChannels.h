#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

namespace LabCollisionChannels
{
	/** 프로젝트 설정의 이름으로 찾은 오브젝트 채널 */
	LABPROJECT_API ECollisionChannel HitableBody();
	LABPROJECT_API ECollisionChannel Projectile();
	LABPROJECT_API ECollisionChannel OverlapBox();

	/** 지면 판정과 그래플 trace에 쓰는 엔진 trace 채널 */
	LABPROJECT_API ETraceTypeQuery VisibilityTrace();
}
