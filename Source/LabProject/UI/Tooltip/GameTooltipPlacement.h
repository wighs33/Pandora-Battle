#pragma once

#include "CoreMinimal.h"

class UWidget;

/** Extend Slate's tooltip exclusion zone to include the game's software cursor. */
namespace GameTooltipPlacement
{
	void ApplyToButton(UWidget* Widget);
}
