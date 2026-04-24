#pragma once

#include "CoreMinimal.h"
#include "Enum_Direction.generated.h"

UENUM(BlueprintType)
enum class EEnum_Direction : uint8
{
	Center UMETA(DisplayName = "Center"),
	Up UMETA(DisplayName = "Up"),
	Right UMETA(DisplayName = "Right"),
	Down UMETA(DisplayName = "Down"),
	Left UMETA(DisplayName = "Left")
};
