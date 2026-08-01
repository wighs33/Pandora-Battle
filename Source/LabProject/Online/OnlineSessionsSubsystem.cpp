#include "Online/OnlineSessionsSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OnlineSessionsSubsystem)

namespace LabOnlineSession
{
	const FName RoomNameSettingKey(TEXT("ROOM_NAME"));
	const FName MapNameSettingKey(TEXT("MAP_NAME"));
}

UOnlineSessionsSubsystem::UOnlineSessionsSubsystem()
	: StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionCompleted))
	, EndSessionCompleteDelegate(FOnEndSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnEndSessionCompleted))
	, UpdateSessionCompleteDelegate(FOnUpdateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnUpdateSessionCompleted))
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

}

void UOnlineSessionsSubsystem::Deinitialize()
{
	ClearSessionDelegates();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionOperationTimeoutHandle);
	}
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
	if (BeginCreateRoomSession(
		ResolveDefaultLocalPlayer(),
		RoomName,
		MapName,
		NumPublicConnections,
		bIsLAN) == 0)
	{
		OnCreateSessionComplete.Broadcast(false);
	}
}

void UOnlineSessionsSubsystem::FindRoomSessions(const int32 MaxSearchResults, const bool bIsLAN, const bool bUseLobbies)
{
	if (BeginFindRoomSessions(
		ResolveDefaultLocalPlayer(),
		MaxSearchResults,
		bIsLAN,
		bUseLobbies) == 0)
	{
		OnFindSessionsComplete.Broadcast(TArray<FBlueprintSessionResult>(), false);
	}
}

void UOnlineSessionsSubsystem::JoinRoomSession(const FBlueprintSessionResult& SessionResult)
{
	if (BeginJoinRoomSession(ResolveDefaultLocalPlayer(), SessionResult) == 0)
	{
		OnJoinSessionComplete.Broadcast(false);
	}
}

void UOnlineSessionsSubsystem::CancelPendingJoinSession()
{
	if (ActiveSessionRequestId != 0
		&& SessionOperationState == ESessionOperationState::JoiningSession)
	{
		CancelSessionRequest(ActiveSessionRequestId);
	}
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

void UOnlineSessionsSubsystem::StartSession()
{
	if (!RefreshSessionManager() || !SessionManager->GetNamedSession(NAME_GameSession))
	{
		OnStartSessionComplete.Broadcast(true);
		return;
	}

	if (StartSessionCompleteDelegateHandle.IsValid())
	{
		SessionManager->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		StartSessionCompleteDelegateHandle.Reset();
	}
	StartSessionCompleteDelegateHandle = SessionManager->AddOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegate);

	if (!SessionManager->StartSession(NAME_GameSession))
	{
		SessionManager->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		StartSessionCompleteDelegateHandle.Reset();
		OnStartSessionComplete.Broadcast(true);
	}
}

void UOnlineSessionsSubsystem::EndSession()
{
	if (!RefreshSessionManager() || !SessionManager->GetNamedSession(NAME_GameSession))
	{
		OnEndSessionComplete.Broadcast(true);
		return;
	}

	if (EndSessionCompleteDelegateHandle.IsValid())
	{
		SessionManager->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
		EndSessionCompleteDelegateHandle.Reset();
	}
	EndSessionCompleteDelegateHandle = SessionManager->AddOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegate);

	if (!SessionManager->EndSession(NAME_GameSession))
	{
		SessionManager->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
		EndSessionCompleteDelegateHandle.Reset();
		OnEndSessionComplete.Broadcast(true);
	}
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

void UOnlineSessionsSubsystem::UpdateSessionMapName(
	const FString& MapName,
	const bool bAllowJoinInProgress)
{
	const IOnlineSessionPtr Sessions = GetSessionManagerForWorld();
	const FNamedOnlineSession* ExistingSession = Sessions.IsValid()
		? Sessions->GetNamedSession(NAME_GameSession)
		: nullptr;
	const int32 ExistingPublicConnections = ExistingSession
		? ExistingSession->SessionSettings.NumPublicConnections
		: LabGameSession::MaxPlayerCount;
	UpdateSessionSettings(
		MapName,
		ExistingPublicConnections,
		bAllowJoinInProgress);
}

