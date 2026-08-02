#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Definition/Experience/ExperienceGameModeSettings.h"
#include "TimerManager.h"
#include "UI/GameResultTypes.h"
#include "ExperienceMatchFlowComponent.generated.h"

class AExperienceGameMode;
class APdPlayerState;
class APlayerState;
class ARewardChest;
class UMatchRuleDefinition;
class URewardDefinition;
struct FLobbyMatchMapOption;
struct FStreamableHandle;

/**
 * Authoritative match timer, victory, reward, result, and exit coordinator.
 */
UCLASS(ClassGroup = (Experience))
class LABPROJECT_API UExperienceMatchFlowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UExperienceMatchFlowComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ApplySettings(const FExperienceMatchFlowSettings& InSettings);

	void InitializeTravelOptions(const FString& Options);
	void InitializeGameState();
	void StartServerMatchTimerIfNeeded();
	void ConfigureRewardChestSpawns();

	int32 GrantGameVictoryGoldReward(
		AController* WinnerController,
		int32 WinningTeamMemberCount);
	void HandleMatchTimerExpired();
	bool ShowGameResultForWinner(APlayerState* WinnerPlayerState);
	void NotifyPlayerKillScored(
		APlayerState* KillerPlayerState,
		APlayerState* VictimPlayerState);
	bool RequestAbortMatchToTitle(APlayerController* RequestingPlayer);
	bool AbortMatchToTitleForPlayerExit(
		const APlayerState* ExitingPlayerState);

	const UMatchRuleDefinition* GetMatchRuleDefinition() const;
	bool FindCurrentMatchMapOption(
		FLobbyMatchMapOption& OutMapOption) const;
	bool IsGameResultShown() const { return bGameResultShown; }

	int32 CalculateVictoryGoldReward(
		int32 KillCount,
		int32 DeathCount,
		int32 WinningTeamMemberCount) const;

private:
	AExperienceGameMode* GetExperienceGameMode() const;
	const AExperienceGameMode* GetExperienceGameModeConst() const;

	bool ShouldSuppressServerMatchTimerForCurrentMap() const;
	bool ShouldSuppressServerMatchTimer() const;
	bool TryFindUniqueKillLeader(
		APdPlayerState*& OutWinnerPlayerState,
		int32& OutTopKillCount,
		bool& bOutTie) const;
	bool FindTopKiller(
		APdPlayerState*& OutTopKillerPlayerState,
		int32& OutTopKillCount) const;
	bool ShouldAbortMatchForPlayerExit(
		const APlayerState* ExitingPlayerState) const;
	FString GetResolvedTitleTravelMapName() const;
	FGameResultPresentationData BuildPlayerExitGameResult(
		const APlayerState* ExitingPlayerState,
		const APlayerState* WinnerPlayerState,
		int32 WinnerTeamColorIndex,
		int32 WinnerTeamMemberCount) const;
	void SendPlayerExitGameResultToTitle(
		const FGameResultPresentationData& GameResultData);
	AController* FindControllerForPlayerState(
		const APlayerState* PlayerState) const;
	int32 CountPlayersOnTeam(int32 TeamColorIndex) const;
	int32 GrantVictoryRewardsForWinner(
		APlayerState* WinnerPlayerState,
		int32 WinnerTeamColorIndex,
		int32 WinnerTeamMemberCount);
	void ApplyVictoryRewardEligibility(
		TArray<FGameResultPlayerStat>& PlayerStats,
		const APlayerState* WinnerPlayerState,
		int32 WinnerTeamColorIndex,
		int32 WinnerTeamMemberCount) const;
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
	void ForceMovePlayersForTimerTie();
	void RaiseForceMoveGatesForTimerTie();
	void StartTimerTieNextKillWins();
	const URewardDefinition* ResolveRewardDefinitionForChestSpawns(
		const TArray<ARewardChest*>& RewardChests) const;
	void BeginRuntimeContentPreload();
	void HandleRuntimeContentPreloadComplete(uint32 RequestGeneration);
	void ReleaseRuntimeContentPreload();
	void ResumePendingInitialization();

	UPROPERTY(Transient)
	FExperienceMatchFlowSettings Settings;

	bool bMatchTimerExpired = false;
	bool bTimerTieNextKillWinsActive = false;
	bool bGameResultShown = false;
	bool bMatchTimerSuppressedByTravelOption = false;
	FTimerHandle MatchTimerHandle;
	FTimerHandle ChestConfigurationRetryTimerHandle;
	TSharedPtr<FStreamableHandle> RuntimeContentPreloadHandle;
	uint32 RuntimeContentRequestGeneration = 0;
	bool bRuntimeContentLoadPending = false;
	bool bInitializeGameStateRequested = false;
	bool bStartMatchTimerRequested = false;
	bool bConfigureRewardChestsRequested = false;
};
