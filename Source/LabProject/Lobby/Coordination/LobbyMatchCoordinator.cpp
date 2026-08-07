#include "Lobby/Coordination/LobbyMatchCoordinator.h"

#include "Definition/Match/MatchRuleDefinition.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Lobby/Contents/LobbyPlayerState.h"
#include "Lobby/Coordination/LobbyTravelCoordinator.h"
#include "Mode/PdGameInstance.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyMatchCoordinator)

void ULobbyMatchCoordinator::InitializeSession()
{
	UpdateAdvertisedSessionSettingsForCurrentConfig();
}

void ULobbyMatchCoordinator::Shutdown()
{
	ClearStartTimers();
	bGameStartRequested = false;
}

void ULobbyMatchCoordinator::TryStartGame()
{
	if (CanHostStartGame())
	{
		BeginStartGame(TEXT("host_manual"));
	}
}

bool ULobbyMatchCoordinator::CanHostStartGame() const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode)
	{
		return false;
	}

	const int32 ActivePlayerCount = GetActiveLobbyPlayerCount();
	const int32 MaxPlayerCount = GameMode->GetConfiguredMaxPlayerCount();
	return GameMode->HasAuthority()
		&& !bGameStartRequested
		&& ActivePlayerCount > 0
		&& ActivePlayerCount <= MaxPlayerCount
		&& AreLobbyTeamsBalanced();
}

bool ULobbyMatchCoordinator::AreLobbyTeamsBalanced() const
{
	int32 ActivePlayerCount = 0;
	int32 TeamCount = 0;
	int32 PlayersPerTeam = 0;
	return GetLobbyTeamBalanceStatus(ActivePlayerCount, TeamCount, PlayersPerTeam);
}

void ULobbyMatchCoordinator::NotifyLobbyTeamChanged()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->HasAuthority())
	{
		return;
	}

	if (bGameStartRequested)
	{
		CancelPendingGameStart(TEXT("team_changed"));
	}

	GameMode->RefreshLobbyUIForAllPlayers();
}

void ULobbyMatchCoordinator::BeginStartGame(const TCHAR* Reason)
{
	static_cast<void>(Reason);

	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->HasAuthority() || bGameStartRequested)
	{
		return;
	}

	const int32 ActivePlayerCount = GetActiveLobbyPlayerCount();
	const int32 MaxPlayerCount = FMath::Max(
		GameMode->GetConfiguredMaxPlayerCount(),
		1);
	if (ActivePlayerCount <= 0 || ActivePlayerCount > MaxPlayerCount)
	{
		GameMode->RefreshLobbyUIForAllPlayers();
		return;
	}

	if (!AreLobbyTeamsBalanced())
	{
		GameMode->RefreshLobbyUIForAllPlayers();
		return;
	}

	bGameStartRequested = true;
	ClearStartTimers();

	const float EffectiveStartGameDelay = GetEffectiveStartGameDelay(ActivePlayerCount);
	if (ALobbyGameState* LobbyGameState = GameMode->GetGameState<ALobbyGameState>())
	{
		LobbyGameState->SetGameStartPending(
			true,
			LobbyGameState->GetServerWorldTimeSeconds()
				+ FMath::Max(EffectiveStartGameDelay, 0.0f));
	}

	if (GameMode->TravelCoordinator)
	{
		GameMode->TravelCoordinator->SetAllLobbyPawnsTravelLocked(true);
	}
	GameMode->RefreshLobbyUIForAllPlayers();

	if (EffectiveStartGameDelay > 0.0f)
	{
		GameMode->GetWorldTimerManager().SetTimer(
			StartGameTimerHandle,
			this,
			&ThisClass::HandleStartCountdownElapsed,
			EffectiveStartGameDelay,
			false);
		return;
	}

	HandleStartCountdownElapsed();
}

