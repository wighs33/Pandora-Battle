#pragma once

#include "CoreMinimal.h"

class UProgressBar;
class UTextBlock;
class UWidget;

// 능력 슬롯과 행동 슬롯이 함께 쓰는 쿨다운 표시(타이머 묶음, 진행 막대, 남은 초 글자).
namespace PdSlotCooldownDisplay
{
	// 남은 시간을 진행률(0~1)로 바꾼다. 전체 시간을 모르면 다 찬 것으로 본다.
	float CalculatePercent(float TimeRemaining, double CooldownDuration);

	// 쿨다운이 없으면 타이머 묶음을 접고 진행 막대를 채우고 남은 초 글자를 비운다.
	void ShowReady(UWidget* TimerContainer, UProgressBar* Progress, UTextBlock* TimerText);

	// 쿨다운 중이면 진행 막대와 남은 초 글자를 남은 시간에 맞춘다.
	void ShowRemaining(UProgressBar* Progress, UTextBlock* TimerText, float TimeRemaining, double CooldownDuration, bool bShowTimeRemaining);
}