void UOnlineSessionsSubsystem::UpdateSessionSettings(
	const FString& MapName,
	const int32 NumPublicConnections,
	const bool bAllowJoinInProgress)
{
	if (!RefreshSessionManager())
	{
		OnUpdateSessionComplete.Broadcast(false);
		return;
	}

	FNamedOnlineSession* ExistingSession = SessionManager->GetNamedSession(NAME_GameSession);
	if (!ExistingSession)
	{
		OnUpdateSessionComplete.Broadcast(true);
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
		OnUpdateSessionComplete.Broadcast(false);
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
	if (!SessionManager.IsValid())
	{

		return false;
	}

	return true;
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

uint64 UOnlineSessionsSubsystem::BeginSessionRequest(
	ULocalPlayer* RequestingLocalPlayer,
	const ESessionRequestKind RequestKind)
{
	if (SessionOperationState != ESessionOperationState::Idle
		|| !RefreshSessionManager()
		|| (!RequestingLocalPlayer && !IsRunningDedicatedServer()))
	{
		return 0;
	}

	const uint64 RequestId = NextSessionRequestId++;
	if (NextSessionRequestId == 0)
	{
		NextSessionRequestId = 1;
	}

	ActiveSessionRequestId = RequestId;
	ActiveSessionRequestKind = RequestKind;
	ActiveRequestLocalPlayer = RequestingLocalPlayer;
	ActiveRequestLocalPlayerNetId = RequestingLocalPlayer
		? RequestingLocalPlayer->GetPreferredUniqueNetId()
		: FUniqueNetIdRepl();
	ActiveRequestControllerId = RequestingLocalPlayer
		? RequestingLocalPlayer->GetControllerId()
		: 0;
	bActiveRequestCancelRequested = false;
	bActiveRequestCompletionBroadcast = false;
	return RequestId;
}

bool UOnlineSessionsSubsystem::StartCreateRoomPhase(const uint64 RequestId)
{
	if (!IsCallbackForActiveRequest(RequestId)
		|| !RefreshSessionManager()
		|| SessionManager->GetNamedSession(NAME_GameSession))
	{
		return false;
	}

	CleanupOperationDelegateForState(SessionOperationState);
	SessionOperationState = ESessionOperationState::CreatingSession;
	CreateSessionCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(
		this,
		&ThisClass::OnCreateSessionCompleted,
		RequestId);
	CreateSessionCompleteDelegateHandle =
		SessionManager->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	FOnlineSessionSettings Settings;
	const bool bDedicatedSession = IsRunningDedicatedServer()
		|| (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer);
	const bool bUseLobbySession = IsSteamSubsystemActive() && !bDedicatedSession;
	Settings.NumPublicConnections = PendingNumPublicConnections;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bIsLANMatch = bPendingIsLAN;
	Settings.bIsDedicated = bDedicatedSession;
	Settings.bUsesPresence = bUseLobbySession;
	Settings.bAllowJoinViaPresence = bUseLobbySession;
	Settings.bUseLobbiesIfAvailable = bUseLobbySession;
	Settings.Set(
		LabOnlineSession::RoomNameSettingKey,
		PendingRoomName,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(
		LabOnlineSession::MapNameSettingKey,
		PendingMapName,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	const bool bStarted = ActiveRequestLocalPlayerNetId.IsValid()
		? SessionManager->CreateSession(
			*ActiveRequestLocalPlayerNetId,
			NAME_GameSession,
			Settings)
		: SessionManager->CreateSession(
			ActiveRequestControllerId,
			NAME_GameSession,
			Settings);
	if (!bStarted)
	{
		CleanupOperationDelegateForState(SessionOperationState);
		return false;
	}

	ArmSessionOperationTimeout(RequestId);
	return true;
}

bool UOnlineSessionsSubsystem::StartFindRoomsPhase(const uint64 RequestId)
{
	if (!IsCallbackForActiveRequest(RequestId) || !RefreshSessionManager())
	{
		return false;
	}

	CleanupOperationDelegateForState(SessionOperationState);
	SessionOperationState = ESessionOperationState::FindingSessions;
	FindSessionsCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(
		this,
		&ThisClass::OnFindSessionsCompleted,
		RequestId);
	FindSessionsCompleteDelegateHandle =
		SessionManager->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	LastSessionSearch = MakeShared<FOnlineSessionSearch>();
	LastSessionSearch->MaxSearchResults = PendingMaxSearchResults;
	LastSessionSearch->bIsLanQuery = bPendingIsLAN;
	if (bPendingUseLobbies)
	{
		LastSessionSearch->QuerySettings.Set(
			SEARCH_LOBBIES,
			true,
			EOnlineComparisonOp::Equals);
	}

	const bool bStarted = ActiveRequestLocalPlayerNetId.IsValid()
		? SessionManager->FindSessions(
			*ActiveRequestLocalPlayerNetId,
			LastSessionSearch.ToSharedRef())
		: SessionManager->FindSessions(
			ActiveRequestControllerId,
			LastSessionSearch.ToSharedRef());
	if (!bStarted)
	{
		CleanupOperationDelegateForState(SessionOperationState);
		LastSessionSearch.Reset();
		return false;
	}

	ArmSessionOperationTimeout(RequestId);
	return true;
}

bool UOnlineSessionsSubsystem::StartJoinRoomPhase(const uint64 RequestId)
{
	if (!IsCallbackForActiveRequest(RequestId)
		|| !RefreshSessionManager()
		|| !PendingJoinSessionResult.OnlineResult.IsValid())
	{
		return false;
	}

	CleanupOperationDelegateForState(SessionOperationState);
	SessionOperationState = ESessionOperationState::JoiningSession;
	JoinSessionCompleteDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(
		this,
		&ThisClass::OnJoinSessionCompleted,
		RequestId);
	JoinSessionCompleteDelegateHandle =
		SessionManager->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	const bool bStarted = ActiveRequestLocalPlayerNetId.IsValid()
		? SessionManager->JoinSession(
			*ActiveRequestLocalPlayerNetId,
			NAME_GameSession,
			PendingJoinSessionResult.OnlineResult)
		: SessionManager->JoinSession(
			ActiveRequestControllerId,
			NAME_GameSession,
			PendingJoinSessionResult.OnlineResult);
	if (!bStarted)
	{
		CleanupOperationDelegateForState(SessionOperationState);
		return false;
	}

	ArmSessionOperationTimeout(RequestId);
	return true;
}

bool UOnlineSessionsSubsystem::StartDestroySessionPhase(
	const uint64 RequestId,
	const bool bCleanupCanceledSession)
{
	if (!IsCallbackForActiveRequest(RequestId)
		|| !RefreshSessionManager()
		|| !SessionManager->GetNamedSession(NAME_GameSession))
	{
		return false;
	}

	CleanupOperationDelegateForState(SessionOperationState);
	SessionOperationState = bCleanupCanceledSession
		? ESessionOperationState::CleaningCanceledSession
		: ESessionOperationState::DestroyingExistingSession;
	DestroySessionCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(
		this,
		&ThisClass::OnDestroySessionCompleted,
		RequestId);
	DestroySessionCompleteDelegateHandle =
		SessionManager->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);
	if (!SessionManager->DestroySession(NAME_GameSession))
	{
		CleanupOperationDelegateForState(SessionOperationState);
		return false;
	}

	ArmSessionOperationTimeout(RequestId);
	return true;
}

void UOnlineSessionsSubsystem::ArmSessionOperationTimeout(const uint64 RequestId)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(SessionOperationTimeoutHandle);
	World->GetTimerManager().SetTimer(
		SessionOperationTimeoutHandle,
		FTimerDelegate::CreateUObject(
			this,
			&ThisClass::HandleSessionOperationTimeout,
			RequestId),
		FMath::Max(SessionOperationTimeoutSeconds, 1.0f),
		false);
}

void UOnlineSessionsSubsystem::HandleSessionOperationTimeout(const uint64 RequestId)
{
	if (!IsSessionRequestActive(RequestId))
	{
		return;
	}

	if (SessionOperationState == ESessionOperationState::CleaningCanceledSession)
	{
		FinishActiveSessionRequest();
		return;
	}

	if (bActiveRequestCancelRequested)
	{
		return;
	}

	bActiveRequestCancelRequested = true;
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
		return;
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
}

void UOnlineSessionsSubsystem::BroadcastActiveRequestFailure()
{
	if (bActiveRequestCompletionBroadcast || ActiveSessionRequestId == 0)
	{
		return;
	}

	bActiveRequestCompletionBroadcast = true;
	switch (ActiveSessionRequestKind)
	{
	case ESessionRequestKind::CreateRoom:
		OnCreateRoomRequestComplete.Broadcast(ActiveSessionRequestId, false);
		OnCreateSessionComplete.Broadcast(false);
		break;
	case ESessionRequestKind::FindRooms:
		OnFindRoomsRequestComplete.Broadcast(
			ActiveSessionRequestId,
			TArray<FBlueprintSessionResult>(),
			false);
		OnFindSessionsComplete.Broadcast(TArray<FBlueprintSessionResult>(), false);
		break;
	case ESessionRequestKind::JoinRoom:
		OnJoinRoomRequestComplete.Broadcast(ActiveSessionRequestId, false);
		OnJoinSessionComplete.Broadcast(false);
		break;
	case ESessionRequestKind::DestroySession:
		OnDestroySessionRequestComplete.Broadcast(ActiveSessionRequestId, false);
		OnDestroySessionComplete.Broadcast(false);
		break;
	case ESessionRequestKind::QuickMatch:
		OnQuickMatchRequestComplete.Broadcast(ActiveSessionRequestId, false, false);
		break;
	default:
		break;
	}
}

void UOnlineSessionsSubsystem::FinishActiveSessionRequest()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionOperationTimeoutHandle);
	}

	CleanupOperationDelegateForState(SessionOperationState);
	SessionOperationState = ESessionOperationState::Idle;
	ActiveSessionRequestKind = ESessionRequestKind::None;
	ActiveSessionRequestId = 0;
	ActiveRequestLocalPlayer.Reset();
	ActiveRequestLocalPlayerNetId = FUniqueNetIdRepl();
	ActiveRequestControllerId = 0;
	bActiveRequestCancelRequested = false;
	bActiveRequestCompletionBroadcast = false;
	LastSessionSearch.Reset();
	PendingJoinSessionResult = FBlueprintSessionResult();
}

