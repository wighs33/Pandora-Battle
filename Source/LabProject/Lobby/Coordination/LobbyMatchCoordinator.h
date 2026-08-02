#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LobbyMatchCoordinator.generated.h"

class ALobbyGameMode;
class ALobbyPlayerState;

UCLASS(Transient)
class LABPROJECT_API ULobbyMatchCoordinator : public UObject
{
	GENERATED_BODY()

public:
	void InitializeSession();
	void Shutdown();

	void TryStartGame();
	bool CanHostStartGame() const;
	bool AreLobbyTeamsBalanced() const;
	void NotifyLobbyTeamChanged();
	void BeginStartGame(const TCHAR* Reason);
	void CancelPendingGameStart(const TCHAR* Reason);
	void UpdateFullLobbyAutoStartTimer();
	void ClearStartTimers();

	bool IsGameStartRequested() const { return bGameStartRequested; }
	bool IsFullLobbyAutoStartTimerActive() const;
	int32 GetActiveLobbyPlayerCount() const;

	void UpdateAdvertisedSessionSettings(FName SessionMapKey, int32 MaxPlayerCount) const;
	void AssignLobbyTeamColorIfNeeded(ALobbyPlayerState* LobbyPlayerState) const;

private:
	ALobbyGameMode* GetLobbyGameMode() const;
	void HandleStartCountdownElapsed();
	void HandleFullLobbyAutoStart();
	float GetEffectiveStartGameDelay(int32 ActivePlayerCount) const;
	bool GetLobbyTeamBalanceStatus(
		int32& OutActivePlayerCount,
		int32& OutTeamCount,
		int32& OutPlayersPerTeam) const;
	void UpdateAdvertisedSessionSettingsForCurrentConfig() const;
	void CreateDedicatedServerSessionIfNeeded();
	FString GetInitialSessionMapName() const;
	int32 FindAvailableLobbyTeamColorIndex(const ALobbyPlayerState* IgnoredPlayerState) const;

	FTimerHandle StartGameTimerHandle;
	FTimerHandle FullLobbyAutoStartTimerHandle;
	bool bGameStartRequested = false;
};
