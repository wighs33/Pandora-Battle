#include "Lobby/Coordination/LobbyMatchCoordinator.h"
#include "Component/Lobby/LobbyPlayerStateComponent.h"
#include "Component/Player/PlayerMatchComponent.h"

#include "Component/Lobby/LobbyConfigurationComponent.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Mode/PdPlayerState.h"
#include "Lobby/Coordination/LobbyTravelCoordinator.h"
#include "Engine/GameInstance.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyMatchCoordinator)

// 로비 설정 로딩이 끝나면 선택 맵과 정원을 온라인 방 목록의 광고 정보에 반영한다.
void ULobbyMatchCoordinator::UpdateAdvertisedSessionSettingsFromLobbyConfig()
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode)
	{
		return;
	}

	const ULobbyRuntimeSubsystem* LobbySubsystem = UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GameMode->GetGameInstance());
	const FName MapKey = LobbySubsystem && !LobbySubsystem->GetLobbySelectedMapKey().IsNone()
		? LobbySubsystem->GetLobbySelectedMapKey()
		: GameMode->GetLobbyConfigurationComponent()->GetFirstMapKey();
	UpdateAdvertisedSessionSettings(MapKey, GameMode->GetLobbyConfigurationComponent()->GetConfiguredMaxPlayerCount());
}

// 로비 종료 시 남은 시작 카운트다운을 해제하고 이 로비의 시작 요청 상태를 초기화한다.
void ULobbyMatchCoordinator::Shutdown()
{
	ClearStartCountdownTimer();
	bGameStartRequested = false;
}

// 시작 버튼의 활성화와 중복 클릭 방지를 위해, 미시작 상태와 현재 로비의 경기 시작 조건을 함께 확인한다.
bool ULobbyMatchCoordinator::CanHostStartGame() const
{
	return !bGameStartRequested && AreMatchStartConditionsMet();
}

// 시작 버튼과 비동기 세션 시작 전후가 같은 기준을 쓰도록 서버 권한·콘텐츠 준비·정원·팀 균형을 검사한다.
bool ULobbyMatchCoordinator::AreMatchStartConditionsMet() const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->HasAuthority() || !GameMode->IsReadyForPlayerStart())
	{
		return false;
	}

	const int32 ActivePlayerCount = GetActiveLobbyPlayerCount();
	const int32 MaxPlayerCount = GameMode->GetLobbyConfigurationComponent()->GetConfiguredMaxPlayerCount();
	return ActivePlayerCount > 0 && ActivePlayerCount <= MaxPlayerCount && AreLobbyTeamsBalanced();
}

// 호스트의 시작 요청을 검증하고 Pawn 이동을 잠근다. 여러 명이면 카운트다운을 복제하고, 혼자이면 바로 전장 진입을 요청한다.
void ULobbyMatchCoordinator::TryStartGame()
{
	if (!CanHostStartGame())
	{
		return;
	}

	ALobbyGameMode* GameMode = GetLobbyGameMode();
	bGameStartRequested = true;
	ClearStartCountdownTimer();

	const float CountdownSeconds = GetStartCountdownSeconds(GetActiveLobbyPlayerCount());
	if (ALobbyGameState* LobbyGameState = GameMode->GetGameState<ALobbyGameState>())
	{
		LobbyGameState->SetGameStartPending(true, LobbyGameState->GetServerWorldTimeSeconds() + CountdownSeconds);
	}
	if (ULobbyTravelCoordinator* TravelCoordinator = GameMode->GetTravelCoordinator())
	{
		TravelCoordinator->SetAllLobbyPawnsTravelLocked(true);
	}

	if (CountdownSeconds > 0.0f)
	{
		GameMode->GetWorldTimerManager().SetTimer(
			StartCountdownTimerHandle, this, &ThisClass::HandleStartCountdownElapsed, CountdownSeconds, false);
		return;
	}

	HandleStartCountdownElapsed();
}

// 카운트다운이 끝나면 이동 코디네이터에 온라인 세션 시작과 전장 진입을 요청한다.
void ULobbyMatchCoordinator::HandleStartCountdownElapsed()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (GameMode && GameMode->GetTravelCoordinator())
	{
		GameMode->GetTravelCoordinator()->StartSessionAndTravel();
	}
}

