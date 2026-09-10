#include "Online/OnlineSessionsSubsystem.h"
#include "Engine/World.h"
#include "OnlineSessionSettings.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnlineSessionsSubsystemLifecycle, Log, All);

// 사용자가 직접 경기에서 나가는 중임을 기록해 뒤따르는 연결 종료를 비정상 이탈 결과로 표시하지 않게 한다.
void UOnlineSessionsSubsystem::MarkVoluntaryMatchExit()
{
	bVoluntaryMatchExitInProgress = true;
}

// 로비의 Start 동작을 온라인 서비스의 경기 시작 상태로 전환한다. 완료 이벤트를 받은 로비 흐름이 전장 이동을 진행한다.
void UOnlineSessionsSubsystem::StartSession()
{
	if (ActiveSessionLifecycleOperation != ESessionLifecycleOperation::None || SessionOperationState != ESessionOperationState::Idle
		|| !RefreshSessionInterface())
	{
		OnStartSessionComplete.Broadcast(false);
		return;
	}
	if (SessionInterface->GetSessionState(NAME_GameSession) == EOnlineSessionState::InProgress)
	{
		// Starting is idempotent when the backend already reached the target state.
		OnStartSessionComplete.Broadcast(true);
		return;
	}
	if (!SessionInterface->GetNamedSession(NAME_GameSession))
	{
		OnStartSessionComplete.Broadcast(false);
		return;
	}

	const uint64 RequestId = BeginSessionLifecycleOperation(ESessionLifecycleOperation::Starting);
	if (RequestId == 0 || !LifecycleSessionInterface.IsValid())
	{
		OnStartSessionComplete.Broadcast(false);
		return;
	}

	const FOnStartSessionCompleteDelegate StartSessionCompleteDelegate =
		FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionCompleted, RequestId);
	StartSessionCompleteDelegateHandle = LifecycleSessionInterface->AddOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegate);
	if (!StartSessionCompleteDelegateHandle.IsValid())
	{
		CompleteSessionLifecycleOperation(ESessionLifecycleOperation::Starting, RequestId, false);
		return;
	}

	const bool bStarted = LifecycleSessionInterface->StartSession(NAME_GameSession);
	if (!bStarted)
	{
		if (IsSessionLifecycleOperationActive(ESessionLifecycleOperation::Starting, RequestId))
		{
			CompleteSessionLifecycleOperation(ESessionLifecycleOperation::Starting, RequestId, false);
		}
		return;
	}

	if (IsSessionLifecycleOperationActive(ESessionLifecycleOperation::Starting, RequestId))
	{
		StartSessionLifecycleTimeout(ESessionLifecycleOperation::Starting, RequestId);
	}
}

// GameSession의 시작 완료만 받아 현재 시작 작업을 마무리하고 로비의 전장 진입 흐름에 결과를 전달한다.
void UOnlineSessionsSubsystem::OnStartSessionCompleted(const FName SessionName, const bool bWasSuccessful, const uint64 CallbackRequestId)
{
	if (SessionName != NAME_GameSession)
	{
		return;
	}

	CompleteSessionLifecycleOperation(ESessionLifecycleOperation::Starting, CallbackRequestId, bWasSuccessful);
}

// 경기 결과 화면에서 온라인 경기를 종료 상태로 전환하고, UI가 후속 로비 복귀를 진행하도록 결과를 알린다.
void UOnlineSessionsSubsystem::EndSession()
{
	if (ActiveSessionLifecycleOperation != ESessionLifecycleOperation::None || SessionOperationState != ESessionOperationState::Idle
		|| !RefreshSessionInterface())
	{
		OnEndSessionComplete.Broadcast(false);
		return;
	}
	const EOnlineSessionState::Type CurrentSessionState = SessionInterface->GetSessionState(NAME_GameSession);
	if (!SessionInterface->GetNamedSession(NAME_GameSession) || CurrentSessionState == EOnlineSessionState::NoSession
		|| CurrentSessionState == EOnlineSessionState::Ended)
	{
		// Ending is idempotent when the session is already absent.
		OnEndSessionComplete.Broadcast(true);
		return;
	}

	const uint64 RequestId = BeginSessionLifecycleOperation(ESessionLifecycleOperation::Ending);
	if (RequestId == 0 || !LifecycleSessionInterface.IsValid())
	{
		OnEndSessionComplete.Broadcast(false);
		return;
	}

	const FOnEndSessionCompleteDelegate EndSessionCompleteDelegate =
		FOnEndSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnEndSessionCompleted, RequestId);
	EndSessionCompleteDelegateHandle = LifecycleSessionInterface->AddOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegate);
	if (!EndSessionCompleteDelegateHandle.IsValid())
	{
		CompleteSessionLifecycleOperation(ESessionLifecycleOperation::Ending, RequestId, false);
		return;
	}

	const bool bStarted = LifecycleSessionInterface->EndSession(NAME_GameSession);
	if (!bStarted)
	{
		if (IsSessionLifecycleOperationActive(ESessionLifecycleOperation::Ending, RequestId))
		{
			CompleteSessionLifecycleOperation(ESessionLifecycleOperation::Ending, RequestId, false);
		}
		return;
	}

	if (IsSessionLifecycleOperationActive(ESessionLifecycleOperation::Ending, RequestId))
	{
		StartSessionLifecycleTimeout(ESessionLifecycleOperation::Ending, RequestId);
	}
}

