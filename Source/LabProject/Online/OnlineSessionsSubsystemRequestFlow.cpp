#include "Online/OnlineSessionsSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "TimerManager.h"

// 다른 요청과 경기 시작·종료가 진행 중인지 검사하고, UI 요청 ID와 요청자의 로그인 식별 정보를 저장한다.
uint64 UOnlineSessionsSubsystem::BeginSessionRequest(ULocalPlayer* RequestingLocalPlayer, const ESessionRequestKind RequestKind)
{
	if (SessionOperationState != ESessionOperationState::Idle || ActiveSessionLifecycleOperation != ESessionLifecycleOperation::None
		|| !RefreshSessionInterface() || (!RequestingLocalPlayer && !IsRunningDedicatedServer()))
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
	ActiveRequestLocalPlayerNetId = RequestingLocalPlayer ? RequestingLocalPlayer->GetPreferredUniqueNetId() : FUniqueNetIdRepl();
	ActiveRequestControllerId = RequestingLocalPlayer ? RequestingLocalPlayer->GetControllerId() : 0;
	bActiveRequestCancelRequested = false;
	bActiveRequestResultSent = false;
	return RequestId;
}

// 정원·공개 이름·맵·LAN·Steam 로비 설정을 온라인 서비스에 전달하고 생성 콜백과 제한시간을 등록한다.
bool UOnlineSessionsSubsystem::StartCreateRoomPhase(const uint64 RequestId)
{
	if (!MatchesActiveRequestId(RequestId) || !RefreshSessionInterface() || SessionInterface->GetNamedSession(NAME_GameSession))
	{
		return false;
	}

	ClearSessionOperationDelegate(SessionOperationState);
	SessionOperationState = ESessionOperationState::CreatingSession;
	const FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate =
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionCompleted, RequestId);
	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	FOnlineSessionSettings Settings;
	const bool bDedicatedSession = IsRunningDedicatedServer() || (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer);
	const bool bUseLobbySession = IsSteamSubsystemActive() && !bDedicatedSession;
	Settings.NumPublicConnections = PendingNumPublicConnections;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bIsLANMatch = bPendingIsLAN;
	Settings.bIsDedicated = bDedicatedSession;
	Settings.bUsesPresence = bUseLobbySession;
	Settings.bAllowJoinViaPresence = bUseLobbySession;
	Settings.bUseLobbiesIfAvailable = bUseLobbySession;
	Settings.Set(GetRoomNameSettingKey(), PendingRoomName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(GetMapNameSettingKey(), PendingMapName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	const bool bStarted = ActiveRequestLocalPlayerNetId.IsValid()
		? SessionInterface->CreateSession(*ActiveRequestLocalPlayerNetId, NAME_GameSession, Settings)
		: SessionInterface->CreateSession(ActiveRequestControllerId, NAME_GameSession, Settings);
	if (!bStarted)
	{
		ClearSessionOperationDelegate(SessionOperationState);
		return false;
	}

	StartSessionOperationTimeout(RequestId);
	return true;
}

// 방 생성 결과를 일반 생성 UI 또는 빠른 매칭 UI에 알린다. 취소·계정 변경 뒤의 결과는 성공 이동 대신 세션 정리로 보낸다.
void UOnlineSessionsSubsystem::OnCreateSessionCompleted(FName, const bool bWasSuccessful, const uint64 CallbackRequestId)
{
	if (!MatchesActiveRequestId(CallbackRequestId))
	{
		return;
	}

	const ESessionRequestKind CompletedRequestKind = ActiveSessionRequestKind;
	const bool bIdentityValid = IsActiveLocalPlayerIdentityValid();
	const bool bCanceled = bActiveRequestCancelRequested || !bIdentityValid;
	ClearSessionOperationDelegate(SessionOperationState);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionOperationTimeoutHandle);
	}

	if (bCanceled)
	{
		BroadcastActiveRequestFailure();
		HandleCanceledCreateOrJoinCompletion(CallbackRequestId);
		return;
	}

	bActiveRequestResultSent = true;
	if (CompletedRequestKind == ESessionRequestKind::QuickMatch)
	{
		OnQuickMatchRequestComplete.Broadcast(CallbackRequestId, bWasSuccessful, bWasSuccessful);
	}
	else if (CompletedRequestKind == ESessionRequestKind::CreateRoom)
	{
		OnCreateRoomRequestComplete.Broadcast(CallbackRequestId, bWasSuccessful);
	}

	ResetActiveSessionRequest();
}

