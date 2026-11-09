#pragma once

#include "CoreMinimal.h"
#include "UI/Common/UiMotion.h"

class UWidget;

/**
 * 쿨다운이 끝난 칸을 알린다. 금빛 고리가 칸 밖으로 퍼지며 사라지고, 그림은 살짝 커졌다가 탄성 있게 돌아온다.
 * 능력 칸과 행동 칸이 함께 쓴다. 칸 위젯이 매 프레임 Tick을 부르고, 재생 중이 아니면 아무것도 바꾸지 않는다.
 */
struct FSlotReadyPulse
{
	void Play(UWidget* Ring, UWidget* Icon);
	void Tick(float DeltaSeconds, UWidget* Ring, UWidget* Icon);

private:
	UiMotion::FOneShot RingProgress;
	UiMotion::FSpring IconScale{1.f, 0.f};
	bool bIconSettling = false;
};
