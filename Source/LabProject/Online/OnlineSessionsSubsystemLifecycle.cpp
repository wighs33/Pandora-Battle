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

void UOnlineSessionsSubsystem::MarkVoluntaryMatchExit()
{
	bVoluntaryMatchExitInProgress = true;
}

DEFINE_LOG_CATEGORY_STATIC(LogOnlineSessionsSubsystemLifecycle, Log, All);

void UOnlineSessionsSubsystem::StartSession()
{
	if (ActiveSessionLifecycleOperation != ESessionLifecycleOperation::None
		|| SessionOperationState != ESessionOperationState::Idle
		|| !RefreshSessionManager())
	{
		OnStartSessionComplete.Broadcast(false);
		return;
	}
	if (SessionManager->GetSessionState(NAME_GameSession)
		== EOnlineSessionState::InProgress)
	{
		// Starting is idempotent when the backend already reached the target state.
		OnStartSessionComplete.Broadcast(true);
		return;
	}
	if (!SessionManager->GetNamedSession(NAME_GameSession))
	{
		OnStartSessionComplete.Broadcast(false);
		return;
	}

	const uint64 RequestId = BeginSessionLifecycleOperation(
		ESessionLifecycleOperation::Starting);
	if (RequestId == 0 || !ActiveSessionLifecycleManager.IsValid())
	{
		OnStartSessionComplete.Broadcast(false);
		return;
	}

	StartSessionCompleteDelegate =
		FOnStartSessionCompleteDelegate::CreateUObject(
			this,
			&ThisClass::OnStartSessionCompleted,
			RequestId);
	StartSessionCompleteDelegateHandle =
		ActiveSessionLifecycleManager
			->AddOnStartSessionCompleteDelegate_Handle(
				StartSessionCompleteDelegate);
	if (!StartSessionCompleteDelegateHandle.IsValid())
	{
		CompleteSessionLifecycleOperation(
			ESessionLifecycleOperation::Starting,
			RequestId,
			false);
		return;
	}

	const bool bStarted =
		ActiveSessionLifecycleManager->StartSession(NAME_GameSession);
	if (!bStarted)
	{
		if (IsSessionLifecycleOperationActive(
			ESessionLifecycleOperation::Starting,
			RequestId))
		{
			CompleteSessionLifecycleOperation(
				ESessionLifecycleOperation::Starting,
				RequestId,
				false);
		}
		return;
	}

	if (IsSessionLifecycleOperationActive(
		ESessionLifecycleOperation::Starting,
		RequestId))
	{
		ArmSessionLifecycleTimeout(
			ESessionLifecycleOperation::Starting,
			RequestId);
	}
}

void UOnlineSessionsSubsystem::EndSession()
{
	if (ActiveSessionLifecycleOperation != ESessionLifecycleOperation::None
		|| SessionOperationState != ESessionOperationState::Idle
		|| !RefreshSessionManager())
	{
		OnEndSessionComplete.Broadcast(false);
		return;
	}
	const EOnlineSessionState::Type CurrentSessionState =
		SessionManager->GetSessionState(NAME_GameSession);
	if (!SessionManager->GetNamedSession(NAME_GameSession)
		|| CurrentSessionState == EOnlineSessionState::NoSession
		|| CurrentSessionState == EOnlineSessionState::Ended)
	{
		// Ending is idempotent when the session is already absent.
		OnEndSessionComplete.Broadcast(true);
		return;
	}

	const uint64 RequestId = BeginSessionLifecycleOperation(
		ESessionLifecycleOperation::Ending);
	if (RequestId == 0 || !ActiveSessionLifecycleManager.IsValid())
	{
		OnEndSessionComplete.Broadcast(false);
		return;
	}

	EndSessionCompleteDelegate =
		FOnEndSessionCompleteDelegate::CreateUObject(
			this,
			&ThisClass::OnEndSessionCompleted,
			RequestId);
	EndSessionCompleteDelegateHandle =
		ActiveSessionLifecycleManager
			->AddOnEndSessionCompleteDelegate_Handle(
				EndSessionCompleteDelegate);
	if (!EndSessionCompleteDelegateHandle.IsValid())
	{
		CompleteSessionLifecycleOperation(
			ESessionLifecycleOperation::Ending,
			RequestId,
			false);
		return;
	}

	const bool bStarted =
		ActiveSessionLifecycleManager->EndSession(NAME_GameSession);
	if (!bStarted)
	{
		if (IsSessionLifecycleOperationActive(
			ESessionLifecycleOperation::Ending,
			RequestId))
		{
			CompleteSessionLifecycleOperation(
				ESessionLifecycleOperation::Ending,
				RequestId,
				false);
		}
		return;
	}

	if (IsSessionLifecycleOperationActive(
		ESessionLifecycleOperation::Ending,
		RequestId))
	{
		ArmSessionLifecycleTimeout(
			ESessionLifecycleOperation::Ending,
			RequestId);
	}
}

