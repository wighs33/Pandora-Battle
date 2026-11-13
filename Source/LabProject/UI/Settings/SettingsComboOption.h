#pragma once

#include "CoreMinimal.h"

class UFont;
class UTextBlock;
class UWidgetTree;

namespace SettingsComboOption
{
	/** 설정 콤보를 펼쳤을 때 보이는 한 줄. 글자색은 콤보 스타일의 전경색(크림색)을 따르고, 글꼴은 그 줄의 언어에 맞춘다. */
	UTextBlock* Make(UWidgetTree& Tree, const FString& Option, UFont* Font);
}