void UOnlineSessionsSubsystem::CleanupOperationDelegateForState(
	const ESessionOperationState OperationState)
{
	if (!SessionManager.IsValid())
	{
		return;
	}

	switch (OperationState)
	{
	case ESessionOperationState::CreatingSession:
	case ESessionOperationState::CancelingCreate:
		if (CreateSessionCompleteDelegateHandle.IsValid())
		{
			SessionManager->ClearOnCreateSessionCompleteDelegate_Handle(
				CreateSessionCompleteDelegateHandle);
			CreateSessionCompleteDelegateHandle.Reset();
		}
		break;
	case ESessionOperationState::FindingSessions:
	case ESessionOperationState::CancelingFind:
		if (FindSessionsCompleteDelegateHandle.IsValid())
		{
			SessionManager->ClearOnFindSessionsCompleteDelegate_Handle(
				FindSessionsCompleteDelegateHandle);
			FindSessionsCompleteDelegateHandle.Reset();
		}
		break;
	case ESessionOperationState::JoiningSession:
	case ESessionOperationState::CancelingJoin:
		if (JoinSessionCompleteDelegateHandle.IsValid())
		{
			SessionManager->ClearOnJoinSessionCompleteDelegate_Handle(
				JoinSessionCompleteDelegateHandle);
			JoinSessionCompleteDelegateHandle.Reset();
		}
		break;
	case ESessionOperationState::DestroyingExistingSession:
	case ESessionOperationState::CancelingDestroy:
	case ESessionOperationState::CleaningCanceledSession:
		if (DestroySessionCompleteDelegateHandle.IsValid())
		{
			SessionManager->ClearOnDestroySessionCompleteDelegate_Handle(
				DestroySessionCompleteDelegateHandle);
			DestroySessionCompleteDelegateHandle.Reset();
		}
		break;
	default:
		break;
	}
}