// 혼자 입장하면 대기 시간을 없애고, 여러 명이면 매치 규칙에 설정된 로비 시작 카운트다운을 적용한다.
float ULobbyMatchCoordinator::GetStartCountdownSeconds(const int32 ActivePlayerCount) const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const UMatchRuleDefinition* MatchRules = GameMode ? GameMode->GetLobbyConfigurationComponent()->GetMatchRuleDefinition() : nullptr;
	const float CountdownSeconds =
		MatchRules ? MatchRules->LobbyStartCountdownSeconds : GetDefault<UMatchRuleDefinition>()->LobbyStartCountdownSeconds;
	return ActivePlayerCount == 1 ? 0.0f : FMath::Max(CountdownSeconds, 0.0f);
}

// 카운트다운이나 전장 진입 준비 중 팀이 바뀌면 기존 시작 요청을 취소해 변경된 팀으로 다시 시작하게 한다.
void ULobbyMatchCoordinator::NotifyLobbyTeamChanged()
{
	if (bGameStartRequested)
	{
		CancelPendingGameStart();
	}
}

// 팀·맵 변경, 강퇴 또는 진입 실패 시 시작 예약과 이동 준비를 취소하고, 복제된 카운트다운과 Pawn 이동 잠금을 해제한다.
void ULobbyMatchCoordinator::CancelPendingGameStart()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->HasAuthority())
	{
		return;
	}

	ClearStartCountdownTimer();
	bGameStartRequested = false;
	if (ALobbyGameState* LobbyGameState = GameMode->GetGameState<ALobbyGameState>())
	{
		LobbyGameState->SetGameStartPending(false, 0.0);
	}
	if (ULobbyTravelCoordinator* TravelCoordinator = GameMode->GetTravelCoordinator())
	{
		TravelCoordinator->CancelPendingTravel();
		TravelCoordinator->SetAllLobbyPawnsTravelLocked(false);
	}
}

// 시작 취소 또는 로비 종료 후 카운트다운 콜백이 전장 진입을 요청하지 않도록 타이머를 해제한다.
void ULobbyMatchCoordinator::ClearStartCountdownTimer()
{
	if (ALobbyGameMode* GameMode = GetLobbyGameMode())
	{
		GameMode->GetWorldTimerManager().ClearTimer(StartCountdownTimerHandle);
	}
}

// 퇴장 처리 중인 플레이어를 제외한 로비 인원을 세어 정원 검증과 혼자 입장하는 경기의 규칙에 사용한다.
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
		const APdPlayerState* LobbyPlayerState = Cast<APdPlayerState>(PlayerState);
		if (LobbyPlayerState && !LobbyPlayerState->GetLobbyPlayerStateComponent()->IsLeavingLobby())
		{
			++ActivePlayerCount;
		}
	}

	return ActivePlayerCount;
}

// 퇴장 중인 플레이어를 제외하고 팀별 인원이 같은지 검사한다. 여러 명의 경기는 미지정 팀 없이 두 팀 이상이어야 한다.
bool ULobbyMatchCoordinator::AreLobbyTeamsBalanced() const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->GameState)
	{
		return false;
	}

	int32 ActivePlayerCount = 0;
	TMap<int32, int32> PlayerCountsByTeamColor;
	bool bHasUnassignedTeam = false;
	for (APlayerState* PlayerState : GameMode->GameState->PlayerArray)
	{
		const APdPlayerState* LobbyPlayerState = Cast<APdPlayerState>(PlayerState);
		if (!LobbyPlayerState || LobbyPlayerState->GetLobbyPlayerStateComponent()->IsLeavingLobby())
		{
			continue;
		}

		++ActivePlayerCount;
		const int32 TeamColorIndex = LobbyPlayerState->GetPlayerMatchComponent()->GetMatchTeamColorIndex();
		if (TeamColorIndex == INDEX_NONE)
		{
			bHasUnassignedTeam = true;
		}
		else
		{
			++PlayerCountsByTeamColor.FindOrAdd(TeamColorIndex);
		}
	}

	// 혼자 입장할 때에는 팀 지정 여부와 관계없이 연습 경기를 허용한다.
	if (ActivePlayerCount == 1)
	{
		return true;
	}
	if (ActivePlayerCount < 2 || PlayerCountsByTeamColor.Num() < 2 || bHasUnassignedTeam)
	{
		return false;
	}

	const int32 PlayersPerTeam = ActivePlayerCount / PlayerCountsByTeamColor.Num();
	for (const TPair<int32, int32>& TeamPlayerCount : PlayerCountsByTeamColor)
	{
		if (TeamPlayerCount.Value != PlayersPerTeam)
		{
			return false;
		}
	}
	return true;
}

