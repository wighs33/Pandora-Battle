#include "Component/Match/MatchResultReport.h"

#include "Component/Match/MatchOutcomeRules.h"
#include "Component/Match/MatchPlayerSetupComponent.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Definition/Level/LevelDefinition.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameStateBase.h"
#include "Mode/ExperienceGameMode.h"
#include "Mode/PdPlayerState.h"
#include "Online/Backend/MatchReportSubsystem.h"

namespace
{
	// 백엔드가 받는 맵 키 문자 집합([A-Za-z0-9_.-], 최대 64자)으로 맞춘다.
	FString ToReportMapKey(const FString& MapKey)
	{
		FString Sanitized = MapKey.Left(64);
		for (TCHAR& Character : Sanitized)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('.') && Character != TEXT('-'))
			{
				Character = TEXT('_');
			}
		}
		return Sanitized;
	}
}

FText MatchResultReport::ResolveTeamName(const int32 TeamColorIndex)
{
	switch (TeamColorIndex)
	{
	case 0:
		return NSLOCTEXT("GameResult", "TeamNameRed", "Red");
	case 1:
		return NSLOCTEXT("GameResult", "TeamNameBlue", "Blue");
	case 2:
		return NSLOCTEXT("GameResult", "TeamNameYellow", "Yellow");
	case 3:
		return NSLOCTEXT("GameResult", "TeamNamePurple", "Purple");
	case 4:
		return NSLOCTEXT("GameResult", "TeamNameGreen", "Green");
	case 5:
		return NSLOCTEXT("GameResult", "TeamNameOrange", "Orange");
	default:
		return NSLOCTEXT("GameResult", "TeamNameNone", "No Team");
	}
}

void MatchResultReport::SortPlayerStats(TArray<FGameResultPlayerStat>& PlayerStats)
{
	PlayerStats.Sort([](const FGameResultPlayerStat& A, const FGameResultPlayerStat& B)
	{
		if (A.KillCount != B.KillCount)
		{
			return A.KillCount > B.KillCount;
		}
		if (A.DeathCount != B.DeathCount)
		{
			return A.DeathCount < B.DeathCount;
		}
		return A.PlayerName.ToString() < B.PlayerName.ToString();
	});
}

FText MatchResultReport::ResolveWinnerTitle(const int32 WinnerTeamColorIndex)
{
	return FText::Format(
		NSLOCTEXT("GameResult", "WinnerTeamTitleFormat", "{0} Team Wins"),
		ResolveTeamName(WinnerTeamColorIndex));
}

TArray<FGameResultPlayerStat> MatchResultReport::BuildPlayerStats(const AGameStateBase& GameState)
{
	TArray<FGameResultPlayerStat> PlayerStats;
	for (const APlayerState* PlayerState : GameState.PlayerArray)
	{
		const APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState);
		if (!PdPlayerState)
		{
			continue;
		}

		const UPlayerMatchComponent* MatchComponent = PdPlayerState->GetPlayerMatchComponent();
		FGameResultPlayerStat& PlayerStat = PlayerStats.AddDefaulted_GetRef();
		PlayerStat.PlayerName = UPlayerMatchComponent::ResolveDisplayName(PdPlayerState);
		PlayerStat.TeamColorIndex = MatchComponent->GetMatchTeamColorIndex();
		PlayerStat.PlayerStateId = PdPlayerState->GetPlayerId();
		PlayerStat.TeamName = ResolveTeamName(PlayerStat.TeamColorIndex);
		PlayerStat.KillCount = MatchComponent->GetKillCount();
		PlayerStat.DeathCount = MatchComponent->GetDeathCount();
	}

	SortPlayerStats(PlayerStats);
	return PlayerStats;
}

void MatchResultReport::ApplyVictoryRewards(
	TArray<FGameResultPlayerStat>& PlayerStats,
	const APlayerState* WinnerPlayerState,
	const int32 WinnerTeamColorIndex,
	const int32 WinnerTeamMemberCount,
	const APlayerState* ExcludedPlayerState,
	const FVictoryGoldRates& Rates)
{
	const int32 ExcludedPlayerStateId = ExcludedPlayerState ? ExcludedPlayerState->GetPlayerId() : INDEX_NONE;
	const FText ExcludedPlayerName = UPlayerMatchComponent::ResolveDisplayName(ExcludedPlayerState);
	const FText WinnerPlayerName = UPlayerMatchComponent::ResolveDisplayName(WinnerPlayerState);
	for (FGameResultPlayerStat& PlayerStat : PlayerStats)
	{
		const bool bIsExcludedPlayer = ExcludedPlayerState
			&& (ExcludedPlayerStateId != INDEX_NONE
				? PlayerStat.PlayerStateId == ExcludedPlayerStateId
				: PlayerStat.PlayerName.EqualTo(ExcludedPlayerName));
		const bool bIsWinningTeamMember = WinnerTeamColorIndex != INDEX_NONE
			&& PlayerStat.TeamColorIndex == WinnerTeamColorIndex;
		const bool bIsSoloWinner = WinnerTeamColorIndex == INDEX_NONE
			&& PlayerStat.PlayerName.EqualTo(WinnerPlayerName);
		PlayerStat.bVictoryRewardEligible = !bIsExcludedPlayer && (bIsWinningTeamMember || bIsSoloWinner);
		PlayerStat.GoldReward = PlayerStat.bVictoryRewardEligible
			? MatchOutcomeRules::CalculateVictoryGold(
				PlayerStat.KillCount,
				PlayerStat.DeathCount,
				WinnerTeamMemberCount,
				Rates)
			: 0;
	}
}

