#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

namespace LabCollisionChannels
{
	/** Project object channels resolved from their Project Settings names. */
	LABPROJECT_API ECollisionChannel HitableBody();
	LABPROJECT_API ECollisionChannel Projectile();
	LABPROJECT_API ECollisionChannel OverlapBox();

	/** Named engine trace used for ground and grapple traces. */
	LABPROJECT_API ETraceTypeQuery VisibilityTrace();
}
