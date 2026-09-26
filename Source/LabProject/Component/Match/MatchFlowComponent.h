#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "TimerManager.h"
#include "UI/GameResultTypes.h"
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

	int32 GrantGameVictoryGoldReward(
		AController* WinnerController,
		int32 WinningTeamMemberCount);
	bool ShowGameResultForWinner(APlayerState* WinnerPlayerState);
	void NotifyPlayerKillScored(
		APlayerState* KillerPlayerState,
		APlayerState* VictimPlayerState);
	bool RequestAbortMatchToTitle(APlayerController* RequestingPlayer);

	bool FindCurrentMatchMapOption(
		FLobbyMatchMapOption& OutMapOption) const;
	bool IsGameResultShown() const { return bGameResultShown; }
	bool IsGoldenKillActive() const { return bGoldenKillActive; }
	int32 GetGoldenKillVictoryScore() const
	{
		return GoldenKillVictoryScore;
	}
	static int32 CalculateGoldenKillVictoryScore(int32 TopKillCount);
	static bool HasReachedGoldenKillVictoryScore(
		int32 KillCount,
		int32 VictoryScore);
	static bool ShouldEnterGoldenKillForLeaderTeams(
		const TArray<int32>& LeaderTeamColorIndices);

	int32 CalculateVictoryGoldReward(
		int32 KillCount,
		int32 DeathCount,
		int32 WinningTeamMemberCount) const;

	// Event Handlers --------------------------------------------------------------------------------------------------
	void ConfigureRewardChestSpawns();
	void HandleMatchTimerExpired();
	bool HandlePlayerLogout(const APlayerState* ExitingPlayerState);

private:
	void ReturnToLobbyAfterGameResult();
	void HandleRewardContentLoaded();

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
	void StartGoldenKill(int32 TopKillCount);
	void RestorePlayerResourcesForGoldenKill() const;
	const URewardDefinition* ResolveRewardDefinitionForChestSpawns(
		const TArray<ARewardChest*>& RewardChests) const;
	void FinishMatchRuntime();

private:
	bool bServerMatchTimerStarted = false;
	bool bMatchTimerExpired = false;
	bool bGoldenKillActive = false;
	int32 GoldenKillVictoryScore = 0;
	bool bGameResultShown = false;
	bool bMatchTimerSuppressedByTravelOption = false;
	FTimerHandle MatchTimerHandle;
	FTimerHandle ChestConfigurationRetryTimerHandle;
	FTimerHandle GameResultLobbyReturnTimerHandle;
	TSharedPtr<FStreamableHandle> RewardContentPreloadHandle;
};
