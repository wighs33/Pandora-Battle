#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "TimerManager.h"
#include "MatchFlowComponent.generated.h"

class AExperienceGameMode;
class APdPlayerState;
class APlayerController;
class APlayerState;

/**
 * 서버 권한으로 경기 진행 단계를 조율한다: 경기 타이머, 승패 판정 시점, 골든킬, 결과 표시, 로비 복귀, 중도 이탈 종료.
 *
 * 승패 규칙은 MatchOutcomeRules, 보상 지급은 UMatchRewardComponent, 결과 데이터와 전적 보고는 MatchResultReport,
 * 참가자 이동은 MatchTravel이 맡는다. 이 컴포넌트는 언제 무엇을 부를지만 정한다.
 */
UCLASS(ClassGroup = (Match))
class LABPROJECT_API UMatchFlowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Public API ------------------------------------------------------------------------------------------------------
	UMatchFlowComponent();

	void InitializeTravelOptions(const FString& Options);
	void InitializeGameState();
	void StartServerMatchTimerIfNeeded();

	bool RequestAbortMatchToTitle(APlayerController* RequestingPlayer);
	bool HandlePlayerLogout(const APlayerState* ExitingPlayerState);

	bool IsGameResultShown() const { return bGameResultShown; }
	/** travel 옵션 BossRaid로 연 보스 레이드 월드. 경기 타이머·결과·이탈 종료가 없다. */
	bool IsBossRaid() const { return bBossRaid; }

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleMatchTimerExpired();
	void HandlePlayerKillScored(APlayerState* KillerPlayerState, APlayerState* VictimPlayerState);

	// Match Phases ----------------------------------------------------------------------------------------------------
	void StartGoldenKill();
	void ShowGameResult(
		APdPlayerState* WinnerPlayerState,
		int32 WinnerTeamMemberCount,
		const APdPlayerState* TopScorerPlayerState,
		int32 TopScore);
	bool AbortMatchToTitleForPlayerExit(const APlayerState* ExitingPlayerState);
	void ReturnToLobbyAfterGameResult();
	void FinishMatchRuntime();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	AExperienceGameMode* GetExperienceGameMode() const;
	bool ShouldSuppressServerMatchTimer() const;
	bool ShouldAbortMatchForPlayerExit(const APlayerState* ExitingPlayerState) const;
	FString GetTitleMapName() const;

private:
	bool bServerMatchTimerStarted = false;
	bool bMatchTimerExpired = false;
	bool bGoldenKillActive = false;
	bool bGameResultShown = false;
	bool bMatchTimerSuppressedByTravelOption = false;
	bool bBossRaid = false;
	FTimerHandle MatchTimerHandle;
	FTimerHandle GameResultLobbyReturnTimerHandle;
	FDelegateHandle KillScoredHandle;
};
