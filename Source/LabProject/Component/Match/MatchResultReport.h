#pragma once

#include "CoreMinimal.h"
#include "Common/GameResultTypes.h"

class AExperienceGameMode;
class AGameStateBase;
class APlayerState;
struct FVictoryGoldRates;

/** 끝난 경기를 참가자 결과 화면 데이터와 백엔드 전적으로 만든다. */
namespace MatchResultReport
{
	FText ResolveTeamName(int32 TeamColorIndex);

	/** 승리 팀 이름을 넣은 결과 제목. */
	FText ResolveWinnerTitle(int32 WinnerTeamColorIndex);

	/** 처치 많은 순, 사망 적은 순, 이름 순. 결과 화면·점수판·전적이 모두 이 순서를 쓴다. */
	void SortPlayerStats(TArray<FGameResultPlayerStat>& PlayerStats);

	/** 경기 결과·점수판에 쓰는 참가자 기록을 SortPlayerStats 순서로 만든다. 서버와 클라이언트 어디서나 쓸 수 있다. */
	TArray<FGameResultPlayerStat> BuildPlayerStats(const AGameStateBase& GameState);

	/** 승리 팀(팀이 없으면 승자)에게 승리 골드 표시를 채운다. 나간 플레이어는 보상 대상에서 뺀다. */
	void ApplyVictoryRewards(
		TArray<FGameResultPlayerStat>& PlayerStats,
		const APlayerState* WinnerPlayerState,
		int32 WinnerTeamColorIndex,
		int32 WinnerTeamMemberCount,
		const APlayerState* ExcludedPlayerState,
		const FVictoryGoldRates& Rates);

	/**
	 * 플레이어가 나가 경기가 끝났을 때 남은 참가자에게 보여 줄 결과. 승자가 없으면 보상을 표시하지 않는다.
	 * 연결이 끊긴 클라이언트는 누가 나갔는지 모르므로 나간 사람과 승자 없이 부른다.
	 */
	FGameResultPresentationData BuildPlayerExitResult(
		const AGameStateBase& GameState,
		const APlayerState* ExitingPlayerState,
		const APlayerState* WinnerPlayerState,
		int32 WinnerTeamColorIndex,
		int32 WinnerTeamMemberCount,
		const FVictoryGoldRates& Rates);

	/**
	 * 전적을 백엔드에 보고한다. 보고 서브시스템은 신뢰할 수 있는 전용 서버에만 있고, 훈련장과 봇은 기록하지 않는다.
	 * 나간 플레이어는 패배, 팀 승리면 같은 팀 전원 승리, 승자가 없으면 무승부로 기록한다.
	 */
	void ReportToBackend(
		const AExperienceGameMode& GameMode,
		const APlayerState* WinnerPlayerState,
		int32 WinnerTeamColorIndex,
		const TCHAR* EndReason,
		const APlayerState* ExitingPlayerState = nullptr);
}
