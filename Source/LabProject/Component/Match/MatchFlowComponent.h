#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "TimerManager.h"
#include "UI/Match/GameResultTypes.h"
#include "MatchFlowComponent.generated.h"

class AExperienceGameMode;
class APdPlayerState;
class APlayerState;
class ARewardChest;
class URewardDefinition;
struct FLobbyMatchMapOption;
struct FStreamableHandle;

/**
 * 서버 권한으로 경기 타이머·승리·보상·결과·퇴장 흐름을 조율한다.
 */
UCLASS(ClassGroup = (Match))
class LABPROJECT_API UMatchFlowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Public API ------------------------------------------------------------------------------------------------------
	UMatchFlowComponent();

	void PreloadRewardContent();

	void InitializeTravelOptions(const FString& Options);
	void InitializeGameState();
	void StartServerMatchTimerIfNeeded();

	void NotifyPlayerKillScored(
		APlayerState* KillerPlayerState,
		APlayerState* VictimPlayerState);
	bool RequestAbortMatchToTitle(APlayerController* RequestingPlayer);

	bool FindCurrentMatchMapOption(
		FLobbyMatchMapOption& OutMapOption) const;
	bool IsGameResultShown() const { return bGameResultShown; }
	/** travel 옵션 RpgMode로 연 공유 월드. 경기 타이머·결과·이탈 종료가 없다. */
	bool IsRpgMode() const { return bRpgMode; }
	bool HandlePlayerLogout(const APlayerState* ExitingPlayerState);

private:
	int32 GrantGameVictoryGoldReward(AController* WinnerController, int32 WinningTeamMemberCount);
	bool ShowGameResultForWinner(APlayerState* WinnerPlayerState);
	static bool ShouldEnterGoldenKillForLeaderTeams(const TArray<int32>& LeaderTeamColorIndices);
	int32 CalculateVictoryGoldReward(int32 KillCount, int32 DeathCount, int32 WinningTeamMemberCount) const;
	void ConfigureRewardChestSpawns();
	void HandleMatchTimerExpired();
	void ReturnToLobbyAfterGameResult();
	void HandleRewardContentLoaded();
	void ReportMatchResultToBackend(
		const APlayerState* WinnerPlayerState,
		int32 WinnerTeamColorIndex,
		const TCHAR* EndReason,
		const APlayerState* ExitingPlayerState = nullptr) const;
	void SendPlayersToTitleForSessionEnd() const;

	// Internal Helpers ------------------------------------------------------------------------------------------------
	AExperienceGameMode* GetExperienceGameMode() const;

	bool ShouldSuppressServerMatchTimerForCurrentMap() const;
	bool ShouldSuppressServerMatchTimer() const;
	bool TryFindUniqueKillLeader(
		APdPlayerState*& OutWinnerPlayerState,
		int32& OutTopKillCount,
		bool& bOutTie,
		const APlayerState* ExcludedPlayerState = nullptr) const;
	bool TryFindSharedLeadingTeamWinner(
		APdPlayerState*& OutWinnerPlayerState,
		int32 TopKillCount,
		const APlayerState* ExcludedPlayerState = nullptr) const;
	bool FindTopKiller(
		APdPlayerState*& OutTopKillerPlayerState,
		int32& OutTopKillCount) const;
	bool ShouldAbortMatchForPlayerExit(
		const APlayerState* ExitingPlayerState) const;
	bool AbortMatchToTitleForPlayerExit(
		const APlayerState* ExitingPlayerState);
	FString GetResolvedTitleTravelMapName() const;
	FString GetResolvedLobbyTravelMapName() const;
	FGameResultPresentationData BuildPlayerExitGameResult(
		const APlayerState* ExitingPlayerState,
		const APlayerState* WinnerPlayerState,
		int32 WinnerTeamColorIndex,
		int32 WinnerTeamMemberCount) const;
	void SendPlayerExitGameResultToTitle(
		const FGameResultPresentationData& GameResultData,
		const APlayerState* ExitingPlayerState);
	AController* FindControllerForPlayerState(
		const APlayerState* PlayerState) const;
	int32 CountPlayersOnTeam(
		int32 TeamColorIndex,
		const APlayerState* ExcludedPlayerState = nullptr) const;
	int32 GrantVictoryRewardsForWinner(
		APlayerState* WinnerPlayerState,
		int32 WinnerTeamColorIndex,
		int32 WinnerTeamMemberCount,
		const APlayerState* ExcludedPlayerState = nullptr);
	void ApplyVictoryRewardEligibility(
		TArray<FGameResultPlayerStat>& PlayerStats,
		const APlayerState* WinnerPlayerState,
		int32 WinnerTeamColorIndex,
		int32 WinnerTeamMemberCount,
		const APlayerState* ExcludedPlayerState = nullptr) const;
	int32 CalculateVictoryGoldReward(
		const APdPlayerState* PlayerState,
		int32 WinningTeamMemberCount) const;
	FText ResolveResultPlayerName(
		const APlayerState* PlayerState) const;
	FText ResolveResultTeamName(int32 TeamColorIndex) const;
	FText ResolveWinnerTeamTitle(
		const APlayerState* WinnerPlayerState) const;
	void BuildGameResultPlayerStats(
		TArray<FGameResultPlayerStat>& OutPlayerStats) const;
	void ForceMovePlayersForGoldenKill();
	void RaiseForceMoveGatesForGoldenKill();
	void StartGoldenKill();
	void RestorePlayerResourcesForGoldenKill() const;
	const URewardDefinition* ResolveRewardDefinitionForChestSpawns(
		const TArray<ARewardChest*>& RewardChests) const;
	void FinishMatchRuntime();

private:
	bool bServerMatchTimerStarted = false;
	bool bMatchTimerExpired = false;
	bool bGoldenKillActive = false;
	bool bGameResultShown = false;
	bool bMatchTimerSuppressedByTravelOption = false;
	bool bRpgMode = false;
	FTimerHandle MatchTimerHandle;
	FTimerHandle ChestConfigurationRetryTimerHandle;
	FTimerHandle GameResultLobbyReturnTimerHandle;
	TSharedPtr<FStreamableHandle> RewardContentPreloadHandle;
};
