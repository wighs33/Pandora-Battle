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
#include "Mode/PdGameInstance.h"
#include "Mode/PdPlayerState.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "TimerManager.h"
#include "UI/GameResultTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OnlineSessionsSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogOnlineSessionsSubsystem, Log, All);

namespace LabOnlineSession
{
	const FName RoomNameSettingKey(TEXT("ROOM_NAME"));
	const FName MapNameSettingKey(TEXT("MAP_NAME"));
}

namespace
{
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

		OutGameResultData.PlayerStats.Sort([](const FGameResultPlayerStat& Left, const FGameResultPlayerStat& Right)
		{
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
		else if (OutGameResultData.MaxKillerName.IsEmpty())
		{
			OutGameResultData.MaxKillerName = NSLOCTEXT("GameResult", "UnknownPlayerName", "Unknown");
			OutGameResultData.MaxKillCount = 0;
		}

		return true;
	}
}

UOnlineSessionsSubsystem::UOnlineSessionsSubsystem()
	: UpdateSessionCompleteDelegate(FOnUpdateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnUpdateSessionCompleted))
{
}

void UOnlineSessionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	IOnlineSubsystem* OnlineSubsystem = GetOnlineSubsystemForWorld();
	if (!OnlineSubsystem)
	{

		return;
	}

	SessionManager = GetSessionManagerForWorld();

	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
	}
}

void UOnlineSessionsSubsystem::Deinitialize()
{
	if (GEngine)
	{
		if (NetworkFailureHandle.IsValid())
		{
			GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
			NetworkFailureHandle.Reset();
		}

		if (TravelFailureHandle.IsValid())
		{
			GEngine->OnTravelFailure().Remove(TravelFailureHandle);
			TravelFailureHandle.Reset();
		}
	}

	ClearSessionDelegates();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionOperationTimeoutHandle);
		World->GetTimerManager().ClearTimer(SessionLifecycleTimeoutHandle);
	}
	ClearSessionLifecycleOperation();
	FinishActiveSessionRequest();
	SessionManager.Reset();
	LastSessionSearch.Reset();
	Super::Deinitialize();
}

void UOnlineSessionsSubsystem::CreateRoomSession(
	const FString& RoomName,
	const FString& MapName,
	const int32 NumPublicConnections,
	const bool bIsLAN)
{
	BeginCreateRoomSession(
		ResolveDefaultLocalPlayer(),
		RoomName,
		MapName,
		NumPublicConnections,
		bIsLAN);
}

uint64 UOnlineSessionsSubsystem::BeginCreateRoomSession(
	ULocalPlayer* RequestingLocalPlayer,
	const FString& RoomName,
	const FString& MapName,
	const int32 NumPublicConnections,
	const bool bIsLAN)
{
	const uint64 RequestId = BeginSessionRequest(
		RequestingLocalPlayer,
		ESessionRequestKind::CreateRoom);
	if (RequestId == 0 || SessionManager->GetNamedSession(NAME_GameSession))
	{
		if (RequestId != 0)
		{
			FinishActiveSessionRequest();
		}
		return 0;
	}

	PendingRoomName = RoomName.TrimStartAndEnd().IsEmpty()
		? TEXT("Room")
		: RoomName.TrimStartAndEnd();
	PendingMapName = MapName.TrimStartAndEnd().IsEmpty()
		? TEXT("Unknown")
		: MapName.TrimStartAndEnd();
	PendingNumPublicConnections = FMath::Max(NumPublicConnections, 1);
	bPendingIsLAN = ResolveLanSession(bIsLAN);

	if (!StartCreateRoomPhase(RequestId))
	{
		FinishActiveSessionRequest();
		return 0;
	}

	return RequestId;
}

