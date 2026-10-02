#pragma once

#include "CoreMinimal.h"

/** 승패 판정에 쓰는 참가자 한 명의 기록. 처치 점수와 팀만 본다. */
struct FMatchStanding
{
	int32 Score = 0;
	int32 TeamColorIndex = INDEX_NONE;
};

enum class EMatchOutcomeType : uint8
{
	/** 판정할 참가자가 없다. */
	NoPlayers,
	/** 최고 점수를 한 명이 가졌다. */
	UniqueLeader,
	/** 최고 점수를 같은 팀 여러 명이 나눠 가졌다. 팀 승리로 본다. */
	LeadingTeam,
	/** 최고 점수를 서로 다른 팀이나 팀 없는 참가자끼리 나눠 가졌다. 골든킬로 승부를 가린다. */
	OpposingTie
};

struct FMatchOutcome
{
	EMatchOutcomeType Type = EMatchOutcomeType::NoPlayers;
	/** 승자로 볼 참가자. 팀 승리면 최고 점수자 중 첫 번째이고, 승자가 없으면 INDEX_NONE이다. */
	int32 WinnerIndex = INDEX_NONE;
	/** 최고 점수자 중 첫 번째. 결과 화면의 최다 처치자로 표시한다. */
	int32 TopScorerIndex = INDEX_NONE;
	int32 TopScore = 0;

	bool HasWinner() const { return WinnerIndex != INDEX_NONE; }
};

struct FVictoryGoldRates
{
	int32 GoldPerKill = 0;
	int32 PenaltyPerDeath = 0;
	int32 GoldPerWinningTeamMember = 0;
};

/** 경기 승패와 승리 골드 규칙. 월드나 액터에 의존하지 않으므로 자동화 테스트로 검증한다. */
namespace MatchOutcomeRules
{
	LABPROJECT_API FMatchOutcome Resolve(TConstArrayView<FMatchStanding> Standings);

	/** 최고 점수자들의 팀 목록이 서로 다른 팀을 포함하는지 확인한다. 팀이 없는 참가자는 각자 다른 팀으로 본다. */
	LABPROJECT_API bool IsOpposingTie(TConstArrayView<int32> LeaderTeamColorIndices);

	/** 승리 골드 계산에 쓰는 팀 인원. 팀이 없으면 승자 혼자로 센다. */
	LABPROJECT_API int32 CountTeamMembers(TConstArrayView<FMatchStanding> Standings, int32 TeamColorIndex);

	LABPROJECT_API int32 CalculateVictoryGold(
		int32 KillCount,
		int32 DeathCount,
		int32 WinningTeamMemberCount,
		const FVictoryGoldRates& Rates);
}