void ULobbyMatchCoordinator::CancelPendingGameStart(const TCHAR* Reason)
{
	static_cast<void>(Reason);

	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->HasAuthority())
	{
		return;
	}

	ClearStartTimers();
	bGameStartRequested = false;

	if (ALobbyGameState* LobbyGameState = GameMode->GetGameState<ALobbyGameState>())
	{
		LobbyGameState->SetGameStartPending(false, 0.0);
	}

	if (GameMode->TravelCoordinator)
	{
		GameMode->TravelCoordinator->CancelPendingTravel();
		GameMode->TravelCoordinator->SetAllLobbyPawnsTravelLocked(false);
	}
	GameMode->RefreshLobbyUIForAllPlayers();
}

void ULobbyMatchCoordinator::ClearStartTimers()
{
	if (ALobbyGameMode* GameMode = GetLobbyGameMode())
	{
		GameMode->GetWorldTimerManager().ClearTimer(StartGameTimerHandle);
	}
}

int32 ULobbyMatchCoordinator::GetActiveLobbyPlayerCount() const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->GameState)
	{
		return 0;
	}

	int32 ActivePlayerCount = 0;
	for (APlayerState* PlayerState : GameMode->GameState->PlayerArray)
	{
		const ALobbyPlayerState* LobbyPlayerState = Cast<ALobbyPlayerState>(PlayerState);
		if (LobbyPlayerState && !LobbyPlayerState->IsLeavingLobby())
		{
			++ActivePlayerCount;
		}
	}

	return ActivePlayerCount;
}

void ULobbyMatchCoordinator::UpdateAdvertisedSessionSettings(
	const FName SessionMapKey,
	const int32 MaxPlayerCount) const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->HasAuthority())
	{
		return;
	}

	UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GameMode->GetGameInstance()
		? GameMode->GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr;
	if (!OnlineSessionsSubsystem || !OnlineSessionsSubsystem->HasNamedSession())
	{
		return;
	}

	const FString SessionMapName = SessionMapKey.IsNone()
		? GetInitialSessionMapName()
		: SessionMapKey.ToString();
	OnlineSessionsSubsystem->UpdateSessionSettings(SessionMapName, MaxPlayerCount, true);
}

void ULobbyMatchCoordinator::AssignLobbyTeamColorIfNeeded(ALobbyPlayerState* LobbyPlayerState) const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !LobbyPlayerState || LobbyPlayerState->GetTeamColorIndex() != INDEX_NONE)
	{
		return;
	}

	int32 TeamColorIndex =
		LobbyPlayerState->GetPlayerMatchComponent()->GetMatchSpawnIndex();
	if (TeamColorIndex == INDEX_NONE)
	{
		TeamColorIndex = FindAvailableLobbyTeamColorIndex(LobbyPlayerState);
	}

	TeamColorIndex = FMath::Clamp(TeamColorIndex, 0, GameMode->GetConfiguredMaxPlayerCount() - 1);
	LobbyPlayerState->SetTeamColorIndex(TeamColorIndex);
}

ALobbyGameMode* ULobbyMatchCoordinator::GetLobbyGameMode() const
{
	return Cast<ALobbyGameMode>(GetOuter());
}

void ULobbyMatchCoordinator::HandleStartCountdownElapsed()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (GameMode && GameMode->TravelCoordinator)
	{
		GameMode->TravelCoordinator->StartSessionAndTravel();
	}
}

float ULobbyMatchCoordinator::GetEffectiveStartGameDelay(const int32 ActivePlayerCount) const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const UMatchRuleDefinition* MatchRules = GameMode ? GameMode->GetMatchRuleDefinition() : nullptr;
	const float CountdownSeconds = MatchRules
		? MatchRules->LobbyStartCountdownSeconds
		: GetDefault<UMatchRuleDefinition>()->LobbyStartCountdownSeconds;
	return ActivePlayerCount == 1 ? 0.0f : FMath::Max(CountdownSeconds, 0.0f);
}

