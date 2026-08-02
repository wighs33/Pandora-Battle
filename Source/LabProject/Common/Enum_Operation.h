#pragma once

#include "CoreMinimal.h"
#include "Enum_Operation.generated.h"

UENUM(BlueprintType)
enum class EEnum_Operation : uint8
{
	Add UMETA(DisplayName = "Add"),
	Multiply UMETA(DisplayName = "Multiply")
};