// 검색 결과 객체를 만들고 방 목록 또는 빠른 매칭에 필요한 검색을 온라인 서비스에 요청한다.
bool UOnlineSessionsSubsystem::StartFindRoomsPhase(const uint64 RequestId)
{
	if (!MatchesActiveRequestId(RequestId) || !RefreshSessionInterface())
	{
		return false;
	}

	ClearSessionOperationDelegate(SessionOperationState);
	SessionOperationState = ESessionOperationState::FindingSessions;
	const FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate =
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsCompleted, RequestId);
	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	ActiveSessionSearch = MakeShared<FOnlineSessionSearch>();
	ActiveSessionSearch->MaxSearchResults = PendingMaxSearchResults;
	ActiveSessionSearch->bIsLanQuery = bPendingIsLAN;
	if (bPendingUseLobbies)
	{
		ActiveSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	}

	const bool bStarted = ActiveRequestLocalPlayerNetId.IsValid()
		? SessionInterface->FindSessions(*ActiveRequestLocalPlayerNetId, ActiveSessionSearch.ToSharedRef())
		: SessionInterface->FindSessions(ActiveRequestControllerId, ActiveSessionSearch.ToSharedRef());
	if (!bStarted)
	{
		ClearSessionOperationDelegate(SessionOperationState);
		ActiveSessionSearch.Reset();
		return false;
	}

	StartSessionOperationTimeout(RequestId);
	return true;
}

// 일반 검색 결과는 방 목록에 전달한다. 빠른 매칭은 빈 방을 무작위로 선택해 참가하고, 후보가 없거나 검색에 실패하면 방 생성을 시도한다.
void UOnlineSessionsSubsystem::OnFindSessionsCompleted(const bool bWasSuccessful, const uint64 CallbackRequestId)
{
	if (!MatchesActiveRequestId(CallbackRequestId))
	{
		return;
	}

	TArray<FBlueprintSessionResult> Results;
	if (bWasSuccessful && ActiveSessionSearch.IsValid())
	{
		for (const FOnlineSessionSearchResult& SearchResult : ActiveSessionSearch->SearchResults)
		{
			FBlueprintSessionResult BlueprintResult;
			BlueprintResult.OnlineResult = SearchResult;
			Results.Add(BlueprintResult);
		}
	}

	const ESessionRequestKind CompletedRequestKind = ActiveSessionRequestKind;
	const bool bIdentityValid = IsActiveLocalPlayerIdentityValid();
	const bool bCanceled = bActiveRequestCancelRequested || !bIdentityValid;
	ClearSessionOperationDelegate(SessionOperationState);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionOperationTimeoutHandle);
	}

	if (bCanceled)
	{
		BroadcastActiveRequestFailure();
		ResetActiveSessionRequest();
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
				if (Result.OnlineResult.IsValid() && Result.OnlineResult.Session.NumOpenPublicConnections > 0)
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
				ResetActiveSessionRequest();
				return;
			}
		}

		if (StartCreateRoomPhase(CallbackRequestId))
		{
			return;
		}

		BroadcastActiveRequestFailure();
		ResetActiveSessionRequest();
		return;
	}

	bActiveRequestResultSent = true;
	OnFindRoomsRequestComplete.Broadcast(CallbackRequestId, Results, bWasSuccessful);
	ResetActiveSessionRequest();
}

// 선택한 검색 결과로 온라인 세션 참가를 요청하고, 완료 콜백에서 접속 주소를 받도록 준비한다.
bool UOnlineSessionsSubsystem::StartJoinRoomPhase(const uint64 RequestId)
{
	if (!MatchesActiveRequestId(RequestId) || !RefreshSessionInterface() || !PendingJoinSessionResult.OnlineResult.IsValid())
	{
		return false;
	}

	ClearSessionOperationDelegate(SessionOperationState);
	SessionOperationState = ESessionOperationState::JoiningSession;
	const FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate =
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionCompleted, RequestId);
	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	const bool bStarted = ActiveRequestLocalPlayerNetId.IsValid()
		? SessionInterface->JoinSession(*ActiveRequestLocalPlayerNetId, NAME_GameSession, PendingJoinSessionResult.OnlineResult)
		: SessionInterface->JoinSession(ActiveRequestControllerId, NAME_GameSession, PendingJoinSessionResult.OnlineResult);
	if (!bStarted)
	{
		ClearSessionOperationDelegate(SessionOperationState);
		return false;
	}

	StartSessionOperationTimeout(RequestId);
	return true;
}