uint64 UOnlineSessionsSubsystem::BeginFindRoomSessions(
	ULocalPlayer* RequestingLocalPlayer,
	const int32 MaxSearchResults,
	const bool bIsLAN,
	const bool bUseLobbies)
{
	const uint64 RequestId = BeginSessionRequest(
		RequestingLocalPlayer,
		ESessionRequestKind::FindRooms);
	if (RequestId == 0)
	{
		return 0;
	}

	PendingMaxSearchResults = FMath::Max(MaxSearchResults, 1);
	bPendingIsLAN = ResolveLanSession(bIsLAN);
	bPendingUseLobbies = ResolveLobbySession(bUseLobbies);
	if (!StartFindRoomsPhase(RequestId))
	{
		FinishActiveSessionRequest();
		return 0;
	}

	return RequestId;
}

uint64 UOnlineSessionsSubsystem::BeginJoinRoomSession(
	ULocalPlayer* RequestingLocalPlayer,
	const FBlueprintSessionResult& SessionResult)
{
	if (!SessionResult.OnlineResult.IsValid())
	{
		return 0;
	}

	const uint64 RequestId = BeginSessionRequest(
		RequestingLocalPlayer,
		ESessionRequestKind::JoinRoom);
	if (RequestId == 0)
	{
		return 0;
	}

	PendingJoinSessionResult = SessionResult;
	if (!StartJoinRoomPhase(RequestId))
	{
		FinishActiveSessionRequest();
		return 0;
	}

	return RequestId;
}

uint64 UOnlineSessionsSubsystem::BeginDestroySession(ULocalPlayer* RequestingLocalPlayer)
{
	if (!RefreshSessionManager() || !SessionManager->GetNamedSession(NAME_GameSession))
	{
		return 0;
	}

	const uint64 RequestId = BeginSessionRequest(
		RequestingLocalPlayer,
		ESessionRequestKind::DestroySession);
	if (RequestId == 0)
	{
		return 0;
	}

	if (!StartDestroySessionPhase(RequestId))
	{
		FinishActiveSessionRequest();
		return 0;
	}

	return RequestId;
}

uint64 UOnlineSessionsSubsystem::BeginQuickMatch(
	ULocalPlayer* RequestingLocalPlayer,
	const int32 MaxSearchResults,
	const int32 MaxPublicConnections,
	const FString& RoomName,
	const FString& MapName,
	const bool bIsLAN,
	const bool bUseLobbies)
{
	const uint64 RequestId = BeginSessionRequest(
		RequestingLocalPlayer,
		ESessionRequestKind::QuickMatch);
	if (RequestId == 0)
	{
		return 0;
	}

	PendingMaxSearchResults = FMath::Max(MaxSearchResults, 1);
	PendingNumPublicConnections = FMath::Max(MaxPublicConnections, 1);
	PendingRoomName = RoomName.TrimStartAndEnd().IsEmpty()
		? TEXT("Quick Match")
		: RoomName.TrimStartAndEnd();
	PendingMapName = MapName.TrimStartAndEnd().IsEmpty()
		? TEXT("Lobby")
		: MapName.TrimStartAndEnd();
	bPendingIsLAN = ResolveLanSession(bIsLAN);
	bPendingUseLobbies = ResolveLobbySession(bUseLobbies);

	const bool bStarted = SessionManager->GetNamedSession(NAME_GameSession)
		? StartDestroySessionPhase(RequestId)
		: StartFindRoomsPhase(RequestId);
	if (!bStarted)
	{
		FinishActiveSessionRequest();
		return 0;
	}

	return RequestId;
}

