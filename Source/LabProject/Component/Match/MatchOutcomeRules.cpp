#include "Component/Match/MatchOutcomeRules.h"

FMatchOutcome MatchOutcomeRules::Resolve(const TConstArrayView<FMatchStanding> Standings)
{
	FMatchOutcome Outcome;
	TArray<int32, TInlineAllocator<8>> LeaderIndices;
	for (int32 Index = 0; Index < Standings.Num(); ++Index)
	{
		const int32 Score = FMath::Max(Standings[Index].Score, 0);
		if (LeaderIndices.IsEmpty() || Score > Outcome.TopScore)
		{
			Outcome.TopScore = Score;
			LeaderIndices.Reset();
			LeaderIndices.Add(Index);
		}
		else if (Score == Outcome.TopScore)
		{
			LeaderIndices.Add(Index);
		}
	}

	if (LeaderIndices.IsEmpty())
	{
		return Outcome;
	}

	Outcome.TopScorerIndex = LeaderIndices[0];
	if (LeaderIndices.Num() == 1)
	{
		Outcome.Type = EMatchOutcomeType::UniqueLeader;
		Outcome.WinnerIndex = LeaderIndices[0];
		return Outcome;
	}

	TArray<int32, TInlineAllocator<8>> LeaderTeamColorIndices;
	for (const int32 LeaderIndex : LeaderIndices)
	{
		LeaderTeamColorIndices.Add(Standings[LeaderIndex].TeamColorIndex);
	}

	if (IsOpposingTie(LeaderTeamColorIndices))
	{
		Outcome.Type = EMatchOutcomeType::OpposingTie;
		return Outcome;
	}

	Outcome.Type = EMatchOutcomeType::LeadingTeam;
	Outcome.WinnerIndex = LeaderIndices[0];
	return Outcome;
}

bool MatchOutcomeRules::IsOpposingTie(const TConstArrayView<int32> LeaderTeamColorIndices)
{
	if (LeaderTeamColorIndices.Num() < 2)
	{
		return false;
	}

	const int32 FirstTeamColorIndex = LeaderTeamColorIndices[0];
	for (const int32 TeamColorIndex : LeaderTeamColorIndices)
	{
		if (TeamColorIndex == INDEX_NONE || TeamColorIndex != FirstTeamColorIndex)
		{
			return true;
		}
	}
	return false;
}

int32 MatchOutcomeRules::CountTeamMembers(const TConstArrayView<FMatchStanding> Standings, const int32 TeamColorIndex)
{
	if (TeamColorIndex == INDEX_NONE)
	{
		return 1;
	}

	int32 TeamMemberCount = 0;
	for (const FMatchStanding& Standing : Standings)
	{
		if (Standing.TeamColorIndex == TeamColorIndex)
		{
			++TeamMemberCount;
		}
	}
	return FMath::Max(TeamMemberCount, 1);
}

int32 MatchOutcomeRules::CalculateVictoryGold(
	const int32 KillCount,
	const int32 DeathCount,
	const int32 WinningTeamMemberCount,
	const FVictoryGoldRates& Rates)
{
	const int32 RawReward =
		FMath::Max(KillCount, 0) * FMath::Max(Rates.GoldPerKill, 0)
		- FMath::Max(DeathCount, 0) * FMath::Max(Rates.PenaltyPerDeath, 0)
		+ FMath::Max(WinningTeamMemberCount, 1) * FMath::Max(Rates.GoldPerWinningTeamMember, 0);
	return FMath::Max(RawReward, 0);
}
