#pragma once

#include "CoreMinimal.h"

/**
 * 유지해야 하는 로그의 반복 출력을 호출 위치·객체별로 제한한다.
 * 틱·재시도·반복 RPC·다수 액터로 인한 로그 중복을 줄인다.
 */
struct FLogRateLimiter
{

public:
	// Public API ------------------------------------------------------------------------------------------------------
	bool TryAcquire(const double IntervalSeconds, uint32& OutSuppressedCount)
	{
		const double CurrentTimeSeconds = FPlatformTime::Seconds();
		if (CurrentTimeSeconds < NextAllowedLogTimeSeconds)
		{
			++SuppressedCount;
			OutSuppressedCount = 0;
			return false;
		}

		OutSuppressedCount = SuppressedCount;
		SuppressedCount = 0;
		NextAllowedLogTimeSeconds =
			CurrentTimeSeconds + FMath::Max(IntervalSeconds, 0.0);
		return true;
	}

private:
	double NextAllowedLogTimeSeconds = 0.0;
	uint32 SuppressedCount = 0;
};
