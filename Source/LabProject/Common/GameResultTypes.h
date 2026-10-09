#pragma once

#include "CoreMinimal.h"
#include "GameResultTypes.generated.h"

USTRUCT(BlueprintType)
struct LABPROJECT_API FGameResultPlayerStat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult")
	FText PlayerName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult")
	FText TeamName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult")
	int32 TeamColorIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult")
	int32 PlayerStateId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult")
	int32 KillCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult")
	int32 DeathCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult")
	int32 GoldReward = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult")
	bool bVictoryRewardEligible = false;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FGameResultPresentationData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult")
	FText WinnerTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult")
	int32 WinnerTeamColorIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult")
	FText MaxKillerName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult")
	int32 MaxKillCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult")
	TArray<FGameResultPlayerStat> PlayerStats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult")
	bool bAllowLobbyTravelOnExit = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult")
	bool bShowRewards = true;

	/** 누가 나가서 경기가 끝났다. 결과 화면은 승리 팀 대신 이 사유를 제목으로 보여 준다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult")
	bool bEndedByPlayerExit = false;
};
