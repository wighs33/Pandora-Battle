#include "Online/OnlineSessionsSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetDriver.h"
#include "Definition/Level/LevelDefinition.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Mode/ExperienceGameState.h"
#include "Engine/GameInstance.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Mode/PdPlayerState.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "TimerManager.h"
#include "UI/GameResultTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OnlineSessionsSubsystem)

namespace LabOnlineSession
{
const FName RoomNameSettingKey(TEXT("ROOM_NAME"));
const FName MapNameSettingKey(TEXT("MAP_NAME"));
} // namespace LabOnlineSession

// 게임 인스턴스가 시작될 때 현재 월드의 온라인 세션 인터페이스와 연결 끊김 알림을 연결한다.
void UOnlineSessionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	IOnlineSubsystem* OnlineSubsystem = GetOnlineSubsystemForWorld();
	if (!OnlineSubsystem)
	{

		return;
	}

	SessionInterface = OnlineSubsystem->GetSessionInterface();

	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
	}
}

// 게임 인스턴스가 종료될 때 엔진 알림·세션 콜백·타이머·요청 데이터를 해제한다. 온라인 방 삭제를 요청하는 함수는 아니다.
void UOnlineSessionsSubsystem::Deinitialize()
{
	if (GEngine)
	{
		if (NetworkFailureHandle.IsValid())
		{
			GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
			NetworkFailureHandle.Reset();
		}
	}

	ClearSessionDelegates();
	ResetActiveSessionRequest();
	SessionInterface.Reset();
	Super::Deinitialize();
}

// 방 생성 팝업의 이름·맵·정원을 검증해 생성 요청을 시작한다. 반환 ID가 0이면 시작 실패이며, 완료 후 로비 이동은 UI가 결정한다.
uint64 UOnlineSessionsSubsystem::BeginCreateRoomSession(ULocalPlayer* RequestingLocalPlayer, const FString& RoomName,
	const FString& MapName, const int32 NumPublicConnections, const bool bIsLAN)
{
	const uint64 RequestId = BeginSessionRequest(RequestingLocalPlayer, ESessionRequestKind::CreateRoom);
	if (RequestId == 0)
	{
		return 0;
	}
	if (SessionInterface->GetNamedSession(NAME_GameSession))
	{
		ResetActiveSessionRequest();
		return 0;
	}

	PendingRoomName = RoomName.TrimStartAndEnd().IsEmpty() ? TEXT("Room") : RoomName.TrimStartAndEnd();
	PendingMapName = MapName.TrimStartAndEnd().IsEmpty() ? TEXT("Unknown") : MapName.TrimStartAndEnd();
	PendingNumPublicConnections = FMath::Max(NumPublicConnections, 1);
	bPendingIsLAN = ShouldUseLANSession(bIsLAN);

	if (!StartCreateRoomPhase(RequestId))
	{
		ResetActiveSessionRequest();
		return 0;
	}

	return RequestId;
}

// 방 목록 화면에서 사용할 LAN·Steam 로비 검색을 시작하고, 완료 알림을 구분할 요청 ID를 반환한다.
uint64 UOnlineSessionsSubsystem::BeginFindRoomSessions(
	ULocalPlayer* RequestingLocalPlayer, const int32 MaxSearchResults, const bool bIsLAN, const bool bUseLobbies)
{
	const uint64 RequestId = BeginSessionRequest(RequestingLocalPlayer, ESessionRequestKind::FindRooms);
	if (RequestId == 0)
	{
		return 0;
	}

	PendingMaxSearchResults = FMath::Max(MaxSearchResults, 1);
	bPendingIsLAN = ShouldUseLANSession(bIsLAN);
	bPendingUseLobbies = ShouldUseLobbySession(bUseLobbies);
	if (!StartFindRoomsPhase(RequestId))
	{
		ResetActiveSessionRequest();
		return 0;
	}

	return RequestId;
}

