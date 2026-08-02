#pragma once

#include "CoreMinimal.h"
#include "PdMapTypes.generated.h"

UENUM(BlueprintType)
enum class EPdPlayerMapRegion : uint8
{
	Windmill UMETA(DisplayName = "Windmill"),
	Dome UMETA(DisplayName = "Dome"),
	Temple UMETA(DisplayName = "Temple")
};