bool UOnlineSessionsSubsystem::CancelSessionRequest(const uint64 RequestId)
{
	if (!IsSessionRequestActive(RequestId)
		|| bActiveRequestCancelRequested
		|| bActiveRequestCompletionBroadcast)
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
		if (SessionManager.IsValid())
		{
			SessionManager->CancelFindSessions();
		}
		BroadcastActiveRequestFailure();
		FinishActiveSessionRequest();
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

bool UOnlineSessionsSubsystem::IsSessionRequestActive(const uint64 RequestId) const
{
	return RequestId != 0
		&& RequestId == ActiveSessionRequestId
		&& SessionOperationState != ESessionOperationState::Idle;
}

void UOnlineSessionsSubsystem::DestroySession()
{
	if (!RefreshSessionManager() || !SessionManager->GetNamedSession(NAME_GameSession))
	{
		OnDestroySessionComplete.Broadcast(true);
		return;
	}

	if (BeginDestroySession(ResolveDefaultLocalPlayer()) == 0)
	{
		OnDestroySessionComplete.Broadcast(false);
	}
}

void UOnlineSessionsSubsystem::UpdateSessionSettings(
	const FString& MapName,
	const int32 NumPublicConnections,
	const bool bAllowJoinInProgress)
{
	if (!RefreshSessionManager())
	{
		return;
	}

	FNamedOnlineSession* ExistingSession = SessionManager->GetNamedSession(NAME_GameSession);
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

	if (UpdateSessionCompleteDelegateHandle.IsValid())
	{
		SessionManager->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
		UpdateSessionCompleteDelegateHandle.Reset();
	}
	UpdateSessionCompleteDelegateHandle = SessionManager->AddOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegate);

	if (!SessionManager->UpdateSession(NAME_GameSession, UpdatedSettings, true))
	{
		SessionManager->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
		UpdateSessionCompleteDelegateHandle.Reset();
	}
}

bool UOnlineSessionsSubsystem::HasNamedSession() const
{
	const IOnlineSessionPtr Sessions = GetSessionManagerForWorld();
	return Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
}

FString UOnlineSessionsSubsystem::GetOnlineSubsystemName() const
{
	const IOnlineSubsystem* OnlineSubsystem = GetOnlineSubsystemForWorld();
	return OnlineSubsystem ? OnlineSubsystem->GetSubsystemName().ToString() : FString();
}

IOnlineSubsystem* UOnlineSessionsSubsystem::GetOnlineSubsystemForWorld() const
{
	if (IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld()))
	{
		return OnlineSubsystem;
	}

	return IOnlineSubsystem::Get();
}

IOnlineSessionPtr UOnlineSessionsSubsystem::GetSessionManagerForWorld() const
{
	if (IOnlineSubsystem* OnlineSubsystem = GetOnlineSubsystemForWorld())
	{
		return OnlineSubsystem->GetSessionInterface();
	}

	return nullptr;
}

bool UOnlineSessionsSubsystem::RefreshSessionManager()
{
	SessionManager = GetSessionManagerForWorld();
	return SessionManager.IsValid();
}

bool UOnlineSessionsSubsystem::IsNullSubsystemActive() const
{
	return GetOnlineSubsystemName().Equals(TEXT("NULL"), ESearchCase::IgnoreCase);
}

bool UOnlineSessionsSubsystem::IsSteamSubsystemActive() const
{
	return GetOnlineSubsystemName().Equals(TEXT("STEAM"), ESearchCase::IgnoreCase);
}

bool UOnlineSessionsSubsystem::ResolveLanSession(const bool bRequestedLAN) const
{
	// OnlineSubsystemNull discovers sessions through LAN beacons. Steam/EOS sessions
	// should keep the caller's value so platform matchmaking can be used.
	return bRequestedLAN || IsNullSubsystemActive();
}

bool UOnlineSessionsSubsystem::ResolveLobbySession(const bool bRequestedLobbies) const
{
	return bRequestedLobbies && IsSteamSubsystemActive();
}

FName UOnlineSessionsSubsystem::GetRoomNameSettingKey()
{
	return LabOnlineSession::RoomNameSettingKey;
}

FName UOnlineSessionsSubsystem::GetMapNameSettingKey()
{
	return LabOnlineSession::MapNameSettingKey;
}

