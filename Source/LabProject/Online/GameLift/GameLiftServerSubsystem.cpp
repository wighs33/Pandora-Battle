#include "Online/GameLift/GameLiftServerSubsystem.h"

#include "Async/Async.h"
#include "Common/GameSessionConstants.h"
#include "Definition/Level/LevelDefinition.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "IPAddress.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformOutputDevices.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Mode/PdPlayerState.h"
#include "Online/Backend/BackendSettings.h"
#include "Online/Backend/MatchReportSubsystem.h"

#if PD_WITH_GAMELIFT
#include "GameLiftServerSDK.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameLiftServerSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogGameLiftServer, Log, All);

namespace
{
	TAutoConsoleVariable<float> CVarFirstPlayerTimeout(
		TEXT("pd.GameLift.FirstPlayerTimeout"),
		120.0f,
		TEXT("Seconds an activated game session waits for its first player before the process ends."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarEmptySessionTimeout(
		TEXT("pd.GameLift.EmptySessionTimeout"),
		15.0f,
		TEXT("Seconds a game session may stay empty after players left before the process ends."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarSessionEndTimeout(
		TEXT("pd.GameLift.SessionEndTimeout"),
		20.0f,
		TEXT("After the match ends, maximum seconds to wait for result reports and player exits."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarRpgEmptySessionTimeout(
		TEXT("pd.GameLift.RpgEmptySessionTimeout"),
		300.0f,
		TEXT("Seconds an RPG shared-world session may stay empty before the process ends. Players may rejoin meanwhile."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarRpgWorldReadyTimeout(
		TEXT("pd.GameLift.RpgWorldReadyTimeout"),
		60.0f,
		TEXT("Seconds an RPG game session may wait for the shared-world map to load before the process ends."),
		ECVF_Default);

	constexpr float WatchdogIntervalSeconds = 1.0f;

	bool IsGameLiftLaunch()
	{
		const TCHAR* CommandLine = FCommandLine::Get();
		return !FParse::Param(CommandLine, TEXT("NoGameLift"))
			&& (FParse::Param(CommandLine, TEXT("glAnywhere")) || FParse::Param(CommandLine, TEXT("GameLift")));
	}

	// 엔진은 연결의 RequestURL을 "맵?옵션..." 형태로 다시 만들어 저장한다(NMT_Login). ParseOption은 '?'로 시작하는
	// 문자열만 읽으므로 첫 '?'부터 넘긴다.
	FString ParseConnectionOption(const UNetConnection& Connection, const TCHAR* Key)
	{
		const int32 OptionsStart = Connection.RequestURL.Find(TEXT("?"));
		return OptionsStart == INDEX_NONE
			? FString()
			: UGameplayStatics::ParseOption(Connection.RequestURL.Mid(OptionsStart), Key);
	}

	// 로컬 시험용 PlayerId는 클라이언트가 보낸 값이므로 백엔드 ID 문자 집합만 남긴다.
	FString SanitizeLocalPlayerId(const FString& PlayerId)
	{
		FString Sanitized;
		for (const TCHAR Character : PlayerId.Left(64))
		{
			if (FChar::IsAlnum(Character) || Character == TEXT(':') || Character == TEXT('_') || Character == TEXT('-'))
			{
				Sanitized.AppendChar(Character);
			}
		}
		return Sanitized;
	}

#if PD_WITH_GAMELIFT
	// SDK 모델은 빌드 설정(GAMELIFT_USE_STD)에 따라 std::string 또는 const char*를 돌려준다.
	FString ToFString(const char* Text)
	{
		return Text ? FString(UTF8_TO_TCHAR(Text)) : FString();
	}

	FString ToFString(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}

	FString DescribeError(const FGameLiftError& Error)
	{
		return Error.m_errorMessage.IsEmpty() ? Error.m_errorName : Error.m_errorMessage;
	}

	// 백엔드가 CreateGameSession의 GameProperties에 넣은 값. 없으면 빈 문자열이다.
	FString FindGameProperty(const Aws::GameLift::Server::Model::GameSession& GameSession, const FString& Key)
	{
#ifdef GAMELIFT_USE_STD
		for (const Aws::GameLift::Server::Model::GameProperty& Property : GameSession.GetGameProperties())
		{
			if (ToFString(Property.GetKey()) == Key)
			{
				return ToFString(Property.GetValue());
			}
		}
#else
		int Count = 0;
		const Aws::GameLift::Server::Model::GameProperty* Properties = GameSession.GetGameProperties(Count);
		for (int Index = 0; Properties && Index < Count; ++Index)
		{
			if (ToFString(Properties[Index].GetKey()) == Key)
			{
				return ToFString(Properties[Index].GetValue());
			}
		}
#endif
		return FString();
	}
#endif
}

bool UGameLiftServerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UGameLiftServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 로그인·퇴장은 로비와 경기 GameMode 모두에서 같은 방식으로 처리하므로 엔진 전역 이벤트로 받는다.
	PostLoginHandle = FGameModeEvents::OnGameModePostLoginEvent().AddUObject(this, &ThisClass::HandlePostLogin);
	LogoutHandle = FGameModeEvents::OnGameModeLogoutEvent().AddUObject(this, &ThisClass::HandleLogout);

	if (!IsGameLiftLaunch())
	{
		return;
	}

#if PD_WITH_GAMELIFT
	bSdkInitialized = InitializeSdk();
	if (!bSdkInitialized)
	{
		// GameLift가 띄운 프로세스가 SDK에 연결하지 못하면 게임 세션을 받을 수 없다. 종료해 새 프로세스로 교체되게 한다.
		EndProcess(TEXT("GameLift InitSDK failed"));
		return;
	}
	WatchdogHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::TickSessionWatchdog), WatchdogIntervalSeconds);
#else
	UE_LOG(LogGameLiftServer, Error,
		TEXT("GameLift launch arguments were given, but this server was built without the GameLift server SDK."));
#endif
}

void UGameLiftServerSubsystem::Deinitialize()
{
	FGameModeEvents::OnGameModePostLoginEvent().Remove(PostLoginHandle);
	FGameModeEvents::OnGameModeLogoutEvent().Remove(LogoutHandle);
	if (WatchdogHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(WatchdogHandle);
		WatchdogHandle.Reset();
	}
	ShutdownSdk();
	Super::Deinitialize();
}

UGameLiftServerSubsystem* UGameLiftServerSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGameLiftServerSubsystem>() : nullptr;
}

bool UGameLiftServerSubsystem::InitializeSdk()
{
#if PD_WITH_GAMELIFT
	SdkModule = &FModuleManager::LoadModuleChecked<FGameLiftServerSDKModule>(FName(TEXT("GameLiftServerSDK")));

	// Anywhere는 실행 인자로 연결 정보를 받고, 관리형 플릿은 GameLift 에이전트가 넣어 준 환경에서 읽는다.
	const bool bAnywhere = FParse::Param(FCommandLine::Get(), TEXT("glAnywhere"));
	FServerParameters ServerParameters;
	if (bAnywhere)
	{
		const TCHAR* CommandLine = FCommandLine::Get();
		FParse::Value(CommandLine, TEXT("glAnywhereWebSocketUrl="), ServerParameters.m_webSocketUrl);
		FParse::Value(CommandLine, TEXT("glAnywhereFleetId="), ServerParameters.m_fleetId);
		FParse::Value(CommandLine, TEXT("glAnywhereHostId="), ServerParameters.m_hostId);
		FParse::Value(CommandLine, TEXT("glAnywhereAuthToken="), ServerParameters.m_authToken);
		FParse::Value(CommandLine, TEXT("glAnywhereAwsRegion="), ServerParameters.m_awsRegion);
		FParse::Value(CommandLine, TEXT("glAnywhereAccessKey="), ServerParameters.m_accessKey);
		FParse::Value(CommandLine, TEXT("glAnywhereSecretKey="), ServerParameters.m_secretKey);
		FParse::Value(CommandLine, TEXT("glAnywhereSessionToken="), ServerParameters.m_sessionToken);
		if (!FParse::Value(CommandLine, TEXT("glAnywhereProcessId="), ServerParameters.m_processId))
		{
			ServerParameters.m_processId = FString::Printf(TEXT("pd-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		}

		AnywhereCredentials.AccessKeyId = ServerParameters.m_accessKey;
		AnywhereCredentials.SecretAccessKey = ServerParameters.m_secretKey;
		AnywhereCredentials.SessionToken = ServerParameters.m_sessionToken;
	}

	const FGameLiftGenericOutcome Outcome = bAnywhere ? SdkModule->InitSDK(ServerParameters) : SdkModule->InitSDK();
	if (!Outcome.IsSuccess())
	{
		UE_LOG(LogGameLiftServer, Error, TEXT("GameLift InitSDK failed: %s"), *DescribeError(Outcome.GetError()));
		return false;
	}

	UE_LOG(LogGameLiftServer, Log, TEXT("GameLift InitSDK succeeded (%s). Fleet=%s Compute=%s Process=%s"),
		bAnywhere ? TEXT("Anywhere") : TEXT("managed"),
		*ServerParameters.m_fleetId, *ServerParameters.m_hostId, *ServerParameters.m_processId);
	return true;
#else
	return false;
#endif
}

// SDK 콜백은 GameLift 통신 스레드에서 오므로, 게임 상태는 게임 스레드로 넘겨서 바꾼다.
void UGameLiftServerSubsystem::NotifyServerReadyForSessions(const UWorld* World)
{
#if PD_WITH_GAMELIFT
	if (!bSdkInitialized || bProcessReadySent || !World)
	{
		return;
	}

	const TWeakObjectPtr<ThisClass> WeakThis(this);
	ProcessParameters = MakeShared<FProcessParameters>();
	ProcessParameters->OnStartGameSession.BindLambda(
		[WeakThis](Aws::GameLift::Server::Model::GameSession GameSession)
		{
			const FString SessionId = ToFString(GameSession.GetGameSessionId());
			const int32 MaxPlayers = GameSession.GetMaximumPlayerSessionCount();
			const FString SessionMode = FindGameProperty(GameSession, LabGameSession::SessionModeProperty);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, SessionId, MaxPlayers, SessionMode]()
			{
				if (UGameLiftServerSubsystem* This = WeakThis.Get())
				{
					This->HandleGameSessionStarted(SessionId, MaxPlayers, SessionMode);
				}
			});
		});
	ProcessParameters->OnTerminate.BindLambda([WeakThis]()
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis]()
		{
			if (UGameLiftServerSubsystem* This = WeakThis.Get())
			{
				This->EndProcess(TEXT("GameLift requested process termination"));
			}
		});
	});
	ProcessParameters->OnHealthCheck.BindLambda([]() { return true; });

	// 요청한 포트가 사용 중이면 IP 넷 드라이버가 다음 포트에 바인딩하므로, 실제로 열린 포트를 알린다.
	UNetDriver* NetDriver = World->GetNetDriver();
	const TSharedPtr<const FInternetAddr> LocalAddress = NetDriver ? NetDriver->GetLocalAddr() : nullptr;
	ProcessParameters->port = LocalAddress.IsValid() ? LocalAddress->GetPort() : World->URL.Port;
	ProcessParameters->logParameters.Add(
		FPaths::ConvertRelativePathToFull(FPlatformOutputDevices::GetAbsoluteLogFilename()));

	const FGameLiftGenericOutcome Outcome = SdkModule->ProcessReady(*ProcessParameters);
	if (!Outcome.IsSuccess())
	{
		EndProcess(FString::Printf(TEXT("GameLift ProcessReady failed: %s"), *DescribeError(Outcome.GetError())));
		return;
	}

	bProcessReadySent = true;
	UE_LOG(LogGameLiftServer, Log, TEXT("GameLift ProcessReady sent. Port=%d"), ProcessParameters->port);
#endif
}

// 경기 세션은 이미 열린 로비가 바로 받는다. RPG 세션은 공유 월드로 이동하고, 그 맵이 준비되면 활성화한다.
void UGameLiftServerSubsystem::HandleGameSessionStarted(
	const FString& InGameSessionId,
	const int32 MaxPlayerSessionCount,
	const FString& SessionMode)
{
#if PD_WITH_GAMELIFT
	if (bEnding)
	{
		return;
	}

	bRpgSession = SessionMode.Equals(LabGameSession::RpgSessionMode, ESearchCase::IgnoreCase);
	if (!bRpgSession)
	{
		ActivateSession(InGameSessionId, MaxPlayerSessionCount);
		return;
	}

	PendingGameSessionId = InGameSessionId;
	PendingMaxPlayerSessionCount = MaxPlayerSessionCount;
	PendingSinceSeconds = FPlatformTime::Seconds();
	if (!TravelToRpgWorld())
	{
		EndProcess(TEXT("RPG world could not be opened"));
	}
#endif
}

void UGameLiftServerSubsystem::ActivatePendingGameSession()
{
	if (PendingGameSessionId.IsEmpty() || bEnding)
	{
		return;
	}

	const FString SessionId = PendingGameSessionId;
	PendingGameSessionId.Reset();
	ActivateSession(SessionId, PendingMaxPlayerSessionCount);
}

bool UGameLiftServerSubsystem::TravelToRpgWorld()
{
	UWorld* World = GetGameInstance()->GetWorld();
	const ULevelDefinition* Levels = ULevelDefinition::ResolveDefaultDefinition();
	const FString MapName = Levels ? Levels->GetRpgTravelMapName() : FString();
	if (!World || MapName.IsEmpty())
	{
		UE_LOG(LogGameLiftServer, Error, TEXT("RPG game session %s has no RpgLevel to open."), *PendingGameSessionId);
		return false;
	}

	UE_LOG(LogGameLiftServer, Log, TEXT("RPG game session %s: opening %s before activation."), *PendingGameSessionId, *MapName);
	return World->ServerTravel(FString::Printf(TEXT("%s?%s=1"), *MapName, LabGameSession::RpgModeOption));
}

void UGameLiftServerSubsystem::ActivateSession(const FString& InGameSessionId, const int32 MaxPlayerSessionCount)
{
#if PD_WITH_GAMELIFT
	GameSessionId = InGameSessionId;
	SessionActivatedSeconds = FPlatformTime::Seconds();
	EmptySinceSeconds = -1.0;
	bAnyPlayerJoined = false;

	const FGameLiftGenericOutcome Outcome = SdkModule->ActivateGameSession();
	if (!Outcome.IsSuccess())
	{
		EndProcess(FString::Printf(TEXT("ActivateGameSession failed: %s"), *DescribeError(Outcome.GetError())));
		return;
	}
	UE_LOG(LogGameLiftServer, Log, TEXT("GameLift game session activated: %s (%s, max %d players)"),
		*GameSessionId, bRpgSession ? LabGameSession::RpgSessionMode : LabGameSession::MatchSessionMode,
		MaxPlayerSessionCount);
#endif
}

void UGameLiftServerSubsystem::ValidatePlayerJoin(const FString& Options, FString& InOutErrorMessage)
{
#if PD_WITH_GAMELIFT
	if (!bSdkInitialized || !InOutErrorMessage.IsEmpty())
	{
		return;
	}
	if (GameSessionId.IsEmpty() || bEnding || bSessionEndRequested)
	{
		InOutErrorMessage = TEXT("Game session is not accepting players.");
		return;
	}

	const FString PlayerSessionId = UGameplayStatics::ParseOption(Options, TEXT("PlayerSessionId"));
	if (PlayerSessionId.IsEmpty())
	{
		InOutErrorMessage = TEXT("Missing PlayerSessionId.");
		return;
	}

	// RESERVED 상태의 player session만 ACTIVE로 바뀐다. 같은 ID의 재사용이나 다른 게임 세션의 ID는 거절된다.
	const FGameLiftGenericOutcome AcceptOutcome = SdkModule->AcceptPlayerSession(PlayerSessionId);
	if (!AcceptOutcome.IsSuccess())
	{
		UE_LOG(LogGameLiftServer, Warning, TEXT("Rejected player session %s: %s"),
			*PlayerSessionId, *DescribeError(AcceptOutcome.GetError()));
		InOutErrorMessage = TEXT("Player session was rejected.");
		return;
	}

	FGameLiftDescribePlayerSessionsRequest DescribeRequest;
	DescribeRequest.m_playerSessionId = PlayerSessionId;
	const FGameLiftDescribePlayerSessionsOutcome DescribeOutcome = SdkModule->DescribePlayerSessions(DescribeRequest);
	FString BackendPlayerId;
	if (DescribeOutcome.IsSuccess() && DescribeOutcome.GetResult().m_playerSessions.Num() > 0)
	{
		BackendPlayerId = DescribeOutcome.GetResult().m_playerSessions[0].m_playerId;
	}
	else
	{
		UE_LOG(LogGameLiftServer, Warning, TEXT("Could not read the player ID of %s. Its result will not be recorded."),
			*PlayerSessionId);
	}
	AcceptedPlayerIds.Add(PlayerSessionId, BackendPlayerId);
	UE_LOG(LogGameLiftServer, Log, TEXT("Accepted player session %s (%s)"), *PlayerSessionId, *BackendPlayerId);
#endif
}

// 접속 URL은 연결에 남아 있으므로, 경기 맵으로 심리스 이동해도 PlayerState 복사로 같은 식별 정보를 유지한다.
void UGameLiftServerSubsystem::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (!GameMode || GameMode->GetGameInstance() != GetGameInstance() || !NewPlayer)
	{
		return;
	}

	const UNetConnection* Connection = NewPlayer->GetNetConnection();
	APdPlayerState* PlayerState = NewPlayer->GetPlayerState<APdPlayerState>();
	if (!Connection || !PlayerState)
	{
		return;
	}

	if (!bSdkInitialized)
	{
		// GameLift 없이 로컬 전용 서버에서 전적 기록을 시험할 때만 클라이언트가 보낸 PlayerId를 쓴다.
		const FString LocalPlayerId = SanitizeLocalPlayerId(ParseConnectionOption(*Connection, TEXT("PlayerId")));
		PlayerState->SetBackendIdentity(FString(), LocalPlayerId);
		if (!LocalPlayerId.IsEmpty())
		{
			UE_LOG(LogGameLiftServer, Log, TEXT("Local backend player ID for %s: %s"), *PlayerState->GetPlayerName(), *LocalPlayerId);
		}
		return;
	}

	const FString PlayerSessionId = ParseConnectionOption(*Connection, TEXT("PlayerSessionId"));
	if (PlayerSessionId.IsEmpty())
	{
		UE_LOG(LogGameLiftServer, Warning, TEXT("%s joined without a PlayerSessionId."), *PlayerState->GetPlayerName());
		return;
	}
	PlayerState->SetBackendIdentity(PlayerSessionId, AcceptedPlayerIds.FindRef(PlayerSessionId));
	ConnectedPlayerSessionIds.Add(PlayerSessionId);
	bAnyPlayerJoined = true;
	EmptySinceSeconds = -1.0;
}

void UGameLiftServerSubsystem::HandleLogout(AGameModeBase* GameMode, AController* Exiting)
{
#if PD_WITH_GAMELIFT
	const APdPlayerState* PlayerState = Exiting ? Exiting->GetPlayerState<APdPlayerState>() : nullptr;
	if (!bSdkInitialized || !PlayerState || !GameMode || GameMode->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	const FString& PlayerSessionId = PlayerState->GetBackendPlayerSessionId();
	if (PlayerSessionId.IsEmpty() || ConnectedPlayerSessionIds.Remove(PlayerSessionId) == 0)
	{
		return;
	}

	const FGameLiftGenericOutcome Outcome = SdkModule->RemovePlayerSession(PlayerSessionId);
	if (!Outcome.IsSuccess())
	{
		UE_LOG(LogGameLiftServer, Warning, TEXT("RemovePlayerSession %s failed: %s"),
			*PlayerSessionId, *DescribeError(Outcome.GetError()));
	}
	if (ConnectedPlayerSessionIds.IsEmpty())
	{
		EmptySinceSeconds = FPlatformTime::Seconds();
	}
#endif
}

void UGameLiftServerSubsystem::SetPlayerSessionCreationPolicy(const bool bAcceptNewPlayers)
{
#if PD_WITH_GAMELIFT
	if (!bSdkInitialized || GameSessionId.IsEmpty() || (bAcceptNewPlayers && bSessionEndRequested))
	{
		return;
	}

	const FGameLiftGenericOutcome Outcome = SdkModule->UpdatePlayerSessionCreationPolicy(
		bAcceptNewPlayers ? EPlayerSessionCreationPolicy::ACCEPT_ALL : EPlayerSessionCreationPolicy::DENY_ALL);
	if (!Outcome.IsSuccess())
	{
		UE_LOG(LogGameLiftServer, Warning, TEXT("UpdatePlayerSessionCreationPolicy failed: %s"),
			*DescribeError(Outcome.GetError()));
	}
#endif
}

void UGameLiftServerSubsystem::RequestSessionEnd(const FString& Reason)
{
	if (!bSdkInitialized || bSessionEndRequested)
	{
		return;
	}

	SetPlayerSessionCreationPolicy(false);
	bSessionEndRequested = true;
	SessionEndReason = Reason;
	SessionEndDeadlineSeconds = FPlatformTime::Seconds() + FMath::Max(1.0f, CVarSessionEndTimeout.GetValueOnGameThread());
	UE_LOG(LogGameLiftServer, Log, TEXT("GameLift session end requested: %s"), *Reason);
}

// 게임 세션이 끝나야 할 조건을 1초마다 확인한다. 결과 보고가 진행 중이면 전송을 마칠 때까지 기다린다.
bool UGameLiftServerSubsystem::TickSessionWatchdog(float DeltaSeconds)
{
	if (!bSdkInitialized || bEnding)
	{
		return true;
	}

	const double NowSeconds = FPlatformTime::Seconds();
	// RPG 맵이 열리지 않으면 세션이 활성화되지 않은 채 남는다. 프로세스를 끝내 새 프로세스로 바꾼다.
	if (!PendingGameSessionId.IsEmpty()
		&& NowSeconds - PendingSinceSeconds >= CVarRpgWorldReadyTimeout.GetValueOnGameThread())
	{
		EndProcess(TEXT("RPG world was not ready in time"));
		return true;
	}
	if (GameSessionId.IsEmpty())
	{
		return true;
	}

	const UMatchReportSubsystem* Reports = GetGameInstance()->GetSubsystem<UMatchReportSubsystem>();
	const bool bReportsPending = Reports && Reports->HasPendingReports();
	if (bSessionEndRequested)
	{
		if ((ConnectedPlayerSessionIds.IsEmpty() && !bReportsPending) || NowSeconds >= SessionEndDeadlineSeconds)
		{
			EndProcess(SessionEndReason);
		}
		return true;
	}

	if (!ConnectedPlayerSessionIds.IsEmpty() || bReportsPending)
	{
		return true;
	}

	// RPG 공유 월드는 잠시 비어도 다시 들어올 수 있게 더 오래 기다린다.
	const double EmptySince = bAnyPlayerJoined ? EmptySinceSeconds : SessionActivatedSeconds;
	const TAutoConsoleVariable<float>& EmptyTimeout = bRpgSession ? CVarRpgEmptySessionTimeout : CVarEmptySessionTimeout;
	const float Timeout = bAnyPlayerJoined
		? EmptyTimeout.GetValueOnGameThread()
		: CVarFirstPlayerTimeout.GetValueOnGameThread();
	if (EmptySince >= 0.0 && NowSeconds - EmptySince >= Timeout)
	{
		EndProcess(bAnyPlayerJoined
			? TEXT("All players left the game session")
			: TEXT("No player joined the game session"));
	}
	return true;
}

bool UGameLiftServerSubsystem::TryGetServiceCredentials(FAwsCredentials& OutCredentials)
{
	if (AnywhereCredentials.IsValid())
	{
		OutCredentials = AnywhereCredentials;
		return true;
	}

#if PD_WITH_GAMELIFT
	if (bSdkInitialized && !FParse::Param(FCommandLine::Get(), TEXT("glAnywhere")))
	{
		if (FleetRoleCredentials.IsValid() && FDateTime::UtcNow() + FTimespan::FromMinutes(5) < FleetRoleCredentialsExpiration)
		{
			OutCredentials = FleetRoleCredentials;
			return true;
		}

		const FString RoleArn = GetDefault<UBackendSettings>()->GetGameServerRoleArn();
		if (!RoleArn.IsEmpty())
		{
			FGameLiftGetFleetRoleCredentialsRequest Request;
			Request.m_roleArn = RoleArn;
			Request.m_roleSessionName = TEXT("labproject-game-server");
			const FGameLiftGetFleetRoleCredentialsOutcome Outcome = SdkModule->GetFleetRoleCredentials(Request);
			if (Outcome.IsSuccess())
			{
				const FGameLiftGetFleetRoleCredentialsResult& Result = Outcome.GetResult();
				FleetRoleCredentials.AccessKeyId = Result.m_accessKeyId;
				FleetRoleCredentials.SecretAccessKey = Result.m_secretAccessKey;
				FleetRoleCredentials.SessionToken = Result.m_sessionToken;
				FleetRoleCredentialsExpiration = Result.m_expiration;
				OutCredentials = FleetRoleCredentials;
				return true;
			}
			UE_LOG(LogGameLiftServer, Warning, TEXT("GetFleetRoleCredentials failed: %s"),
				*DescribeError(Outcome.GetError()));
		}
	}
#endif

	// 로컬 시험: aws configure export-credentials로 환경 변수에 넣은 개발자 자격 증명
	OutCredentials.AccessKeyId = FPlatformMisc::GetEnvironmentVariable(TEXT("AWS_ACCESS_KEY_ID"));
	OutCredentials.SecretAccessKey = FPlatformMisc::GetEnvironmentVariable(TEXT("AWS_SECRET_ACCESS_KEY"));
	OutCredentials.SessionToken = FPlatformMisc::GetEnvironmentVariable(TEXT("AWS_SESSION_TOKEN"));
	return OutCredentials.IsValid();
}

void UGameLiftServerSubsystem::EndProcess(const FString& Reason)
{
	if (bEnding)
	{
		return;
	}

	bEnding = true;
	UE_LOG(LogGameLiftServer, Log, TEXT("Ending GameLift server process: %s"), *Reason);
	ShutdownSdk();
	FPlatformMisc::RequestExit(false, TEXT("UGameLiftServerSubsystem::EndProcess"));
}

// ProcessEnding으로 GameLift에 종료를 알리고 SDK 연결을 정리한다.
void UGameLiftServerSubsystem::ShutdownSdk()
{
#if PD_WITH_GAMELIFT
	if (!bSdkInitialized || !SdkModule)
	{
		return;
	}

	bSdkInitialized = false;
	const FGameLiftGenericOutcome EndingOutcome = SdkModule->ProcessEnding();
	if (!EndingOutcome.IsSuccess())
	{
		UE_LOG(LogGameLiftServer, Error, TEXT("ProcessEnding failed: %s"), *DescribeError(EndingOutcome.GetError()));
	}
	const FGameLiftGenericOutcome DestroyOutcome = SdkModule->Destroy();
	if (!DestroyOutcome.IsSuccess())
	{
		UE_LOG(LogGameLiftServer, Error, TEXT("GameLift SDK Destroy failed: %s"), *DescribeError(DestroyOutcome.GetError()));
	}
#endif
}