// 방 목록에서 선택한 검색 결과로 참가를 요청한다. 접속 주소를 얻고 실제 이동하는 단계는 참가 완료 콜백이다.
uint64 UOnlineSessionsSubsystem::BeginJoinRoomSession(ULocalPlayer* RequestingLocalPlayer, const FBlueprintSessionResult& SessionResult)
{
	if (!SessionResult.OnlineResult.IsValid())
	{
		return 0;
	}

	const uint64 RequestId = BeginSessionRequest(RequestingLocalPlayer, ESessionRequestKind::JoinRoom);
	if (RequestId == 0)
	{
		return 0;
	}

	PendingJoinSessionResult = SessionResult;
	if (!StartJoinRoomPhase(RequestId))
	{
		ResetActiveSessionRequest();
		return 0;
	}

	return RequestId;
}

// 요청자를 지정해 현재 온라인 방에서 나가는 비동기 삭제를 시작한다. 방 목록 UI는 반환 ID로 자기 결과를 구분한다.
uint64 UOnlineSessionsSubsystem::BeginDestroySession(ULocalPlayer* RequestingLocalPlayer)
{
	if (!RefreshSessionInterface() || !SessionInterface->GetNamedSession(NAME_GameSession))
	{
		return 0;
	}

	const uint64 RequestId = BeginSessionRequest(RequestingLocalPlayer, ESessionRequestKind::DestroySession);
	if (RequestId == 0)
	{
		return 0;
	}

	if (!StartDestroySessionPhase(RequestId))
	{
		ResetActiveSessionRequest();
		return 0;
	}

	return RequestId;
}

// 빠른 매칭을 시작한다. 기존 방이 있으면 정리한 뒤 빈자리가 있는 방을 검색하고, 없으면 새 방을 만든다.
uint64 UOnlineSessionsSubsystem::BeginQuickMatch(ULocalPlayer* RequestingLocalPlayer, const int32 MaxSearchResults,
	const int32 MaxPublicConnections, const FString& RoomName, const FString& MapName, const bool bIsLAN, const bool bUseLobbies)
{
	const uint64 RequestId = BeginSessionRequest(RequestingLocalPlayer, ESessionRequestKind::QuickMatch);
	if (RequestId == 0)
	{
		return 0;
	}

	PendingMaxSearchResults = FMath::Max(MaxSearchResults, 1);
	PendingNumPublicConnections = FMath::Max(MaxPublicConnections, 1);
	PendingRoomName = RoomName.TrimStartAndEnd().IsEmpty() ? TEXT("Quick Match") : RoomName.TrimStartAndEnd();
	PendingMapName = MapName.TrimStartAndEnd().IsEmpty() ? TEXT("Lobby") : MapName.TrimStartAndEnd();
	bPendingIsLAN = ShouldUseLANSession(bIsLAN);
	bPendingUseLobbies = ShouldUseLobbySession(bUseLobbies);

	const bool bStarted =
		SessionInterface->GetNamedSession(NAME_GameSession) ? StartDestroySessionPhase(RequestId) : StartFindRoomsPhase(RequestId);
	if (!bStarted)
	{
		ResetActiveSessionRequest();
		return 0;
	}

	return RequestId;
}

// UI가 닫히거나 대기가 취소되면 실패를 한 번 알린다. 생성·참가는 완료 콜백까지 기다렸다가 생긴 세션을 정리한다.
bool UOnlineSessionsSubsystem::CancelSessionRequest(const uint64 RequestId)
{
	if (!IsSessionRequestActive(RequestId) || bActiveRequestCancelRequested || bActiveRequestResultSent)
	{
		return false;
	}

	bActiveRequestCancelRequested = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionOperationTimeoutHandle);
	}

	switch (SessionOperationState)
	{
	case ESessionOperationState::DestroyingExistingSession:
		SessionOperationState = ESessionOperationState::CancelingDestroy;
		break;
	case ESessionOperationState::FindingSessions:
		SessionOperationState = ESessionOperationState::CancelingFind;
		if (SessionInterface.IsValid())
		{
			SessionInterface->CancelFindSessions();
		}
		BroadcastActiveRequestFailure();
		ResetActiveSessionRequest();
		return true;
	case ESessionOperationState::CreatingSession:
		SessionOperationState = ESessionOperationState::CancelingCreate;
		break;
	case ESessionOperationState::JoiningSession:
		SessionOperationState = ESessionOperationState::CancelingJoin;
		break;
	default:
		break;
	}

	BroadcastActiveRequestFailure();
	return true;
}

