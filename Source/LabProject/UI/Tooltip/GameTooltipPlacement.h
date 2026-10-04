#pragma once

#include "CoreMinimal.h"

class UWidget;

/** Slate 툴팁 제외 영역을 게임의 소프트웨어 커서까지 넓힌다. */
namespace GameTooltipPlacement
{
	void ApplyToButton(UWidget* Widget);
}
