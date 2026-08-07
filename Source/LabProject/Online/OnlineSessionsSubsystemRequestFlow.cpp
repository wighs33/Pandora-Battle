#include "Online/OnlineSessionsSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetDriver.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogOnlineSessionsSubsystem, Log, All);

namespace LabOnlineSession
{
	// Internal-linkage copies used by the request-flow translation unit.
	const FName RoomNameSettingKey(TEXT("ROOM_NAME"));
	const FName MapNameSettingKey(TEXT("MAP_NAME"));
}

uint64 UOnlineSessionsSubsystem::BeginSessionRequest(
	ULocalPlayer* RequestingLocalPlayer,
	const ESessionRequestKind RequestKind)
{
	if (SessionOperationState != ESessionOperationState::Idle
		|| ActiveSessionLifecycleOperation
			!= ESessionLifecycleOperation::None
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
	if (RequestKind != ESessionRequestKind::DestroySession)
	{
		bVoluntaryMatchExitInProgress = false;
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
		break;
	case ESessionRequestKind::FindRooms:
		OnFindRoomsRequestComplete.Broadcast(
			ActiveSessionRequestId,
			TArray<FBlueprintSessionResult>(),
			false);
		break;
	case ESessionRequestKind::JoinRoom:
		OnJoinRoomRequestComplete.Broadcast(ActiveSessionRequestId, false);
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
	}

	FinishActiveSessionRequest();
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