bool ULobbyMatchCoordinator::GetLobbyTeamBalanceStatus(
	int32& OutActivePlayerCount,
	int32& OutTeamCount,
	int32& OutPlayersPerTeam) const
{
	OutActivePlayerCount = 0;
	OutTeamCount = 0;
	OutPlayersPerTeam = 0;

	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->GameState)
	{
		return false;
	}

	TMap<int32, int32> TeamCounts;
	bool bHasUnassignedTeam = false;
	for (APlayerState* PlayerState : GameMode->GameState->PlayerArray)
	{
		const ALobbyPlayerState* LobbyPlayerState = Cast<ALobbyPlayerState>(PlayerState);
		if (!LobbyPlayerState || LobbyPlayerState->IsLeavingLobby())
		{
			continue;
		}

		++OutActivePlayerCount;
		const int32 TeamColorIndex = LobbyPlayerState->GetTeamColorIndex();
		if (TeamColorIndex == INDEX_NONE)
		{
			bHasUnassignedTeam = true;
			continue;
		}

		TeamCounts.FindOrAdd(TeamColorIndex)++;
	}

	OutTeamCount = TeamCounts.Num();
	if (OutActivePlayerCount == 1)
	{
		OutPlayersPerTeam = TeamCounts.Num() > 0 ? 1 : 0;
		return true;
	}

	if (OutActivePlayerCount < 2 || OutTeamCount < 2 || bHasUnassignedTeam)
	{
		return false;
	}

	bool bHasExpectedCount = false;
	for (const TPair<int32, int32>& TeamCountPair : TeamCounts)
	{
		if (!bHasExpectedCount)
		{
			OutPlayersPerTeam = TeamCountPair.Value;
			bHasExpectedCount = true;
			continue;
		}

		if (TeamCountPair.Value != OutPlayersPerTeam)
		{
			return false;
		}
	}

	return bHasExpectedCount && OutPlayersPerTeam > 0;
}

void ULobbyMatchCoordinator::UpdateAdvertisedSessionSettingsForCurrentConfig() const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode)
	{
		return;
	}

	const UPdGameInstance* PdGameInstance = GameMode->GetGameInstance<UPdGameInstance>();
	const FName SessionMapKey = PdGameInstance && !PdGameInstance->GetLobbySelectedMapKey().IsNone()
		? PdGameInstance->GetLobbySelectedMapKey()
		: GameMode->GetFirstMapKey();
	UpdateAdvertisedSessionSettings(SessionMapKey, GameMode->GetConfiguredMaxPlayerCount());
}

FString ULobbyMatchCoordinator::GetInitialSessionMapName() const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const FName FirstMapKey = GameMode ? GameMode->GetFirstMapKey() : NAME_None;
	return FirstMapKey.IsNone() ? FString(TEXT("Lobby")) : FirstMapKey.ToString();
}

int32 ULobbyMatchCoordinator::FindAvailableLobbyTeamColorIndex(
	const ALobbyPlayerState* IgnoredPlayerState) const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode)
	{
		return 0;
	}

	TSet<int32> UsedTeamColorIndices;
	if (GameMode->GameState)
	{
		for (APlayerState* PlayerState : GameMode->GameState->PlayerArray)
		{
			const ALobbyPlayerState* LobbyPlayerState = Cast<ALobbyPlayerState>(PlayerState);
			if (!LobbyPlayerState || LobbyPlayerState == IgnoredPlayerState || LobbyPlayerState->IsLeavingLobby())
			{
				continue;
			}

			const int32 TeamColorIndex = LobbyPlayerState->GetTeamColorIndex();
			if (TeamColorIndex != INDEX_NONE)
			{
				UsedTeamColorIndices.Add(TeamColorIndex);
			}
		}
	}

	for (int32 TeamColorIndex = 0; TeamColorIndex < GameMode->GetConfiguredMaxPlayerCount(); ++TeamColorIndex)
	{
		if (!UsedTeamColorIndices.Contains(TeamColorIndex))
		{
			return TeamColorIndex;
		}
	}

	return 0;
}
