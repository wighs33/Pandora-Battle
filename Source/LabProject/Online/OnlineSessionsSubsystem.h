#pragma once

#include "CoreMinimal.h"
#include "Common/GameSessionConstants.h"
#include "FindSessionsCallbackProxy.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OnlineSessionsSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnlineSessionBoolDelegate, bool /*bWasSuccessful*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnlineSessionRequestBoolDelegate, uint64 /*RequestId*/, bool /*bWasSuccessful*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnlineSessionRequestFindDelegate, uint64 /*RequestId*/, const TArray<FBlueprintSessionResult>& /*Results*/, bool /*bWasSuccessful*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnlineSessionQuickMatchDelegate, uint64 /*RequestId*/, bool /*bWasSuccessful*/, bool /*bCreatedRoom*/);

class IOnlineSubsystem;
class ULocalPlayer;

UCLASS(Config = Game)
class LABPROJECT_API UOnlineSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 게임 인스턴스 수명
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// UI가 요청 ID로 결과와 취소를 구분하는 방 작업. 시작 실패는 0을 반환한다.
	uint64 BeginCreateRoomSession(ULocalPlayer* RequestingLocalPlayer, const FString& RoomName, const FString& MapName,
		int32 NumPublicConnections = 6, bool bIsLAN = false);
	uint64 BeginFindRoomSessions(
		ULocalPlayer* RequestingLocalPlayer, int32 MaxSearchResults = 50, bool bIsLAN = false, bool bUseLobbies = true);
	uint64 BeginJoinRoomSession(ULocalPlayer* RequestingLocalPlayer, const FBlueprintSessionResult& SessionResult);
	uint64 BeginDestroySession(ULocalPlayer* RequestingLocalPlayer);
	uint64 BeginQuickMatch(ULocalPlayer* RequestingLocalPlayer, int32 MaxSearchResults, int32 MaxPublicConnections, const FString& RoomName,
		const FString& MapName, bool bIsLAN, bool bUseLobbies);
	bool CancelSessionRequest(uint64 RequestId);
	bool IsSessionRequestActive(uint64 RequestId) const;

	// 온라인 경기 시작·종료와 로비·타이틀 복귀
	UFUNCTION(BlueprintCallable, Category = "!Online|Session")
	void StartSession();

	UFUNCTION(BlueprintCallable, Category = "!Online|Session")
	void EndSession();

	UFUNCTION(BlueprintCallable, Category = "!Online|Session")
	void DestroySession();

	UFUNCTION(BlueprintCallable, Category = "!Online|Session")
	void UpdateSessionSettings(const FString& MapName, int32 NumPublicConnections, bool bAllowJoinInProgress = true);

	UFUNCTION(BlueprintPure, Category = "!Online|Session")
	bool HasNamedSession() const;

	void MarkVoluntaryMatchExit();

	static FName GetRoomNameSettingKey();
	static FName GetMapNameSettingKey();

	// UI와 로비 흐름이 구독하는 완료 알림
	FOnlineSessionBoolDelegate OnStartSessionComplete;
	FOnlineSessionBoolDelegate OnEndSessionComplete;
	FOnlineSessionBoolDelegate OnDestroySessionComplete;
	FOnlineSessionRequestBoolDelegate OnCreateRoomRequestComplete;
	FOnlineSessionRequestFindDelegate OnFindRoomsRequestComplete;
	FOnlineSessionRequestBoolDelegate OnJoinRoomRequestComplete;
	FOnlineSessionRequestBoolDelegate OnDestroySessionRequestComplete;
	FOnlineSessionQuickMatchDelegate OnQuickMatchRequestComplete;

