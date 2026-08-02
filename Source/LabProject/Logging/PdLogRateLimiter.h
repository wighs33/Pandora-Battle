#pragma once

#include "CoreMinimal.h"

/**
 * Small per-call-site/per-object limiter for logs that must remain visible but
 * can otherwise be repeated by ticks, retries, RPC spam, or many actors.
 */
struct FPdLogRateLimiter
{
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
