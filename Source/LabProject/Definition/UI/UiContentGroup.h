#pragma once

#include "CoreMinimal.h"

/** DA_Widget의 참조를 화면 수명에 따라 나눈 그룹. AssetManager의 Asset Bundle과는 별개다. */
enum class EUiContentGroup : uint8
{
	Core,
	Lobby,
	InGame,
	Info,
	Map
};