void UOnlineSessionsSubsystem::ClearSessionDelegates()
{
	ClearSessionLifecycleOperation();

	if (!SessionManager.IsValid())
	{
		return;
	}

	if (CreateSessionCompleteDelegateHandle.IsValid())
	{
		SessionManager->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
	}
	if (FindSessionsCompleteDelegateHandle.IsValid())
	{
		SessionManager->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		FindSessionsCompleteDelegateHandle.Reset();
	}
	if (JoinSessionCompleteDelegateHandle.IsValid())
	{
		SessionManager->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		JoinSessionCompleteDelegateHandle.Reset();
	}
	if (StartSessionCompleteDelegateHandle.IsValid())
	{
		SessionManager->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		StartSessionCompleteDelegateHandle.Reset();
	}
	if (EndSessionCompleteDelegateHandle.IsValid())
	{
		SessionManager->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
		EndSessionCompleteDelegateHandle.Reset();
	}
	if (DestroySessionCompleteDelegateHandle.IsValid())
	{
		SessionManager->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		DestroySessionCompleteDelegateHandle.Reset();
	}
	if (UpdateSessionCompleteDelegateHandle.IsValid())
	{
		SessionManager->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
		UpdateSessionCompleteDelegateHandle.Reset();
	}
}

void UOnlineSessionsSubsystem::OnUpdateSessionCompleted(FName, const bool)
{
	if (SessionManager.IsValid() && UpdateSessionCompleteDelegateHandle.IsValid())
	{
		SessionManager->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
		UpdateSessionCompleteDelegateHandle.Reset();
	}

}

void UOnlineSessionsSubsystem::HandleNetworkFailure(
	UWorld* World,
	UNetDriver* NetDriver,
	const ENetworkFailure::Type FailureType,
	const FString& ErrorString)
{
	static_cast<void>(NetDriver);

	if (World && GetWorld() && World != GetWorld())
	{
		return;
	}
	if (bVoluntaryMatchExitInProgress)
	{
		if (UPdGameInstance* PdGameInstance =
			Cast<UPdGameInstance>(GetGameInstance()))
		{
			PdGameInstance->ClearPendingTitleGameResult();
		}
		return;
	}

	if (IsExpectedConnectionClose(FailureType, ErrorString))
	{
		FGameResultPresentationData GameResultData;
		if (BuildNetworkFailureGameResult(World, GameResultData))
		{
			if (UPdGameInstance* PdGameInstance = Cast<UPdGameInstance>(GetGameInstance()))
			{
				if (!PdGameInstance->HasPendingTitleGameResult())
				{
					PdGameInstance->SetPendingTitleGameResult(GameResultData);
				}
			}

			if (World)
			{
				const ULevelDefinition* Levels =
					ULevelDefinition::ResolveDefaultDefinition();
				const FString TitleMapName = Levels
					? Levels->GetTitleTravelMapName()
					: FString();
				World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(World, [World, TitleMapName]()
				{
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

void UOnlineSessionsSubsystem::HandleTravelFailure(
	UWorld* World,
	const ETravelFailure::Type FailureType,
	const FString& ErrorString)
{
	static_cast<void>(FailureType);
	static_cast<void>(ErrorString);

	if (World && GetWorld() && World != GetWorld())
	{
		return;
	}

	// See HandleNetworkFailure: session cleanup must be driven by explicit UI/game
	// flow, not by global travel-failure callbacks in editor multiplayer tests.
}

bool UOnlineSessionsSubsystem::IsExpectedConnectionClose(
	const ENetworkFailure::Type FailureType,
	const FString& ErrorString) const
{
	if (FailureType != ENetworkFailure::FailureReceived &&
		FailureType != ENetworkFailure::ConnectionLost)
	{
		return false;
	}

	return ErrorString.Contains(TEXT("Host closed the connection"), ESearchCase::IgnoreCase) ||
		ErrorString.Contains(TEXT("connection to the host has been lost"), ESearchCase::IgnoreCase);
}
