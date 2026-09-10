#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LobbyMatchCoordinator.generated.h"

class ALobbyGameMode;
class APdPlayerState;

UCLASS(Transient)
class LABPROJECT_API ULobbyMatchCoordinator : public UObject
{
	GENERATED_BODY()

public:
	// 호스트의 시작 요청과 팀·맵·인원 변경에 따른 취소.
	void TryStartGame();
	bool CanHostStartGame() const;
	bool AreMatchStartConditionsMet() const;
	void NotifyLobbyTeamChanged();
	void CancelPendingGameStart();
	bool IsGameStartRequested() const
	{
		return bGameStartRequested;
	}
	int32 GetActiveLobbyPlayerCount() const;

	// 로비 입장 시 팀 배정과 온라인 방 목록의 맵·정원 갱신.
	void AssignLobbyTeamColorIfNeeded(APdPlayerState* LobbyPlayerState) const;
	void UpdateAdvertisedSessionSettingsFromLobbyConfig();
	void UpdateAdvertisedSessionSettings(FName SessionMapKey, int32 MaxPlayerCount) const;
	void Shutdown();

private:
	ALobbyGameMode* GetLobbyGameMode() const;
	void HandleStartCountdownElapsed();
	float GetStartCountdownSeconds(int32 ActivePlayerCount) const;
	void ClearStartCountdownTimer();
	bool AreLobbyTeamsBalanced() const;
	int32 FindAvailableLobbyTeamColorIndex(const APdPlayerState* IgnoredPlayerState) const;

	FTimerHandle StartCountdownTimerHandle;
	// 카운트다운 이후 온라인 세션 시작·콘텐츠 준비·맵 이동 대기까지 포함한다.
	bool bGameStartRequested = false;
};
