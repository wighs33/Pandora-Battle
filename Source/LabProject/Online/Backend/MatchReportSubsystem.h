#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MatchReportSubsystem.generated.h"

struct FMatchReportPlayer
{
	/** 백엔드 플레이어 ID(steam:<id>, dev:<id>). 비어 있으면 경기 기록에만 남고 개인 전적에는 더하지 않는다. */
	FString PlayerId;
	FString DisplayName;
	int32 Team = INDEX_NONE;
	int32 Kills = 0;
	int32 Deaths = 0;
	/** win, lose, draw */
	FString Result;
};

struct FMatchReport
{
	FString MatchId;
	FString MapKey;
	/** completed, player_exit */
	FString EndReason;
	int32 WinnerTeam = INDEX_NONE;
	TArray<FMatchReportPlayer> Players;
};

/**
 * 전용 서버의 경기 결과를 백엔드(POST /server/match-result)에 보고한다.
 *
 * 요청은 SigV4로 서명하며, 자격 증명은 GameLift 서버 서브시스템이 고른다. API 주소나 자격 증명이 없으면
 * 경고만 남기고 건너뛰므로 백엔드 없이도 전용 서버 경기는 그대로 진행된다.
 */
UCLASS()
class LABPROJECT_API UMatchReportSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// Public API ------------------------------------------------------------------------------------------------------
	void ReportMatch(const FMatchReport& Report);
	bool HasPendingReports() const { return PendingReportCount > 0; }

	/** GameLift 게임 세션은 한 경기만 진행하므로 세션 ID를, 로컬 전용 서버는 경기마다 새 ID를 쓴다. */
	FString CreateMatchId() const;

private:
	int32 PendingReportCount = 0;
};
