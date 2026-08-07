#pragma once

#include "CoreMinimal.h"

#include "WidgetContentBundle.generated.h"

/**
 * Runtime residency groups for DA_Widget soft references.
 *
 * The root definition and its hard class contract stay resident, while the
 * assets referenced by each group are acquired only for the matching screen.
 */
UENUM(BlueprintType)
enum class EWidgetContentBundle : uint8
{
	Core,
	Lobby,
	InGame,
	Info,
	Map
};

UENUM(BlueprintType)
enum class EWidgetContentBundleState : uint8
{
	Unloaded,
	Loading,
	Ready,
	Failed
};