bool UOnlineSessionsSubsystem::IsCallbackForActiveRequest(
	const uint64 CallbackRequestId) const
{
	return CallbackRequestId != 0
		&& CallbackRequestId == ActiveSessionRequestId;
}

bool UOnlineSessionsSubsystem::IsActiveLocalPlayerIdentityValid() const
{
	if (IsRunningDedicatedServer())
	{
		return true;
	}

	const ULocalPlayer* LocalPlayer = ActiveRequestLocalPlayer.Get();
	if (!LocalPlayer || LocalPlayer->GetControllerId() != ActiveRequestControllerId)
	{
		return false;
	}

	if (!ActiveRequestLocalPlayerNetId.IsValid())
	{
		return true;
	}

	const FUniqueNetIdRepl CurrentNetId = LocalPlayer->GetPreferredUniqueNetId();
	return CurrentNetId.IsValid()
		&& *CurrentNetId == *ActiveRequestLocalPlayerNetId;
}

ULocalPlayer* UOnlineSessionsSubsystem::ResolveDefaultLocalPlayer() const
{
	return GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
}

APlayerController* UOnlineSessionsSubsystem::ResolveActiveLocalPlayerController() const
{
	const ULocalPlayer* LocalPlayer = ActiveRequestLocalPlayer.Get();
	return LocalPlayer && GetWorld()
		? LocalPlayer->GetPlayerController(GetWorld())
		: nullptr;
}