uint64 UOnlineSessionsSubsystem::BeginSessionLifecycleOperation(
	const ESessionLifecycleOperation Operation)
{
	if (Operation == ESessionLifecycleOperation::None
		|| ActiveSessionLifecycleOperation
			!= ESessionLifecycleOperation::None
		|| SessionOperationState != ESessionOperationState::Idle
		|| !SessionManager.IsValid())
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
	ActiveSessionLifecycleManager = SessionManager;
	return RequestId;
}

bool UOnlineSessionsSubsystem::IsSessionLifecycleOperationActive(
	const ESessionLifecycleOperation Operation,
	const uint64 RequestId) const
{
	return Operation != ESessionLifecycleOperation::None
		&& RequestId != 0
		&& ActiveSessionLifecycleOperation == Operation
		&& ActiveSessionLifecycleRequestId == RequestId;
}

void UOnlineSessionsSubsystem::ArmSessionLifecycleTimeout(
	const ESessionLifecycleOperation Operation,
	const uint64 RequestId)
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
	World->GetTimerManager().SetTimer(
		SessionLifecycleTimeoutHandle,
		FTimerDelegate::CreateUObject(
			this,
			&ThisClass::HandleSessionLifecycleTimeout,
			Operation,
			RequestId),
		FMath::Max(SessionOperationTimeoutSeconds, 1.0f),
		false);
}

void UOnlineSessionsSubsystem::HandleSessionLifecycleTimeout(
	const ESessionLifecycleOperation Operation,
	const uint64 RequestId)
{
	if (!IsSessionLifecycleOperationActive(Operation, RequestId))
	{
		return;
	}

	UE_LOG(
		LogOnlineSessionsSubsystemLifecycle,
		Error,
		TEXT("Session %s operation timed out after %.1f seconds."),
		Operation == ESessionLifecycleOperation::Starting
			? TEXT("Start")
			: TEXT("End"),
		FMath::Max(SessionOperationTimeoutSeconds, 1.0f));
	CompleteSessionLifecycleOperation(Operation, RequestId, false);
}

void UOnlineSessionsSubsystem::CompleteSessionLifecycleOperation(
	const ESessionLifecycleOperation Operation,
	const uint64 RequestId,
	const bool bWasSuccessful)
{
	if (!IsSessionLifecycleOperationActive(Operation, RequestId))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			SessionLifecycleTimeoutHandle);
	}

	if (ActiveSessionLifecycleManager.IsValid())
	{
		if (Operation == ESessionLifecycleOperation::Starting
			&& StartSessionCompleteDelegateHandle.IsValid())
		{
			ActiveSessionLifecycleManager
				->ClearOnStartSessionCompleteDelegate_Handle(
					StartSessionCompleteDelegateHandle);
		}
		else if (Operation == ESessionLifecycleOperation::Ending
			&& EndSessionCompleteDelegateHandle.IsValid())
		{
			ActiveSessionLifecycleManager
				->ClearOnEndSessionCompleteDelegate_Handle(
					EndSessionCompleteDelegateHandle);
		}
	}
	StartSessionCompleteDelegateHandle.Reset();
	EndSessionCompleteDelegateHandle.Reset();
	ActiveSessionLifecycleRequestId = 0;
	ActiveSessionLifecycleOperation = ESessionLifecycleOperation::None;
	ActiveSessionLifecycleManager.Reset();

	if (Operation == ESessionLifecycleOperation::Starting)
	{
		OnStartSessionComplete.Broadcast(bWasSuccessful);
	}
	else
	{
		OnEndSessionComplete.Broadcast(bWasSuccessful);
	}
}

void UOnlineSessionsSubsystem::ClearSessionLifecycleOperation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			SessionLifecycleTimeoutHandle);
	}

	if (ActiveSessionLifecycleManager.IsValid())
	{
		if (StartSessionCompleteDelegateHandle.IsValid())
		{
			ActiveSessionLifecycleManager
				->ClearOnStartSessionCompleteDelegate_Handle(
					StartSessionCompleteDelegateHandle);
		}
		if (EndSessionCompleteDelegateHandle.IsValid())
		{
			ActiveSessionLifecycleManager
				->ClearOnEndSessionCompleteDelegate_Handle(
					EndSessionCompleteDelegateHandle);
		}
	}

	StartSessionCompleteDelegateHandle.Reset();
	EndSessionCompleteDelegateHandle.Reset();
	ActiveSessionLifecycleRequestId = 0;
	ActiveSessionLifecycleOperation = ESessionLifecycleOperation::None;
	ActiveSessionLifecycleManager.Reset();
}

void UOnlineSessionsSubsystem::OnStartSessionCompleted(
	const FName SessionName,
	const bool bWasSuccessful,
	const uint64 CallbackRequestId)
{
	if (SessionName != NAME_GameSession)
	{
		return;
	}

	CompleteSessionLifecycleOperation(
		ESessionLifecycleOperation::Starting,
		CallbackRequestId,
		bWasSuccessful);
}

void UOnlineSessionsSubsystem::OnEndSessionCompleted(
	const FName SessionName,
	const bool bWasSuccessful,
	const uint64 CallbackRequestId)
{
	if (SessionName != NAME_GameSession)
	{
		return;
	}

	CompleteSessionLifecycleOperation(
		ESessionLifecycleOperation::Ending,
		CallbackRequestId,
		bWasSuccessful);
}