// 참가 성공 시 접속 주소를 찾아 요청자의 컨트롤러로 ClientTravel을 실행하고 결과를 UI에 알린다.
void UOnlineSessionsSubsystem::OnJoinSessionCompleted(
	FName SessionName, const EOnJoinSessionCompleteResult::Type Result, const uint64 CallbackRequestId)
{
	if (!MatchesActiveRequestId(CallbackRequestId))
	{
		return;
	}

	bool bWasSuccessful = Result == EOnJoinSessionCompleteResult::Success;
	FString ConnectString;
	if (bWasSuccessful && SessionInterface.IsValid())
	{
		bWasSuccessful = SessionInterface->GetResolvedConnectString(SessionName, ConnectString);
	}

	const ESessionRequestKind CompletedRequestKind = ActiveSessionRequestKind;
	const bool bIdentityValid = IsActiveLocalPlayerIdentityValid();
	const bool bCanceled = bActiveRequestCancelRequested || !bIdentityValid;
	ClearSessionOperationDelegate(SessionOperationState);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionOperationTimeoutHandle);
	}

	if (bCanceled)
	{
		BroadcastActiveRequestFailure();
		HandleCanceledCreateOrJoinCompletion(CallbackRequestId);
		return;
	}

	bActiveRequestResultSent = true;
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
		OnQuickMatchRequestComplete.Broadcast(CallbackRequestId, bWasSuccessful, false);
	}
	else if (CompletedRequestKind == ESessionRequestKind::JoinRoom)
	{
		OnJoinRoomRequestComplete.Broadcast(CallbackRequestId, bWasSuccessful);
	}

	ResetActiveSessionRequest();
}

// 현재 방을 삭제한다. 빠른 매칭의 이전 방 정리와 취소 후 뒤늦게 생긴 방 정리에도 같은 단계를 사용한다.
bool UOnlineSessionsSubsystem::StartDestroySessionPhase(const uint64 RequestId, const bool bCleanupCanceledSession)
{
	if (!MatchesActiveRequestId(RequestId) || !RefreshSessionInterface() || !SessionInterface->GetNamedSession(NAME_GameSession))
	{
		return false;
	}

	ClearSessionOperationDelegate(SessionOperationState);
	SessionOperationState =
		bCleanupCanceledSession ? ESessionOperationState::CleaningCanceledSession : ESessionOperationState::DestroyingExistingSession;
	const FOnDestroySessionCompleteDelegate DestroySessionCompleteDelegate =
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionCompleted, RequestId);
	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);
	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		ClearSessionOperationDelegate(SessionOperationState);
		return false;
	}

	StartSessionOperationTimeout(RequestId);
	return true;
}

// 삭제 결과를 나가기 UI에 알리거나, 빠른 매칭이면 다음 검색을 시작한다. 취소 후 정리 작업은 추가 UI 성공 알림 없이 끝낸다.
void UOnlineSessionsSubsystem::OnDestroySessionCompleted(FName, const bool bWasSuccessful, const uint64 CallbackRequestId)
{
	if (!MatchesActiveRequestId(CallbackRequestId))
	{
		return;
	}

	const ESessionOperationState CompletedOperationState = SessionOperationState;
	const ESessionRequestKind CompletedRequestKind = ActiveSessionRequestKind;
	const bool bIdentityValid = IsActiveLocalPlayerIdentityValid();
	const bool bCanceled = bActiveRequestCancelRequested || !bIdentityValid;
	ClearSessionOperationDelegate(SessionOperationState);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionOperationTimeoutHandle);
	}

	if (CompletedOperationState == ESessionOperationState::CleaningCanceledSession)
	{
		ResetActiveSessionRequest();
		return;
	}

	if (bCanceled)
	{
		BroadcastActiveRequestFailure();
		ResetActiveSessionRequest();
		return;
	}

	if (CompletedRequestKind == ESessionRequestKind::QuickMatch)
	{
		if (bWasSuccessful && StartFindRoomsPhase(CallbackRequestId))
		{
			return;
		}

		BroadcastActiveRequestFailure();
		ResetActiveSessionRequest();
		return;
	}

	bActiveRequestResultSent = true;
	OnDestroySessionRequestComplete.Broadcast(CallbackRequestId, bWasSuccessful);
	OnDestroySessionComplete.Broadcast(bWasSuccessful);
	ResetActiveSessionRequest();
}

// 취소 후 생성·참가가 늦게 완료되어 생긴 온라인 세션을 삭제한다. 정리할 세션이 없으면 요청을 바로 비운다.
void UOnlineSessionsSubsystem::HandleCanceledCreateOrJoinCompletion(const uint64 RequestId)
{
	if (SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession) && StartDestroySessionPhase(RequestId, true))
	{
		return;
	}

	ResetActiveSessionRequest();
}

// 현재 생성·검색·참가·삭제 단계의 응답 제한시간을 시작해 UI가 응답 없이 계속 기다리지 않게 한다.
void UOnlineSessionsSubsystem::StartSessionOperationTimeout(const uint64 RequestId)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(SessionOperationTimeoutHandle);
	World->GetTimerManager().SetTimer(SessionOperationTimeoutHandle,
		FTimerDelegate::CreateUObject(this, &ThisClass::HandleSessionOperationTimeout, RequestId),
		FMath::Max(SessionOperationTimeoutSeconds, 1.0f), false);
}