void UOnlineSessionsSubsystem::HandleCanceledCreateOrJoinCompletion(
	const uint64 RequestId)
{
	if (SessionManager.IsValid()
		&& SessionManager->GetNamedSession(NAME_GameSession)
		&& StartDestroySessionPhase(RequestId, true))
	{
		return;
	}

	FinishActiveSessionRequest();
}

void UOnlineSessionsSubsystem::ClearSessionDelegates()
{
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

void UOnlineSessionsSubsystem::OnCreateSessionCompleted(
	FName SessionName,
	const bool bWasSuccessful,
	const uint64 CallbackRequestId)
{
	static_cast<void>(SessionName);

	if (!IsCallbackForActiveRequest(CallbackRequestId))
	{
		return;
	}

	const ESessionRequestKind CompletedRequestKind = ActiveSessionRequestKind;
	const bool bIdentityValid = IsActiveLocalPlayerIdentityValid();
	const bool bCanceled = bActiveRequestCancelRequested || !bIdentityValid;
	CleanupOperationDelegateForState(SessionOperationState);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionOperationTimeoutHandle);
	}

	if (bCanceled)
	{
		if (!bActiveRequestCompletionBroadcast)
		{
			BroadcastActiveRequestFailure();
		}
		HandleCanceledCreateOrJoinCompletion(CallbackRequestId);
		return;
	}

	bActiveRequestCompletionBroadcast = true;
	if (CompletedRequestKind == ESessionRequestKind::QuickMatch)
	{
		OnQuickMatchRequestComplete.Broadcast(
			CallbackRequestId,
			bWasSuccessful,
			bWasSuccessful);
	}
	else if (CompletedRequestKind == ESessionRequestKind::CreateRoom)
	{
		OnCreateRoomRequestComplete.Broadcast(CallbackRequestId, bWasSuccessful);
		OnCreateSessionComplete.Broadcast(bWasSuccessful);
	}

	FinishActiveSessionRequest();
}

