#pragma once

#include "CoreMinimal.h"
#include "Enum_Operation.generated.h"

/**
 * <스탯 변경 연산 타입>
 * - 스탯 증가 효과를 어떤 방식으로 적용할지 나타냅니다.
 * - Add는 값을 더합니다.
 * - Multiply는 배율 방식으로 반영할 때 사용합니다.
 */
UENUM(BlueprintType)
enum class EEnum_Operation : uint8
{
	Add UMETA(DisplayName = "Add"),
	Multiply UMETA(DisplayName = "Multiply")
};