// 입장 플레이어에게 팀 색상이 없으면 스폰 번호를 우선 사용하고, 번호도 없으면 비어 있는 팀 색상을 배정한다.
void ULobbyMatchCoordinator::AssignLobbyTeamColorIfNeeded(APdPlayerState* LobbyPlayerState) const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !LobbyPlayerState || LobbyPlayerState->GetPlayerMatchComponent()->GetMatchTeamColorIndex() != INDEX_NONE)
	{
		return;
	}

	int32 TeamColorIndex = LobbyPlayerState->GetPlayerMatchComponent()->GetMatchSpawnIndex();
	if (TeamColorIndex == INDEX_NONE)
	{
		TeamColorIndex = FindAvailableLobbyTeamColorIndex(LobbyPlayerState);
	}

	TeamColorIndex = FMath::Clamp(TeamColorIndex, 0, GameMode->GetLobbyConfigurationComponent()->GetConfiguredMaxPlayerCount() - 1);
	LobbyPlayerState->GetPlayerMatchComponent()->SetMatchTeamColorIndex(TeamColorIndex);
}

// 대상 플레이어와 퇴장 중인 플레이어를 제외하고 사용하지 않는 팀 색상 번호를 찾는다. 빈 번호가 없으면 0을 사용한다.
int32 ULobbyMatchCoordinator::FindAvailableLobbyTeamColorIndex(const APdPlayerState* IgnoredPlayerState) const
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
			const APdPlayerState* LobbyPlayerState = Cast<APdPlayerState>(PlayerState);
			if (!LobbyPlayerState || LobbyPlayerState == IgnoredPlayerState
				|| LobbyPlayerState->GetLobbyPlayerStateComponent()->IsLeavingLobby())
			{
				continue;
			}

			const int32 TeamColorIndex = LobbyPlayerState->GetPlayerMatchComponent()->GetMatchTeamColorIndex();
			if (TeamColorIndex != INDEX_NONE)
			{
				UsedTeamColorIndices.Add(TeamColorIndex);
			}
		}
	}

	for (int32 TeamColorIndex = 0; TeamColorIndex < GameMode->GetLobbyConfigurationComponent()->GetConfiguredMaxPlayerCount();
		++TeamColorIndex)
	{
		if (!UsedTeamColorIndices.Contains(TeamColorIndex))
		{
			return TeamColorIndex;
		}
	}

	return 0;
}

// 호스트가 고른 맵과 정원을 검색 가능한 온라인 세션에 갱신한다. 맵 미지정 시 첫 맵, 맵 목록도 없으면 Lobby를 표시한다.
void ULobbyMatchCoordinator::UpdateAdvertisedSessionSettings(const FName SessionMapKey, const int32 MaxPlayerCount) const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->HasAuthority())
	{
		return;
	}

	UOnlineSessionsSubsystem* OnlineSessionsSubsystem = UGameInstance::GetSubsystem<UOnlineSessionsSubsystem>(GameMode->GetGameInstance());
	if (!OnlineSessionsSubsystem || !OnlineSessionsSubsystem->HasNamedSession())
	{
		return;
	}

	const FName MapKey = SessionMapKey.IsNone() ? GameMode->GetLobbyConfigurationComponent()->GetFirstMapKey() : SessionMapKey;
	const FString MapName = MapKey.IsNone() ? FString(TEXT("Lobby")) : MapKey.ToString();
	OnlineSessionsSubsystem->UpdateSessionSettings(MapName, MaxPlayerCount, true);
}

// 이 코디네이터를 생성한 로비 GameMode를 가져와 서버의 로비 설정과 참가자 상태에 접근한다.
ALobbyGameMode* ULobbyMatchCoordinator::GetLobbyGameMode() const
{
	return Cast<ALobbyGameMode>(GetOuter());
}
