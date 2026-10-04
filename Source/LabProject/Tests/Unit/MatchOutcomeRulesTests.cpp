#include "Component/Match/MatchOutcomeRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// 최고 점수자가 한 명이면 그 사람이, 같은 팀끼리 나눠 가지면 그 팀이 이기고, 다른 팀끼리 나눠 가지면 골든킬로 넘어가는지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdMatchOutcomeResolveTest, "LabProject.Unit.Match.Outcome.Resolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FPdMatchOutcomeResolveTest::RunTest(const FString& Parameters)
{
	const FMatchOutcome Empty = MatchOutcomeRules::Resolve({});
	TestTrue(TEXT("no players"), Empty.Type == EMatchOutcomeType::NoPlayers && !Empty.HasWinner());

	const FMatchStanding Unique[] = {{3, 0}, {5, 1}, {4, 0}};
	const FMatchOutcome UniqueOutcome = MatchOutcomeRules::Resolve(Unique);
	TestTrue(TEXT("unique leader type"), UniqueOutcome.Type == EMatchOutcomeType::UniqueLeader);
	TestEqual(TEXT("unique leader"), UniqueOutcome.WinnerIndex, 1);
	TestEqual(TEXT("unique top score"), UniqueOutcome.TopScore, 5);

	// 같은 팀 두 명이 공동 1위면 첫 번째 공동 1위를 승자로 보고 팀 승리로 끝낸다.
	const FMatchStanding SameTeam[] = {{2, 1}, {6, 0}, {6, 0}};
	const FMatchOutcome SameTeamOutcome = MatchOutcomeRules::Resolve(SameTeam);
	TestTrue(TEXT("same team tie type"), SameTeamOutcome.Type == EMatchOutcomeType::LeadingTeam);
	TestEqual(TEXT("same team tie winner"), SameTeamOutcome.WinnerIndex, 1);

	const FMatchStanding Opposing[] = {{6, 0}, {6, 1}};
	const FMatchOutcome OpposingOutcome = MatchOutcomeRules::Resolve(Opposing);
	TestTrue(TEXT("opposing tie type"), OpposingOutcome.Type == EMatchOutcomeType::OpposingTie);
	TestFalse(TEXT("opposing tie has no winner"), OpposingOutcome.HasWinner());
	TestEqual(TEXT("opposing tie top scorer"), OpposingOutcome.TopScorerIndex, 0);

	// 팀이 없는 참가자끼리는 같은 팀 번호가 아니어도 각자 다른 편이다.
	const FMatchStanding NoTeam[] = {{1, INDEX_NONE}, {1, INDEX_NONE}};
	TestTrue(TEXT("no team tie"), MatchOutcomeRules::Resolve(NoTeam).Type == EMatchOutcomeType::OpposingTie);

	// 음수 점수는 0점으로 본다.
	const FMatchStanding Negative[] = {{-3, 0}, {0, 1}};
	TestTrue(TEXT("negative scores tie at zero"), MatchOutcomeRules::Resolve(Negative).Type == EMatchOutcomeType::OpposingTie);
	return true;
}

// 승리 골드는 처치당 보상에서 사망 감점을 빼고 팀 인원 보상을 더하며, 0 아래로 내려가지 않는지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdMatchVictoryGoldTest, "LabProject.Unit.Match.Outcome.VictoryGold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FPdMatchVictoryGoldTest::RunTest(const FString& Parameters)
{
	const FVictoryGoldRates Rates{100, 50, 3};
	TestEqual(TEXT("kills, deaths and team"), MatchOutcomeRules::CalculateVictoryGold(4, 2, 3, Rates), 400 - 100 + 9);
	TestEqual(TEXT("solo winner counts as one member"), MatchOutcomeRules::CalculateVictoryGold(1, 0, 0, Rates), 103);
	TestEqual(TEXT("never negative"), MatchOutcomeRules::CalculateVictoryGold(0, 10, 1, Rates), 0);

	const FMatchStanding Team[] = {{0, 2}, {0, 2}, {0, 1}};
	TestEqual(TEXT("team members"), MatchOutcomeRules::CountTeamMembers(Team, 2), 2);
	TestEqual(TEXT("no team"), MatchOutcomeRules::CountTeamMembers(Team, INDEX_NONE), 1);
	TestEqual(TEXT("missing team still counts the winner"), MatchOutcomeRules::CountTeamMembers(Team, 5), 1);
	return true;
}

#endif
