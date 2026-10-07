#pragma once

#include "CoreMinimal.h"

namespace LabGameSession
{
	inline constexpr int32 MaxPlayerCount = 6;
	inline constexpr const TCHAR* NoMatchTimerOption = TEXT("NoMatchTimer");

	/** 보스 레이드 월드로 이동할 때 붙이는 travel 옵션. 경기 타이머·결과·이탈 종료 없이 들어오고 나간다. */
	inline constexpr const TCHAR* BossRaidOption = TEXT("BossRaid");

	/** 보스 레이드는 서버 비용이 드는 기능이라 배포(Shipping) 빌드에서는 닫아 둔다. 에디터와 개발 빌드에서는 열려 있다. */
	inline constexpr bool bBossRaidOpen = !UE_BUILD_SHIPPING;

	/** 백엔드가 GameLift 게임 세션에 넣는 게임 속성. 값은 Backend/src/match_join.py와 같다. */
	inline constexpr const TCHAR* SessionModeProperty = TEXT("mode");
	inline constexpr const TCHAR* MatchSessionMode = TEXT("match");
	inline constexpr const TCHAR* BossRaidSessionMode = TEXT("bossraid");
}
