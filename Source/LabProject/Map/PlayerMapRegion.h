#pragma once

#include "CoreMinimal.h"
#include "PlayerMapRegion.generated.h"

UENUM(BlueprintType)
enum class EPlayerMapRegion : uint8
{
	Windmill UMETA(DisplayName = "Windmill"),
	Dome UMETA(DisplayName = "Dome"),
	Temple UMETA(DisplayName = "Temple")
};