void UOnlineSessionsSubsystem::OnFindSessionsCompleted(
	const bool bWasSuccessful,
	const uint64 CallbackRequestId)
{
	if (!IsCallbackForActiveRequest(CallbackRequestId))
	{
		return;
	}

	TArray<FBlueprintSessionResult> Results;
	if (bWasSuccessful && LastSessionSearch.IsValid())
	{
		for (const FOnlineSessionSearchResult& SearchResult : LastSessionSearch->SearchResults)
		{
			FBlueprintSessionResult BlueprintResult;
			BlueprintResult.OnlineResult = SearchResult;
			Results.Add(BlueprintResult);
		}
	}

	const ESessionRequestKind CompletedRequestKind = ActiveSessionRequestKind;
	const bool bIdentityValid = IsActiveLocalPlayerIdentityValid();
	const bool bCanceled = bActiveRequestCancelRequested || !bIdentityValid;
	CleanupOperationDelegateForState(SessionOperationState);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionOperationTimeoutHandle);
	}

	if (bCanceled)
	{
		if (!bActiveRequestCompletionBroadcast)
		{
			BroadcastActiveRequestFailure();
		}
		FinishActiveSessionRequest();
		return;
	}

	if (CompletedRequestKind == ESessionRequestKind::QuickMatch)
	{
		if (bWasSuccessful)
		{
			TArray<const FBlueprintSessionResult*> JoinableResults;
			JoinableResults.Reserve(Results.Num());

			for (const FBlueprintSessionResult& Result : Results)
			{
				if (Result.OnlineResult.IsValid()
					&& Result.OnlineResult.Session.NumOpenPublicConnections > 0)
				{
					JoinableResults.Add(&Result);
				}
			}

			if (!JoinableResults.IsEmpty())
			{
				const int32 SelectedResultIndex = FMath::RandHelper(JoinableResults.Num());
				PendingJoinSessionResult = *JoinableResults[SelectedResultIndex];
				if (StartJoinRoomPhase(CallbackRequestId))
				{
					return;
				}

				BroadcastActiveRequestFailure();
				FinishActiveSessionRequest();
				return;
			}
		}

		if (StartCreateRoomPhase(CallbackRequestId))
		{
			return;
		}

		BroadcastActiveRequestFailure();
		FinishActiveSessionRequest();
		return;
	}

	bActiveRequestCompletionBroadcast = true;
	OnFindRoomsRequestComplete.Broadcast(
		CallbackRequestId,
		Results,
		bWasSuccessful);
	OnFindSessionsComplete.Broadcast(Results, bWasSuccessful);
	FinishActiveSessionRequest();
}

