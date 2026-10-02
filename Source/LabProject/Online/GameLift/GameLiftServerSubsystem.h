#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Online/Backend/AwsSigV4.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameLiftServerSubsystem.generated.h"

class AController;
class AGameModeBase;
class APlayerController;
class FGameLiftServerSDKModule;
struct FProcessParameters;

/**
 * 전용 서버 프로세스와 Amazon GameLift Servers의 수명 주기를 연결한다.
 *
 * - 시작: InitSDK 후, 로비가 접속을 받을 준비가 되면 ProcessReady를 보낸다.
 * - 게임 세션 배정: 게임 속성 mode로 나눈다.
 *   - match: 이미 열린 로비가 받으므로 바로 ActivateGameSession을 호출한다.
 *   - rpg: 공유 월드(ULevelDefinition::RpgLevel)로 이동하고, 그 맵이 준비되면 활성화한다.
 * - 접속: PreLogin에서 PlayerSessionId를 AcceptPlayerSession으로 검증하고, 퇴장하면 RemovePlayerSession을 호출한다.
 * - 종료: 경기 세션은 한 경기만 진행한다. 결과 보고와 참가자 퇴장을 기다린 뒤 ProcessEnding으로 프로세스를 끝내고,
 *   GameLift가 새 프로세스를 띄운다. RPG 세션은 모두 나간 뒤 일정 시간 비어 있으면 끝낸다.
 *
 * -glAnywhere(Anywhere 플릿) 또는 -GameLift(관리형 플릿) 실행 인자가 있고 SDK와 함께 빌드된 Server target에서만 동작한다.
 * 그 밖의 경우 모든 요청이 그대로 통과하므로 로컬 전용 서버와 Listen Server의 흐름은 바뀌지 않는다.
 */
UCLASS()
class LABPROJECT_API UGameLiftServerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Public API ------------------------------------------------------------------------------------------------------
	/** 전용 서버 프로세스의 서브시스템. GameLift가 비활성이어도 돌려주며, 호출은 안전하게 무시된다. */
	static UGameLiftServerSubsystem* Get(const UObject* WorldContextObject);

	bool IsGameLiftActive() const { return bSdkInitialized; }
	const FString& GetGameSessionId() const { return GameSessionId; }

	/** 로비 맵이 접속을 받기 시작하면 한 번 ProcessReady를 보낸다. */
	void NotifyServerReadyForSessions(const UWorld* World);

	/** RPG 공유 월드가 플레이어를 받을 준비가 되면 호출한다. 활성화를 기다리는 세션이 없으면 무시된다. */
	void ActivatePendingGameSession();

	/** 엔진과 GameMode의 검사를 통과한 접속만 GameLift player session으로 검증한다. */
	void ValidatePlayerJoin(const FString& Options, FString& InOutErrorMessage);

	/** 경기가 시작되면 새 참가를 막고, 시작이 취소되면 다시 연다. */
	void SetPlayerSessionCreationPolicy(bool bAcceptNewPlayers);

	/** 결과 보고가 끝나고 참가자가 모두 나가면(또는 제한 시간이 지나면) 프로세스를 끝낸다. */
	void RequestSessionEnd(const FString& Reason);

	/**
	 * 결과 보고 서명에 쓸 자격 증명을 고른다.
	 * Anywhere 실행 인자 → 관리형 플릿 역할(GetFleetRoleCredentials) → AWS_* 환경 변수 순서다.
	 */
	bool TryGetServiceCredentials(FAwsCredentials& OutCredentials);

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void HandleLogout(AGameModeBase* GameMode, AController* Exiting);
	void HandleGameSessionStarted(const FString& InGameSessionId, int32 MaxPlayerSessionCount, const FString& SessionMode);
	bool TickSessionWatchdog(float DeltaSeconds);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	bool InitializeSdk();
	bool TravelToRpgWorld();
	void ActivateSession(const FString& InGameSessionId, int32 MaxPlayerSessionCount);
	void EndProcess(const FString& Reason);
	void ShutdownSdk();

	FGameLiftServerSDKModule* SdkModule = nullptr;
	TSharedPtr<FProcessParameters> ProcessParameters;
	FTSTicker::FDelegateHandle WatchdogHandle;
	FDelegateHandle PostLoginHandle;
	FDelegateHandle LogoutHandle;

	bool bSdkInitialized = false;
	bool bProcessReadySent = false;
	bool bEnding = false;
	FString GameSessionId;
	bool bRpgSession = false;

	/** 공유 월드로 이동하는 동안 활성화를 기다리는 RPG 게임 세션 */
	FString PendingGameSessionId;
	int32 PendingMaxPlayerSessionCount = 0;
	double PendingSinceSeconds = -1.0;

	/** PreLogin에서 검증한 player session → 백엔드가 CreatePlayerSession에 넣은 플레이어 ID */
	TMap<FString, FString> AcceptedPlayerIds;
	TSet<FString> ConnectedPlayerSessionIds;
	bool bAnyPlayerJoined = false;
	double SessionActivatedSeconds = -1.0;
	double EmptySinceSeconds = -1.0;

	bool bSessionEndRequested = false;
	FString SessionEndReason;
	double SessionEndDeadlineSeconds = 0.0;

	FAwsCredentials AnywhereCredentials;
	FAwsCredentials FleetRoleCredentials;
	FDateTime FleetRoleCredentialsExpiration;
};