// GameSession의 종료 완료만 받아 현재 종료 작업을 마무리하고 결과 화면의 후속 이동에 결과를 전달한다.
void UOnlineSessionsSubsystem::OnEndSessionCompleted(const FName SessionName, const bool bWasSuccessful, const uint64 CallbackRequestId)
{
	if (SessionName != NAME_GameSession)
	{
		return;
	}

	CompleteSessionLifecycleOperation(ESessionLifecycleOperation::Ending, CallbackRequestId, bWasSuccessful);
}

// 방 요청과 겹치지 않게 경기 시작·종료 작업 ID를 발급하고, 콜백을 등록할 세션 인터페이스를 고정한다.
uint64 UOnlineSessionsSubsystem::BeginSessionLifecycleOperation(const ESessionLifecycleOperation Operation)
{
	if (Operation == ESessionLifecycleOperation::None || ActiveSessionLifecycleOperation != ESessionLifecycleOperation::None
		|| SessionOperationState != ESessionOperationState::Idle || !SessionInterface.IsValid())
	{
		return 0;
	}

	const uint64 RequestId = NextSessionLifecycleRequestId++;
	if (NextSessionLifecycleRequestId == 0)
	{
		NextSessionLifecycleRequestId = 1;
	}

	ActiveSessionLifecycleRequestId = RequestId;
	ActiveSessionLifecycleOperation = Operation;
	LifecycleSessionInterface = SessionInterface;
	return RequestId;
}

// 시작·종료 콜백과 제한시간이 현재 작업 종류·ID와 일치하는지 검사해 오래된 결과를 무시한다.
bool UOnlineSessionsSubsystem::IsSessionLifecycleOperationActive(const ESessionLifecycleOperation Operation, const uint64 RequestId) const
{
	return Operation != ESessionLifecycleOperation::None && RequestId != 0 && ActiveSessionLifecycleOperation == Operation
		&& ActiveSessionLifecycleRequestId == RequestId;
}

// 경기 시작·종료 응답의 제한시간을 등록한다. 기다릴 월드가 없으면 해당 작업을 실패로 끝낸다.
void UOnlineSessionsSubsystem::StartSessionLifecycleTimeout(const ESessionLifecycleOperation Operation, const uint64 RequestId)
{
	UWorld* World = GetWorld();
	if (!IsSessionLifecycleOperationActive(Operation, RequestId))
	{
		return;
	}
	if (!World)
	{
		CompleteSessionLifecycleOperation(Operation, RequestId, false);
		return;
	}

	World->GetTimerManager().ClearTimer(SessionLifecycleTimeoutHandle);
	World->GetTimerManager().SetTimer(SessionLifecycleTimeoutHandle,
		FTimerDelegate::CreateUObject(this, &ThisClass::HandleSessionLifecycleTimeout, Operation, RequestId),
		FMath::Max(SessionOperationTimeoutSeconds, 1.0f), false);
}

// 경기 시작·종료의 응답이 지연되면 로그를 남기고 실패 완료를 알려 로비·결과 화면의 대기를 해제한다.
void UOnlineSessionsSubsystem::HandleSessionLifecycleTimeout(const ESessionLifecycleOperation Operation, const uint64 RequestId)
{
	if (!IsSessionLifecycleOperationActive(Operation, RequestId))
	{
		return;
	}

	UE_LOG(LogOnlineSessionsSubsystemLifecycle, Error, TEXT("Session %s operation timed out after %.1f seconds."),
		Operation == ESessionLifecycleOperation::Starting ? TEXT("Start") : TEXT("End"), FMath::Max(SessionOperationTimeoutSeconds, 1.0f));
	CompleteSessionLifecycleOperation(Operation, RequestId, false);
}

// 현재 시작·종료 작업의 콜백과 상태를 먼저 비운 뒤 완료 이벤트를 보내, 구독자가 다음 작업을 시작할 수 있게 한다.
void UOnlineSessionsSubsystem::CompleteSessionLifecycleOperation(
	const ESessionLifecycleOperation Operation, const uint64 RequestId, const bool bWasSuccessful)
{
	if (!IsSessionLifecycleOperationActive(Operation, RequestId))
	{
		return;
	}

	ClearSessionLifecycleOperation();

	if (Operation == ESessionLifecycleOperation::Starting)
	{
		OnStartSessionComplete.Broadcast(bWasSuccessful);
	}
	else
	{
		OnEndSessionComplete.Broadcast(bWasSuccessful);
	}
}

// 시작·종료 콜백을 실제 등록한 인터페이스에서 해제하고, 타이머와 작업 ID를 함께 초기화한다.
void UOnlineSessionsSubsystem::ClearSessionLifecycleOperation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionLifecycleTimeoutHandle);
	}

	if (LifecycleSessionInterface.IsValid())
	{
		if (StartSessionCompleteDelegateHandle.IsValid())
		{
			LifecycleSessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		}
		if (EndSessionCompleteDelegateHandle.IsValid())
		{
			LifecycleSessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
		}
	}

	StartSessionCompleteDelegateHandle.Reset();
	EndSessionCompleteDelegateHandle.Reset();
	ActiveSessionLifecycleRequestId = 0;
	ActiveSessionLifecycleOperation = ESessionLifecycleOperation::None;
	LifecycleSessionInterface.Reset();
}