FGameResultPresentationData MatchResultReport::BuildPlayerExitResult(
	const AGameStateBase& GameState,
	const APlayerState* ExitingPlayerState,
	const APlayerState* WinnerPlayerState,
	const int32 WinnerTeamColorIndex,
	const int32 WinnerTeamMemberCount,
	const FVictoryGoldRates& Rates)
{
	FGameResultPresentationData GameResultData;
	GameResultData.WinnerTitle = NSLOCTEXT("GameResult", "MatchEndedByPlayerExit", "Match Ended Due to Player Leaving");
	GameResultData.WinnerTeamColorIndex = WinnerTeamColorIndex;
	GameResultData.bAllowLobbyTravelOnExit = false;
	GameResultData.bShowRewards = WinnerPlayerState != nullptr;
	GameResultData.PlayerStats = BuildPlayerStats(GameState);
	if (WinnerPlayerState)
	{
		ApplyVictoryRewards(
			GameResultData.PlayerStats,
			WinnerPlayerState,
			WinnerTeamColorIndex,
			WinnerTeamMemberCount,
			ExitingPlayerState,
			Rates);
	}

	if (!GameResultData.PlayerStats.IsEmpty())
	{
		GameResultData.MaxKillerName = GameResultData.PlayerStats[0].PlayerName;
		GameResultData.MaxKillCount = GameResultData.PlayerStats[0].KillCount;
	}
	else
	{
		GameResultData.MaxKillerName = UPlayerMatchComponent::ResolveDisplayName(ExitingPlayerState);
	}
	return GameResultData;
}

void MatchResultReport::ReportToBackend(
	const AExperienceGameMode& GameMode,
	const APlayerState* WinnerPlayerState,
	const int32 WinnerTeamColorIndex,
	const TCHAR* EndReason,
	const APlayerState* ExitingPlayerState)
{
	const UGameInstance* GameInstance = GameMode.GetGameInstance();
	UMatchReportSubsystem* Reports = GameInstance ? GameInstance->GetSubsystem<UMatchReportSubsystem>() : nullptr;
	const AGameStateBase* GameState = GameMode.GetGameState<AGameStateBase>();
	if (!Reports || !GameState || GameMode.GetPlayerSetupComponent()->IsTrainingRoomMap())
	{
		return;
	}

	FMatchReport Report;
	Report.MatchId = Reports->CreateMatchId();
	Report.EndReason = EndReason;
	Report.WinnerTeam = WinnerTeamColorIndex;
	FLobbyMatchMapOption MapOption;
	if (GameMode.FindCurrentMatchMapOption(MapOption))
	{
		Report.MapKey = ToReportMapKey(MapOption.MapKey.ToString());
	}

	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		const APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState);
		if (!PdPlayerState || PdPlayerState->IsABot() || PdPlayerState->IsOnlyASpectator())
		{
			continue;
		}

		const UPlayerMatchComponent* MatchComponent = PdPlayerState->GetPlayerMatchComponent();
		FMatchReportPlayer& Player = Report.Players.AddDefaulted_GetRef();
		Player.PlayerId = PdPlayerState->GetBackendPlayerId();
		Player.DisplayName = UPlayerMatchComponent::ResolveDisplayName(PdPlayerState).ToString();
		Player.Team = MatchComponent->GetMatchTeamColorIndex();
		Player.Kills = MatchComponent->GetKillCount();
		Player.Deaths = MatchComponent->GetDeathCount();
		if (PlayerState == ExitingPlayerState)
		{
			Player.Result = TEXT("lose");
		}
		else if (WinnerTeamColorIndex != INDEX_NONE)
		{
			Player.Result = Player.Team == WinnerTeamColorIndex ? TEXT("win") : TEXT("lose");
		}
		else if (WinnerPlayerState)
		{
			Player.Result = PlayerState == WinnerPlayerState ? TEXT("win") : TEXT("lose");
		}
		else
		{
			Player.Result = TEXT("draw");
		}
	}

	if (!Report.Players.IsEmpty())
	{
		Reports->ReportMatch(Report);
	}
}
