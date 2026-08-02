#pragma once

#include "CoreMinimal.h"
#include "Common/GameSessionConstants.h"
#include "FindSessionsCallbackProxy.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OnlineSessionsSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnlineSessionBoolDelegate, bool /*bWasSuccessful*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnlineSessionFindDelegate, const TArray<FBlueprintSessionResult>& /*Results*/, bool /*bWasSuccessful*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnlineSessionRequestBoolDelegate, uint64 /*RequestId*/, bool /*bWasSuccessful*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnlineSessionRequestFindDelegate,
	uint64 /*RequestId*/,
	const TArray<FBlueprintSessionResult>& /*Results*/,
	bool /*bWasSuccessful*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnlineSessionQuickMatchDelegate,
	uint64 /*RequestId*/,
	bool /*bWasSuccessful*/,
	bool /*bCreatedRoom*/);

class IOnlineSubsystem;
class ULocalPlayer;

UCLASS(Config = Game)
class LABPROJECT_API UOnlineSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UOnlineSessionsSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "!Online|Session")
	void CreateRoomSession(const FString& RoomName, const FString& MapName, int32 NumPublicConnections = 6, bool bIsLAN = false);

	UFUNCTION(BlueprintCallable, Category = "!Online|Session")
	void FindRoomSessions(int32 MaxSearchResults = 50, bool bIsLAN = false, bool bUseLobbies = true);

	UFUNCTION(BlueprintCallable, Category = "!Online|Session")
	void JoinRoomSession(const FBlueprintSessionResult& SessionResult);

	UFUNCTION(BlueprintCallable, Category = "!Online|Session")
	void CancelPendingJoinSession();

	uint64 BeginCreateRoomSession(
		ULocalPlayer* RequestingLocalPlayer,
		const FString& RoomName,
		const FString& MapName,
		int32 NumPublicConnections = 6,
		bool bIsLAN = false);
	uint64 BeginFindRoomSessions(
		ULocalPlayer* RequestingLocalPlayer,
		int32 MaxSearchResults = 50,
		bool bIsLAN = false,
		bool bUseLobbies = true);
	uint64 BeginJoinRoomSession(
		ULocalPlayer* RequestingLocalPlayer,
		const FBlueprintSessionResult& SessionResult);
	uint64 BeginDestroySession(ULocalPlayer* RequestingLocalPlayer);
	uint64 BeginQuickMatch(
		ULocalPlayer* RequestingLocalPlayer,
		int32 MaxSearchResults,
		int32 MaxPublicConnections,
		const FString& RoomName,
		const FString& MapName,
		bool bIsLAN,
		bool bUseLobbies);
	bool CancelSessionRequest(uint64 RequestId);
	bool IsSessionRequestActive(uint64 RequestId) const;

	UFUNCTION(BlueprintCallable, Category = "!Online|Session")
	void StartSession();

	UFUNCTION(BlueprintCallable, Category = "!Online|Session")
	void EndSession();

	UFUNCTION(BlueprintCallable, Category = "!Online|Session")
	void DestroySession();

	UFUNCTION(BlueprintCallable, Category = "!Online|Session")
	void UpdateSessionMapName(const FString& MapName, bool bAllowJoinInProgress = true);

	UFUNCTION(BlueprintCallable, Category = "!Online|Session")
	void UpdateSessionSettings(const FString& MapName, int32 NumPublicConnections, bool bAllowJoinInProgress = true);

	UFUNCTION(BlueprintPure, Category = "!Online|Session")
	bool HasNamedSession() const;

	UFUNCTION(BlueprintPure, Category = "!Online|Session")
	FString GetOnlineSubsystemName() const;

	static FName GetRoomNameSettingKey();
	static FName GetMapNameSettingKey();

	FOnlineSessionBoolDelegate OnCreateSessionComplete;
	FOnlineSessionFindDelegate OnFindSessionsComplete;
	FOnlineSessionBoolDelegate OnJoinSessionComplete;
	FOnlineSessionBoolDelegate OnStartSessionComplete;
	FOnlineSessionBoolDelegate OnEndSessionComplete;
	FOnlineSessionBoolDelegate OnDestroySessionComplete;
	FOnlineSessionBoolDelegate OnUpdateSessionComplete;
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

	uint64 BeginSessionRequest(ULocalPlayer* RequestingLocalPlayer, ESessionRequestKind RequestKind);
	bool StartCreateRoomPhase(uint64 RequestId);
	bool StartFindRoomsPhase(uint64 RequestId);
	bool StartJoinRoomPhase(uint64 RequestId);
	bool StartDestroySessionPhase(uint64 RequestId, bool bCleanupCanceledSession = false);
	void ArmSessionOperationTimeout(uint64 RequestId);
	void HandleSessionOperationTimeout(uint64 RequestId);
	void BroadcastActiveRequestFailure();
	void FinishActiveSessionRequest();
	void CleanupOperationDelegateForState(ESessionOperationState OperationState);
	bool IsCallbackForActiveRequest(uint64 CallbackRequestId) const;
	bool IsActiveLocalPlayerIdentityValid() const;
	ULocalPlayer* ResolveDefaultLocalPlayer() const;
	APlayerController* ResolveActiveLocalPlayerController() const;
	void HandleCanceledCreateOrJoinCompletion(uint64 RequestId);
	void ClearSessionDelegates();

	void OnCreateSessionCompleted(FName SessionName, bool bWasSuccessful, uint64 CallbackRequestId);
	void OnFindSessionsCompleted(bool bWasSuccessful, uint64 CallbackRequestId);
	void OnJoinSessionCompleted(
		FName SessionName,
		EOnJoinSessionCompleteResult::Type Result,
		uint64 CallbackRequestId);
	void OnStartSessionCompleted(FName SessionName, bool bWasSuccessful);
	void OnEndSessionCompleted(FName SessionName, bool bWasSuccessful);
	void OnDestroySessionCompleted(FName SessionName, bool bWasSuccessful, uint64 CallbackRequestId);
	void OnUpdateSessionCompleted(FName SessionName, bool bWasSuccessful);
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	bool IsExpectedConnectionClose(ENetworkFailure::Type FailureType, const FString& ErrorString) const;
	IOnlineSubsystem* GetOnlineSubsystemForWorld() const;
	IOnlineSessionPtr GetSessionManagerForWorld() const;
	bool RefreshSessionManager();
	bool IsNullSubsystemActive() const;
	bool IsSteamSubsystemActive() const;
	bool ResolveLanSession(bool bRequestedLAN) const;
	bool ResolveLobbySession(bool bRequestedLobbies) const;

	IOnlineSessionPtr SessionManager;
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;

	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
	FOnStartSessionCompleteDelegate StartSessionCompleteDelegate;
	FOnEndSessionCompleteDelegate EndSessionCompleteDelegate;
	FOnDestroySessionCompleteDelegate DestroySessionCompleteDelegate;
	FOnUpdateSessionCompleteDelegate UpdateSessionCompleteDelegate;

	FDelegateHandle CreateSessionCompleteDelegateHandle;
	FDelegateHandle FindSessionsCompleteDelegateHandle;
	FDelegateHandle JoinSessionCompleteDelegateHandle;
	FDelegateHandle StartSessionCompleteDelegateHandle;
	FDelegateHandle EndSessionCompleteDelegateHandle;
	FDelegateHandle DestroySessionCompleteDelegateHandle;
	FDelegateHandle UpdateSessionCompleteDelegateHandle;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;

	FString PendingRoomName;
	FString PendingMapName;
	int32 PendingNumPublicConnections = LabGameSession::MaxPlayerCount;
	bool bPendingIsLAN = true;
	int32 PendingMaxSearchResults = 50;
	bool bPendingUseLobbies = true;

	FBlueprintSessionResult PendingJoinSessionResult;

	uint64 NextSessionRequestId = 1;
	uint64 ActiveSessionRequestId = 0;
	ESessionRequestKind ActiveSessionRequestKind = ESessionRequestKind::None;
	ESessionOperationState SessionOperationState = ESessionOperationState::Idle;
	TWeakObjectPtr<ULocalPlayer> ActiveRequestLocalPlayer;
	FUniqueNetIdRepl ActiveRequestLocalPlayerNetId;
	int32 ActiveRequestControllerId = 0;
	bool bActiveRequestCancelRequested = false;
	bool bActiveRequestCompletionBroadcast = false;

	UPROPERTY(Config, EditAnywhere, Category = "!Online|Session", meta = (ClampMin = "1.0", ForceUnits = "s"))
	float SessionOperationTimeoutSeconds = 30.0f;

	FTimerHandle SessionOperationTimeoutHandle;
};