// 제한시간이 지나면 UI 취소와 같은 경로로 실패를 처리한다. 취소된 세션의 정리 자체가 지연되면 요청 상태를 비운다.
void UOnlineSessionsSubsystem::HandleSessionOperationTimeout(const uint64 RequestId)
{
	if (!IsSessionRequestActive(RequestId))
	{
		return;
	}

	if (SessionOperationState == ESessionOperationState::CleaningCanceledSession)
	{
		ResetActiveSessionRequest();
		return;
	}

	CancelSessionRequest(RequestId);
}

// 현재 요청 종류에 맞는 UI 완료 이벤트를 실패로 한 번만 보낸다. 취소 이후의 서버 정리는 별도로 계속될 수 있다.
void UOnlineSessionsSubsystem::BroadcastActiveRequestFailure()
{
	if (bActiveRequestResultSent || ActiveSessionRequestId == 0)
	{
		return;
	}

	bActiveRequestResultSent = true;
	switch (ActiveSessionRequestKind)
	{
	case ESessionRequestKind::CreateRoom:
		OnCreateRoomRequestComplete.Broadcast(ActiveSessionRequestId, false);
		break;
	case ESessionRequestKind::FindRooms:
		OnFindRoomsRequestComplete.Broadcast(ActiveSessionRequestId, TArray<FBlueprintSessionResult>(), false);
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

// 완료·실패·정리 후 타이머·콜백·검색 결과·요청자 정보를 비워 다음 UI 요청을 받을 수 있게 한다.
void UOnlineSessionsSubsystem::ResetActiveSessionRequest()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionOperationTimeoutHandle);
	}

	ClearSessionOperationDelegate(SessionOperationState);
	SessionOperationState = ESessionOperationState::Idle;
	ActiveSessionRequestKind = ESessionRequestKind::None;
	ActiveSessionRequestId = 0;
	ActiveRequestLocalPlayer.Reset();
	ActiveRequestLocalPlayerNetId = FUniqueNetIdRepl();
	ActiveRequestControllerId = 0;
	bActiveRequestCancelRequested = false;
	bActiveRequestResultSent = false;
	ActiveSessionSearch.Reset();
	PendingJoinSessionResult = FBlueprintSessionResult();
}

// 직전 작업 단계에 등록했던 콜백만 해제해 빠른 매칭의 다음 단계에 이전 콜백이 남지 않게 한다.
void UOnlineSessionsSubsystem::ClearSessionOperationDelegate(const ESessionOperationState OperationState)
{
	if (!SessionInterface.IsValid())
	{
		return;
	}

	switch (OperationState)
	{
	case ESessionOperationState::CreatingSession:
	case ESessionOperationState::CancelingCreate:
		if (CreateSessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
			CreateSessionCompleteDelegateHandle.Reset();
		}
		break;
	case ESessionOperationState::FindingSessions:
	case ESessionOperationState::CancelingFind:
		if (FindSessionsCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
			FindSessionsCompleteDelegateHandle.Reset();
		}
		break;
	case ESessionOperationState::JoiningSession:
	case ESessionOperationState::CancelingJoin:
		if (JoinSessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
			JoinSessionCompleteDelegateHandle.Reset();
		}
		break;
	case ESessionOperationState::DestroyingExistingSession:
	case ESessionOperationState::CancelingDestroy:
	case ESessionOperationState::CleaningCanceledSession:
		if (DestroySessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
			DestroySessionCompleteDelegateHandle.Reset();
		}
		break;
	default:
		break;
	}
}

// 콜백이나 다음 단계가 현재 요청 ID에 속하는지 확인한다. 단계가 아직 Idle인 요청 시작 직후에도 사용할 수 있다.
bool UOnlineSessionsSubsystem::MatchesActiveRequestId(const uint64 CallbackRequestId) const
{
	return CallbackRequestId != 0 && CallbackRequestId == ActiveSessionRequestId;
}

// 요청을 보낸 로컬 플레이어와 로그인 계정이 그대로인지 확인한다. 대기 중 계정이 바뀌면 해당 결과로 참가하지 않는다.
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
	return CurrentNetId.IsValid() && *CurrentNetId == *ActiveRequestLocalPlayerNetId;
}

// 참가를 요청했던 로컬 플레이어의 현재 컨트롤러를 찾아 올바른 화면을 서버 주소로 이동시킨다.
APlayerController* UOnlineSessionsSubsystem::ResolveActiveLocalPlayerController() const
{
	const ULocalPlayer* LocalPlayer = ActiveRequestLocalPlayer.Get();
	return LocalPlayer && GetWorld() ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
}