void UOnlineSessionsSubsystem::OnJoinSessionCompleted(
	FName SessionName,
	const EOnJoinSessionCompleteResult::Type Result,
	const uint64 CallbackRequestId)
{
	if (!IsCallbackForActiveRequest(CallbackRequestId))
	{
		return;
	}

	bool bWasSuccessful = Result == EOnJoinSessionCompleteResult::Success;
	FString ConnectString;
	if (bWasSuccessful && SessionManager.IsValid())
	{
		bWasSuccessful = SessionManager->GetResolvedConnectString(SessionName, ConnectString);
	}

	const ESessionRequestKind CompletedRequestKind = ActiveSessionRequestKind;
	const bool bIdentityValid = IsActiveLocalPlayerIdentityValid();
	const bool bCanceled = bActiveRequestCancelRequested || !bIdentityValid;
	CleanupOperationDelegateForState(SessionOperationState);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionOperationTimeoutHandle);
	}

	if (bCanceled)
	{
		if (!bActiveRequestCompletionBroadcast)
		{
			BroadcastActiveRequestFailure();
		}
		HandleCanceledCreateOrJoinCompletion(CallbackRequestId);
		return;
	}

	bActiveRequestCompletionBroadcast = true;
	if (bWasSuccessful)
	{
		if (APlayerController* PlayerController = ResolveActiveLocalPlayerController())
		{
			PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
		}
		else
		{
			bWasSuccessful = false;
		}
	}

	if (CompletedRequestKind == ESessionRequestKind::QuickMatch)
	{
		OnQuickMatchRequestComplete.Broadcast(
			CallbackRequestId,
			bWasSuccessful,
			false);
	}
	else if (CompletedRequestKind == ESessionRequestKind::JoinRoom)
	{
		OnJoinRoomRequestComplete.Broadcast(CallbackRequestId, bWasSuccessful);
		OnJoinSessionComplete.Broadcast(bWasSuccessful);
	}

	FinishActiveSessionRequest();
}

void UOnlineSessionsSubsystem::OnStartSessionCompleted(FName SessionName, const bool bWasSuccessful)
{
	if (SessionManager.IsValid() && StartSessionCompleteDelegateHandle.IsValid())
	{
		SessionManager->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		StartSessionCompleteDelegateHandle.Reset();
	}


	OnStartSessionComplete.Broadcast(bWasSuccessful);
}

void UOnlineSessionsSubsystem::OnEndSessionCompleted(FName SessionName, const bool bWasSuccessful)
{
	if (SessionManager.IsValid() && EndSessionCompleteDelegateHandle.IsValid())
	{
		SessionManager->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
		EndSessionCompleteDelegateHandle.Reset();
	}


	OnEndSessionComplete.Broadcast(bWasSuccessful);
}

void UOnlineSessionsSubsystem::OnDestroySessionCompleted(
	FName SessionName,
	const bool bWasSuccessful,
	const uint64 CallbackRequestId)
{
	static_cast<void>(SessionName);

	if (!IsCallbackForActiveRequest(CallbackRequestId))
	{
		return;
	}

	const ESessionOperationState CompletedOperationState = SessionOperationState;
	const ESessionRequestKind CompletedRequestKind = ActiveSessionRequestKind;
	const bool bIdentityValid = IsActiveLocalPlayerIdentityValid();
	const bool bCanceled = bActiveRequestCancelRequested || !bIdentityValid;
	CleanupOperationDelegateForState(SessionOperationState);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionOperationTimeoutHandle);
	}

	if (CompletedOperationState == ESessionOperationState::CleaningCanceledSession)
	{
		FinishActiveSessionRequest();
		return;
	}

	if (bCanceled)
	{
		if (!bActiveRequestCompletionBroadcast)
		{
			BroadcastActiveRequestFailure();
		}
		FinishActiveSessionRequest();
		return;
	}

	if (CompletedRequestKind == ESessionRequestKind::QuickMatch)
	{
		if (bWasSuccessful && StartFindRoomsPhase(CallbackRequestId))
		{
			return;
		}

		BroadcastActiveRequestFailure();
		FinishActiveSessionRequest();
		return;
	}

	bActiveRequestCompletionBroadcast = true;
	OnDestroySessionRequestComplete.Broadcast(CallbackRequestId, bWasSuccessful);
	OnDestroySessionComplete.Broadcast(bWasSuccessful);
	FinishActiveSessionRequest();
}

void UOnlineSessionsSubsystem::OnUpdateSessionCompleted(FName SessionName, const bool bWasSuccessful)
{
	if (SessionManager.IsValid() && UpdateSessionCompleteDelegateHandle.IsValid())
	{
		SessionManager->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
		UpdateSessionCompleteDelegateHandle.Reset();
	}


	OnUpdateSessionComplete.Broadcast(bWasSuccessful);
}