// 요청 ID가 현재 진행 중인 작업인지 확인해 이전 화면의 취소·타임아웃이 새 요청을 건드리지 않게 한다.
bool UOnlineSessionsSubsystem::IsSessionRequestActive(const uint64 RequestId) const
{
	return RequestId != 0 && RequestId == ActiveSessionRequestId && SessionOperationState != ESessionOperationState::Idle;
}

// 로비 나가기·타이틀 복귀 시 현재 방의 삭제를 요청한다. 이미 방이 없으면 성공 알림을 즉시 보내 UI 대기를 끝낸다.
void UOnlineSessionsSubsystem::DestroySession()
{
	if (!RefreshSessionInterface() || !SessionInterface->GetNamedSession(NAME_GameSession))
	{
		OnDestroySessionComplete.Broadcast(true);
		return;
	}

	ULocalPlayer* LocalPlayer = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
	if (BeginDestroySession(LocalPlayer) == 0)
	{
		OnDestroySessionComplete.Broadcast(false);
	}
}

// 호스트가 바꾼 맵·정원·중도 참가 설정을 온라인 방 정보에 반영해 검색하는 플레이어에게 공개한다.
void UOnlineSessionsSubsystem::UpdateSessionSettings(
	const FString& MapName, const int32 NumPublicConnections, const bool bAllowJoinInProgress)
{
	if (!RefreshSessionInterface())
	{
		return;
	}

	FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (!ExistingSession)
	{
		return;
	}

	FOnlineSessionSettings UpdatedSettings = ExistingSession->SessionSettings;
	const bool bUseLobbySession = IsSteamSubsystemActive() && !UpdatedSettings.bIsDedicated;
	UpdatedSettings.NumPublicConnections = FMath::Max(NumPublicConnections, 1);
	UpdatedSettings.Set(LabOnlineSession::MapNameSettingKey, MapName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	UpdatedSettings.bShouldAdvertise = true;
	UpdatedSettings.bAllowJoinInProgress = bAllowJoinInProgress;
	UpdatedSettings.bAllowJoinViaPresence = bUseLobbySession;
	UpdatedSettings.bUsesPresence = bUseLobbySession;
	UpdatedSettings.bUseLobbiesIfAvailable = bUseLobbySession;

	SessionInterface->UpdateSession(NAME_GameSession, UpdatedSettings, true);
}

// 현재 월드에 GameSession이 등록되어 있는지 확인한다. 방 나가기와 경기 시작 UI의 세션 존재 판단에 사용한다.
bool UOnlineSessionsSubsystem::HasNamedSession() const
{
	const IOnlineSessionPtr Sessions = GetSessionInterfaceForWorld();
	return Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
}

// PIE 인스턴스마다 올바른 Steam·NULL 온라인 서비스를 찾고, 월드별 서비스가 없으면 기본 서비스를 사용한다.
IOnlineSubsystem* UOnlineSessionsSubsystem::GetOnlineSubsystemForWorld() const
{
	if (IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld()))
	{
		return OnlineSubsystem;
	}

	return IOnlineSubsystem::Get();
}

// 현재 월드가 사용하는 온라인 서비스에서 방 생성·검색·참가 API를 가져온다.
IOnlineSessionPtr UOnlineSessionsSubsystem::GetSessionInterfaceForWorld() const
{
	if (IOnlineSubsystem* OnlineSubsystem = GetOnlineSubsystemForWorld())
	{
		return OnlineSubsystem->GetSessionInterface();
	}

	return nullptr;
}

// 요청을 시작하기 전에 현재 월드의 세션 인터페이스를 다시 확보하고 사용 가능 여부를 반환한다.
bool UOnlineSessionsSubsystem::RefreshSessionInterface()
{
	SessionInterface = GetSessionInterfaceForWorld();
	return SessionInterface.IsValid();
}

// 플랫폼 없이 로컬 멀티플레이를 시험하는 NULL 서비스를 사용하는지 판단한다.
bool UOnlineSessionsSubsystem::IsNullSubsystemActive() const
{
	const IOnlineSubsystem* OnlineSubsystem = GetOnlineSubsystemForWorld();
	return OnlineSubsystem && OnlineSubsystem->GetSubsystemName() == FName(TEXT("NULL"));
}