private:
	enum class ESessionRequestKind : uint8
	{
		None,
		CreateRoom,
		FindRooms,
		JoinRoom,
		DestroySession,
		QuickMatch
	};

	enum class ESessionOperationState : uint8
	{
		Idle,
		DestroyingExistingSession,
		FindingSessions,
		CreatingSession,
		JoiningSession,
		CancelingDestroy,
		CancelingFind,
		CancelingCreate,
		CancelingJoin,
		CleaningCanceledSession
	};

	enum class ESessionLifecycleOperation : uint8
	{
		None,
		Starting,
		Ending
	};

	// 방 요청의 단계 전환·취소·정리
	uint64 BeginSessionRequest(ULocalPlayer* RequestingLocalPlayer, ESessionRequestKind RequestKind);
	bool StartCreateRoomPhase(uint64 RequestId);
	bool StartFindRoomsPhase(uint64 RequestId);
	bool StartJoinRoomPhase(uint64 RequestId);
	bool StartDestroySessionPhase(uint64 RequestId, bool bCleanupCanceledSession = false);
	void StartSessionOperationTimeout(uint64 RequestId);
	void HandleSessionOperationTimeout(uint64 RequestId);
	void BroadcastActiveRequestFailure();
	void ResetActiveSessionRequest();
	void ClearSessionOperationDelegate(ESessionOperationState OperationState);
	bool MatchesActiveRequestId(uint64 CallbackRequestId) const;
	bool IsActiveLocalPlayerIdentityValid() const;
	APlayerController* ResolveActiveLocalPlayerController() const;
	void HandleCanceledCreateOrJoinCompletion(uint64 RequestId);
	// 경기 시작·종료의 독립된 작업 수명
	uint64 BeginSessionLifecycleOperation(ESessionLifecycleOperation Operation);
	bool IsSessionLifecycleOperationActive(ESessionLifecycleOperation Operation, uint64 RequestId) const;
	void StartSessionLifecycleTimeout(ESessionLifecycleOperation Operation, uint64 RequestId);
	void HandleSessionLifecycleTimeout(ESessionLifecycleOperation Operation, uint64 RequestId);
	void CompleteSessionLifecycleOperation(ESessionLifecycleOperation Operation, uint64 RequestId, bool bWasSuccessful);
	void ClearSessionLifecycleOperation();
	void ClearSessionDelegates();

	void OnCreateSessionCompleted(FName SessionName, bool bWasSuccessful, uint64 CallbackRequestId);
	void OnFindSessionsCompleted(bool bWasSuccessful, uint64 CallbackRequestId);
	void OnJoinSessionCompleted(FName SessionName, EOnJoinSessionCompleteResult::Type Result, uint64 CallbackRequestId);
	void OnStartSessionCompleted(FName SessionName, bool bWasSuccessful, uint64 CallbackRequestId);
	void OnEndSessionCompleted(FName SessionName, bool bWasSuccessful, uint64 CallbackRequestId);
	void OnDestroySessionCompleted(FName SessionName, bool bWasSuccessful, uint64 CallbackRequestId);
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	bool IsHostConnectionLost(ENetworkFailure::Type FailureType, const FString& ErrorString) const;
	IOnlineSubsystem* GetOnlineSubsystemForWorld() const;
	IOnlineSessionPtr GetSessionInterfaceForWorld() const;
	bool RefreshSessionInterface();
	bool IsNullSubsystemActive() const;
	bool IsSteamSubsystemActive() const;
	bool ShouldUseLANSession(bool bRequestedLAN) const;
	bool ShouldUseLobbySession(bool bRequestedLobbies) const;

	// 현재 월드의 세션 API와 현재 검색 수명
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSearch> ActiveSessionSearch;

	FDelegateHandle CreateSessionCompleteDelegateHandle;
	FDelegateHandle FindSessionsCompleteDelegateHandle;
	FDelegateHandle JoinSessionCompleteDelegateHandle;
	FDelegateHandle StartSessionCompleteDelegateHandle;
	FDelegateHandle EndSessionCompleteDelegateHandle;
	FDelegateHandle DestroySessionCompleteDelegateHandle;
	FDelegateHandle NetworkFailureHandle;

	// 요청 시작 시 복사해 비동기 단계 사이에 전달하는 방 설정
	FString PendingRoomName;
	FString PendingMapName;
	int32 PendingNumPublicConnections = LabGameSession::MaxPlayerCount;
	bool bPendingIsLAN = true;
	int32 PendingMaxSearchResults = 50;
	bool bPendingUseLobbies = true;

	FBlueprintSessionResult PendingJoinSessionResult;

	// 사용자 요청과 취소 상태. 결과 통지는 한 번만 보내고 취소 후 정리는 계속할 수 있다.
	uint64 NextSessionRequestId = 1;
	uint64 ActiveSessionRequestId = 0;
	ESessionRequestKind ActiveSessionRequestKind = ESessionRequestKind::None;
	ESessionOperationState SessionOperationState = ESessionOperationState::Idle;
	TWeakObjectPtr<ULocalPlayer> ActiveRequestLocalPlayer;
	FUniqueNetIdRepl ActiveRequestLocalPlayerNetId;
	int32 ActiveRequestControllerId = 0;
	bool bActiveRequestCancelRequested = false;
	bool bActiveRequestResultSent = false;
	bool bVoluntaryMatchExitInProgress = false;

	// 시작·종료 콜백은 처음 등록한 인터페이스에서 해제한다.
	uint64 NextSessionLifecycleRequestId = 1;
	uint64 ActiveSessionLifecycleRequestId = 0;
	ESessionLifecycleOperation ActiveSessionLifecycleOperation = ESessionLifecycleOperation::None;
	IOnlineSessionPtr LifecycleSessionInterface;

	UPROPERTY(Config, EditAnywhere, Category = "!Online|Session", meta = (ClampMin = "1.0", ForceUnits = "s"))
	float SessionOperationTimeoutSeconds = 30.0f;

	FTimerHandle SessionOperationTimeoutHandle;
	FTimerHandle SessionLifecycleTimeoutHandle;
};
