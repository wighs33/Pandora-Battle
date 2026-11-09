#pragma once

#include "CoreMinimal.h"

class UProgressBar;
class UTextBlock;
class UWidget;

// 능력 슬롯과 행동 슬롯이 함께 쓰는 쿨다운 표시(타이머 묶음, 그림을 덮는 그늘 막대, 남은 초 글자).
// 그늘 막대는 남은 시간만큼 그림을 덮고, 시간이 지나면 아래로 걷힌다(막대의 채움 방향은 위젯 애셋이 정한다).
namespace SlotCooldownDisplay
{
	// 남은 시간을 전체 시간에 대한 비율(0~1)로 바꾼다. 전체 시간을 모르면 남은 것이 없는 것으로 본다.
	float CalculateRemainingFraction(float TimeRemaining, double CooldownDuration);

	// 쿨다운이 없으면 타이머 묶음을 접고 그늘을 걷고 남은 초 글자를 비운다.
	void ShowReady(UWidget* TimerContainer, UProgressBar* Shade, UTextBlock* TimerText);

	// 쿨다운 중이면 그늘과 남은 초 글자를 남은 시간에 맞춘다.
	void ShowRemaining(UProgressBar* Shade, UTextBlock* TimerText, float TimeRemaining, double CooldownDuration, bool bShowTimeRemaining);
}
