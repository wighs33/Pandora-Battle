#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

// 지금 처리 중인 일을 마친 다음 틱에 한 번 실행한다. 소유 객체가 그 전에 사라지면 실행하지 않는다.
// 코어 ticker를 쓰므로 게임이 일시정지된 동안에도 실행된다. 화면 동작과 월드 밖 서브시스템의 일은 이것을 쓰고,
// 일시정지와 함께 멈춰야 하는 게임플레이 일이나 핸들로 같은 틱의 요청을 합치는 일은 월드 타이머의 SetTimerForNextTick을 쓴다.
namespace Ticker
{
	inline void RunNextTick(const UObject* Owner, TFunction<void()> Callback)
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(Owner, [Callback = MoveTemp(Callback)](float)
		{
			Callback();
			return false;
		}));
	}

	template <typename OwnerType, ESPMode Mode>
	void RunNextTick(const TSharedRef<OwnerType, Mode>& Owner, TFunction<void()> Callback)
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSPLambda(Owner, [Callback = MoveTemp(Callback)](float)
		{
			Callback();
			return false;
		}));
	}
}