// Steam 로비·Presence 기능을 켤 수 있는 온라인 서비스인지 판단한다.
bool UOnlineSessionsSubsystem::IsSteamSubsystemActive() const
{
	const IOnlineSubsystem* OnlineSubsystem = GetOnlineSubsystemForWorld();
	return OnlineSubsystem && OnlineSubsystem->GetSubsystemName() == FName(TEXT("STEAM"));
}

// NULL 서비스는 LAN 검색을 사용하고, Steam 등 다른 서비스는 호출자가 요청한 LAN 설정을 따른다.
bool UOnlineSessionsSubsystem::ShouldUseLANSession(const bool bRequestedLAN) const
{
	// OnlineSubsystemNull discovers sessions through LAN beacons. Steam/EOS sessions
	// should keep the caller's value so platform matchmaking can be used.
	return bRequestedLAN || IsNullSubsystemActive();
}

// Steam 서비스를 사용하면서 호출자가 로비 검색을 요청했을 때만 로비 검색 조건을 켠다.
bool UOnlineSessionsSubsystem::ShouldUseLobbySession(const bool bRequestedLobbies) const
{
	return bRequestedLobbies && IsSteamSubsystemActive();
}

// 방 생성과 방 목록 UI가 같은 키로 공개 방 이름을 기록·조회하도록 한다.
FName UOnlineSessionsSubsystem::GetRoomNameSettingKey()
{
	return LabOnlineSession::RoomNameSettingKey;
}

// 방 정보 갱신과 방 목록 UI가 같은 키로 맵 이름을 기록·조회하도록 한다.
FName UOnlineSessionsSubsystem::GetMapNameSettingKey()
{
	return LabOnlineSession::MapNameSettingKey;
}

// 종료 시 남아 있는 시작·종료 및 방 요청 콜백을 등록했던 인터페이스에서 해제한다.
void UOnlineSessionsSubsystem::ClearSessionDelegates()
{
	ClearSessionLifecycleOperation();

	if (!SessionInterface.IsValid())
	{
		return;
	}

	if (CreateSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
	}
	if (FindSessionsCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		FindSessionsCompleteDelegateHandle.Reset();
	}
	if (JoinSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		JoinSessionCompleteDelegateHandle.Reset();
	}
	if (DestroySessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		DestroySessionCompleteDelegateHandle.Reset();
	}
}

namespace
{
// 연결이 끊긴 경기 결과에 표시할 이름을 매치 표시명, 엔진 플레이어 이름 순서로 선택한다.
FText ResolveResultPlayerName(const APlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return NSLOCTEXT("GameResult", "UnknownPlayerName", "Unknown");
	}

	if (const APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState))
	{
		if (!PdPlayerState->GetPlayerMatchComponent()->GetMatchDisplayName().IsEmpty())
		{
			return PdPlayerState->GetPlayerMatchComponent()->GetMatchDisplayName();
		}
	}

	const FString PlayerName = PlayerState->GetPlayerName();
	return FText::FromString(PlayerName.IsEmpty() ? GetNameSafe(PlayerState) : PlayerName);
}

// 연결 끊김 결과 화면에 표시할 팀 색상 이름을 선택한다.
FText ResolveResultTeamName(const int32 TeamColorIndex)
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

// 현재 전장의 점수로 연결 끊김 결과를 만든다. 보상·로비 복귀는 허용하지 않고 킬·데스 순서로 정렬한다.
bool BuildNetworkFailureGameResult(UWorld* World, FGameResultPresentationData& OutGameResultData)
{
	const AExperienceGameState* ExperienceGameState = World ? World->GetGameState<AExperienceGameState>() : nullptr;
	if (!ExperienceGameState)
	{
		return false;
	}

	OutGameResultData = FGameResultPresentationData();
	OutGameResultData.WinnerTitle = NSLOCTEXT("GameResult", "MatchEndedByConnectionLost", "Match Ended Due to Player Leaving");
	OutGameResultData.WinnerTeamColorIndex = INDEX_NONE;
	OutGameResultData.bAllowLobbyTravelOnExit = false;
	OutGameResultData.bShowRewards = false;

	for (APlayerState* PlayerState : ExperienceGameState->PlayerArray)
	{
		const APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState);
		if (!PdPlayerState)
		{
			continue;
		}

		FGameResultPlayerStat PlayerStat;
		PlayerStat.PlayerName = ResolveResultPlayerName(PdPlayerState);
		PlayerStat.TeamColorIndex = PdPlayerState->GetPlayerMatchComponent()->GetMatchTeamColorIndex();
		PlayerStat.PlayerStateId = PdPlayerState->GetPlayerId();
		PlayerStat.TeamName = ResolveResultTeamName(PlayerStat.TeamColorIndex);
		PlayerStat.KillCount = PdPlayerState->GetPlayerMatchComponent()->GetKillCount();
		PlayerStat.DeathCount = PdPlayerState->GetPlayerMatchComponent()->GetDeathCount();
		OutGameResultData.PlayerStats.Add(PlayerStat);
	}

	OutGameResultData.PlayerStats.Sort([](const FGameResultPlayerStat& Left, const FGameResultPlayerStat& Right) {
		if (Left.KillCount != Right.KillCount)
		{
			return Left.KillCount > Right.KillCount;
		}

		if (Left.DeathCount != Right.DeathCount)
		{
			return Left.DeathCount < Right.DeathCount;
		}

		return Left.PlayerName.ToString() < Right.PlayerName.ToString();
	});

	if (!OutGameResultData.PlayerStats.IsEmpty())
	{
		OutGameResultData.MaxKillerName = OutGameResultData.PlayerStats[0].PlayerName;
		OutGameResultData.MaxKillCount = OutGameResultData.PlayerStats[0].KillCount;
	}
	else
	{
		OutGameResultData.MaxKillerName = NSLOCTEXT("GameResult", "UnknownPlayerName", "Unknown");
		OutGameResultData.MaxKillCount = 0;
	}

	return true;
}
} // namespace

// 호스트 연결이 끊긴 경기의 점수를 보존하고 타이틀 이동을 예약한다. 자발적으로 나가는 중이면 연결 끊김 결과를 표시하지 않는다.
void UOnlineSessionsSubsystem::HandleNetworkFailure(
	UWorld* World, UNetDriver*, const ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (World && GetWorld() && World != GetWorld())
	{
		return;
	}
	if (bVoluntaryMatchExitInProgress)
	{
		if (ULobbyRuntimeSubsystem* LobbySubsystem = UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GetGameInstance()))
		{
			LobbySubsystem->ClearPendingTitleGameResult();
		}
		return;
	}

	if (IsHostConnectionLost(FailureType, ErrorString))
	{
		FGameResultPresentationData GameResultData;
		if (BuildNetworkFailureGameResult(World, GameResultData))
		{
			if (ULobbyRuntimeSubsystem* LobbySubsystem = UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GetGameInstance()))
			{
				if (!LobbySubsystem->HasPendingTitleGameResult())
				{
					LobbySubsystem->SetPendingTitleGameResult(GameResultData);
				}
			}

			if (World)
			{
				const ULevelDefinition* Levels = ULevelDefinition::ResolveDefaultDefinition();
				const FString TitleMapName = Levels ? Levels->GetTitleTravelMapName() : FString();
				World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(World, [World, TitleMapName]() {
					if (IsValid(World) && !TitleMapName.IsEmpty())
					{
						UGameplayStatics::OpenLevel(World, FName(*TitleMapName));
					}
				}));
			}
			return;
		}
	}

	// Do not auto-destroy sessions here. In multi-PIE/listen-server tests the NULL
	// subsystem can be shared by local windows, so destroying on a client-side close
	// can tear down the host's advertised room.
}

// 엔진 실패 종류와 메시지를 확인해 호스트 종료·호스트 연결 상실에 해당하는 경우를 구분한다.
bool UOnlineSessionsSubsystem::IsHostConnectionLost(const ENetworkFailure::Type FailureType, const FString& ErrorString) const
{
	if (FailureType != ENetworkFailure::FailureReceived && FailureType != ENetworkFailure::ConnectionLost)
	{
		return false;
	}

	return ErrorString.Contains(TEXT("Host closed the connection"), ESearchCase::IgnoreCase)
		|| ErrorString.Contains(TEXT("connection to the host has been lost"), ESearchCase::IgnoreCase);
}
