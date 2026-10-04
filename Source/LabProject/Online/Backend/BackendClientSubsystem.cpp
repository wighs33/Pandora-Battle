#include "Online/Backend/BackendClientSubsystem.h"

#include "Common/GameSessionConstants.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Online/Backend/BackendHttp.h"
#include "Online/Backend/BackendSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BackendClientSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogBackendClient, Log, All);

namespace
{
	FString GetStringField(const TSharedPtr<FJsonObject>& Json, const TCHAR* FieldName)
	{
		FString Value;
		if (Json.IsValid())
		{
			Json->TryGetStringField(FieldName, Value);
		}
		return Value;
	}

	int32 GetIntField(const TSharedPtr<FJsonObject>& Json, const TCHAR* FieldName)
	{
		int32 Value = 0;
		if (Json.IsValid())
		{
			Json->TryGetNumberField(FieldName, Value);
		}
		return Value;
	}

	UBackendClientSubsystem* FindBackendClient(const UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		UBackendClientSubsystem* Backend = GameInstance ? GameInstance->GetSubsystem<UBackendClientSubsystem>() : nullptr;
		if (!Backend)
		{
			UE_LOG(LogBackendClient, Warning, TEXT("Backend client is not available in this world."));
		}
		return Backend;
	}

	// 콘솔 명령은 명령을 입력한 월드의 GameInstance로 전달해 PIE의 여러 클라이언트를 구분한다.
	FAutoConsoleCommandWithWorldAndArgs LoginCommand(
		TEXT("pd.Backend.Login"),
		TEXT("Log in to the backend with Steam (or -BackendDevLogin=<id>)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UBackendClientSubsystem* Backend = FindBackendClient(World)) { Backend->Login(); }
		}));

	FAutoConsoleCommandWithWorldAndArgs DevLoginCommand(
		TEXT("pd.Backend.DevLogin"),
		TEXT("pd.Backend.DevLogin <id>: developer login (backend stage must allow it)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UBackendClientSubsystem* Backend = FindBackendClient(World);
			if (Backend && Args.Num() > 0) { Backend->LoginAsDeveloper(Args[0]); }
		}));

	FAutoConsoleCommandWithWorldAndArgs JoinMatchCommand(
		TEXT("pd.Backend.JoinMatch"),
		TEXT("pd.Backend.JoinMatch [bossraid]: ask the backend for a GameLift game session (PvP match, or the boss raid) and travel to it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const bool bBossRaid = Args.Num() > 0 && Args[0].Equals(LabGameSession::BossRaidSessionMode, ESearchCase::IgnoreCase);
			if (UBackendClientSubsystem* Backend = FindBackendClient(World))
			{
				Backend->JoinOnlineMatch(bBossRaid ? EOnlineMatchMode::BossRaid : EOnlineMatchMode::Match);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs ProfileCommand(
		TEXT("pd.Backend.Profile"),
		TEXT("Print the logged-in player's stats and recent matches."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UBackendClientSubsystem* Backend = FindBackendClient(World)) { Backend->RequestMyProfile(); }
		}));

	FAutoConsoleCommandWithWorldAndArgs ConnectLocalCommand(
		TEXT("pd.Backend.ConnectLocal"),
		TEXT("pd.Backend.ConnectLocal <ip:port>: join a local dedicated server with the backend PlayerId (no GameLift)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UBackendClientSubsystem* Backend = FindBackendClient(World);
			if (Backend && Args.Num() > 0) { Backend->ConnectToLocalServer(Args[0]); }
		}));
}

bool UBackendClientSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

// Steam 클라이언트가 GetAuthTicketForWebApi로 발급한 티켓을 백엔드가 Steam Web API로 검증한다.
void UBackendClientSubsystem::Login()
{
	if (LoginState == EBackendLoginState::LoggingIn)
	{
		return;
	}

	FString DevId;
	if (FParse::Value(FCommandLine::Get(), TEXT("BackendDevLogin="), DevId) && !DevId.IsEmpty())
	{
		LoginAsDeveloper(DevId);
		return;
	}

	LoginState = EBackendLoginState::LoggingIn;
	const IOnlineSubsystem* SteamSubsystem = Online::GetSubsystem(GetWorld(), STEAM_SUBSYSTEM);
	const IOnlineIdentityPtr Identity = SteamSubsystem ? SteamSubsystem->GetIdentityInterface() : nullptr;
	if (!Identity.IsValid())
	{
		FinishLogin(false, TEXT("Steam is not available. Use -BackendDevLogin=<id> for local testing."));
		return;
	}

	const FString TokenType = FString::Printf(TEXT("WebAPI:%s"),
		*GetDefault<UBackendSettings>()->GetSteamWebApiIdentity());
	Identity->GetLinkedAccountAuthToken(0, TokenType,
		IOnlineIdentity::FOnGetLinkedAccountAuthTokenCompleteDelegate::CreateWeakLambda(this,
			[this](int32, const bool bWasSuccessful, const FExternalAuthToken& AuthToken)
			{
				if (!bWasSuccessful || !AuthToken.HasTokenString())
				{
					FinishLogin(false, TEXT("Could not get a Steam Web API ticket."));
					return;
				}

				const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
				Body->SetStringField(TEXT("ticket"), AuthToken.TokenString);
				Body->SetStringField(TEXT("displayName"), ResolveSteamDisplayName());
				SendLoginRequest(TEXT("/auth/steam"), Body);
			}));
}

void UBackendClientSubsystem::LoginAsDeveloper(const FString& DevId)
{
	if (LoginState == EBackendLoginState::LoggingIn)
	{
		return;
	}

	LoginState = EBackendLoginState::LoggingIn;
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("devId"), DevId);
	Body->SetStringField(TEXT("displayName"), DevId);
	SendLoginRequest(TEXT("/auth/dev"), Body);
}

void UBackendClientSubsystem::SendLoginRequest(const TCHAR* Path, const TSharedRef<FJsonObject>& Body)
{
	const UBackendSettings* Settings = GetDefault<UBackendSettings>();
	const FString Url = Settings->BuildApiUrl(Path);
	if (Url.IsEmpty())
	{
		FinishLogin(false, TEXT("Backend ApiBaseUrl is not configured."));
		return;
	}

	PdBackendHttp::Send(
		PdBackendHttp::CreateJsonRequest(TEXT("POST"), Url, Body, Settings->GetClientRequestTimeoutSeconds()),
		PdBackendHttp::FOnResponse::CreateUObject(this, &ThisClass::HandleLoginResponse));
}

void UBackendClientSubsystem::HandleLoginResponse(const PdBackendHttp::FResponse& Response)
{
	const FString Token = GetStringField(Response.Json, TEXT("token"));
	if (!Response.bSucceeded || Token.IsEmpty())
	{
		FinishLogin(false, FString::Printf(TEXT("%s: %s"), *Response.ErrorCode, *Response.ErrorMessage));
		return;
	}

	SessionToken = Token;
	PlayerId = GetStringField(Response.Json, TEXT("playerId"));
	DisplayName = GetStringField(Response.Json, TEXT("displayName"));
	LoginState = EBackendLoginState::LoggedIn;
	FinishLogin(true, FString());
}

void UBackendClientSubsystem::FinishLogin(const bool bSucceeded, const FString& ErrorMessage)
{
	if (bSucceeded)
	{
		UE_LOG(LogBackendClient, Log, TEXT("Backend login succeeded. PlayerId=%s Name=%s"), *PlayerId, *DisplayName);
	}
	else
	{
		LoginState = EBackendLoginState::LoggedOut;
		SessionToken.Reset();
		UE_LOG(LogBackendClient, Warning, TEXT("Backend login failed. %s"), *ErrorMessage);
	}
	OnLoginFinished.Broadcast(bSucceeded, ErrorMessage);

	if (bJoinMatchAfterLogin)
	{
		bJoinMatchAfterLogin = false;
		if (bSucceeded)
		{
			SendMatchJoinRequest();
		}
		else
		{
			FinishMatchJoin(false, ErrorMessage);
		}
	}
}

void UBackendClientSubsystem::JoinOnlineMatch(const EOnlineMatchMode Mode)
{
	if (bMatchJoinInProgress)
	{
		return;
	}

	bMatchJoinInProgress = true;
	PendingMatchMode = Mode;
	if (IsLoggedIn())
	{
		SendMatchJoinRequest();
		return;
	}

	bJoinMatchAfterLogin = true;
	if (LoginState != EBackendLoginState::LoggingIn)
	{
		Login();
	}
}

// 백엔드는 빈 자리가 있는 게임 세션을 찾거나 새로 만들고, 이 플레이어의 player session을 예약해 돌려준다.
void UBackendClientSubsystem::SendMatchJoinRequest()
{
	const UBackendSettings* Settings = GetDefault<UBackendSettings>();
	const FString Url = Settings->BuildApiUrl(TEXT("/match/join"));
	if (Url.IsEmpty())
	{
		FinishMatchJoin(false, TEXT("Backend ApiBaseUrl is not configured."));
		return;
	}

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("mode"), PendingMatchMode == EOnlineMatchMode::BossRaid
		? LabGameSession::BossRaidSessionMode
		: LabGameSession::MatchSessionMode);
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = PdBackendHttp::CreateJsonRequest(
		TEXT("POST"), Url, Body, Settings->GetClientRequestTimeoutSeconds());
	Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + SessionToken);
	PdBackendHttp::Send(Request, PdBackendHttp::FOnResponse::CreateUObject(this, &ThisClass::HandleMatchJoinResponse));
}

void UBackendClientSubsystem::HandleMatchJoinResponse(const PdBackendHttp::FResponse& Response)
{
	if (Response.StatusCode == 401)
	{
		HandleUnauthorized();
	}
	if (!Response.bSucceeded)
	{
		FinishMatchJoin(false, FString::Printf(TEXT("%s: %s"), *Response.ErrorCode, *Response.ErrorMessage));
		return;
	}

	FString Address = GetStringField(Response.Json, TEXT("ipAddress"));
	if (Address.IsEmpty())
	{
		Address = GetStringField(Response.Json, TEXT("dnsName"));
	}
	const int32 Port = GetIntField(Response.Json, TEXT("port"));
	const FString PlayerSessionId = GetStringField(Response.Json, TEXT("playerSessionId"));
	APlayerController* PlayerController = GetLocalPlayerController();
	if (Address.IsEmpty() || Port <= 0 || PlayerSessionId.IsEmpty() || !PlayerController)
	{
		FinishMatchJoin(false, TEXT("Backend returned incomplete connection info."));
		return;
	}

	const FString TravelUrl = FString::Printf(TEXT("%s:%d?PlayerSessionId=%s"), *Address, Port, *PlayerSessionId);
	UE_LOG(LogBackendClient, Log, TEXT("Joining GameLift %s session at %s:%d"),
		PendingMatchMode == EOnlineMatchMode::BossRaid ? LabGameSession::BossRaidSessionMode : LabGameSession::MatchSessionMode,
		*Address, Port);
	PlayerController->ClientTravel(TravelUrl, TRAVEL_Absolute);
	FinishMatchJoin(true, FString());
}

void UBackendClientSubsystem::FinishMatchJoin(const bool bSucceeded, const FString& ErrorMessage)
{
	bMatchJoinInProgress = false;
	if (!bSucceeded)
	{
		UE_LOG(LogBackendClient, Warning, TEXT("Online match join failed. %s"), *ErrorMessage);
	}
	OnMatchJoinFinished.Broadcast(bSucceeded, ErrorMessage);
}

void UBackendClientSubsystem::RequestMyProfile()
{
	const UBackendSettings* Settings = GetDefault<UBackendSettings>();
	const FString Url = Settings->BuildApiUrl(TEXT("/player/me"));
	if (!IsLoggedIn() || Url.IsEmpty())
	{
		UE_LOG(LogBackendClient, Warning, TEXT("Profile request needs a backend login and ApiBaseUrl."));
		OnProfileReceived.Broadcast(false, FBackendPlayerProfile());
		return;
	}

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		PdBackendHttp::CreateJsonRequest(TEXT("GET"), Url, nullptr, Settings->GetClientRequestTimeoutSeconds());
	Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + SessionToken);
	PdBackendHttp::Send(Request, PdBackendHttp::FOnResponse::CreateUObject(this, &ThisClass::HandleProfileResponse));
}

void UBackendClientSubsystem::HandleProfileResponse(const PdBackendHttp::FResponse& Response)
{
	if (Response.StatusCode == 401)
	{
		HandleUnauthorized();
	}
	if (!Response.bSucceeded || !Response.Json.IsValid())
	{
		UE_LOG(LogBackendClient, Warning, TEXT("Profile request failed. %s: %s"), *Response.ErrorCode, *Response.ErrorMessage);
		OnProfileReceived.Broadcast(false, FBackendPlayerProfile());
		return;
	}

	FBackendPlayerProfile Profile;
	Profile.PlayerId = GetStringField(Response.Json, TEXT("playerId"));
	Profile.DisplayName = GetStringField(Response.Json, TEXT("displayName"));

	const TSharedPtr<FJsonObject>* Stats = nullptr;
	if (Response.Json->TryGetObjectField(TEXT("stats"), Stats) && Stats)
	{
		Profile.Stats.Matches = GetIntField(*Stats, TEXT("matches"));
		Profile.Stats.Wins = GetIntField(*Stats, TEXT("wins"));
		Profile.Stats.Losses = GetIntField(*Stats, TEXT("losses"));
		Profile.Stats.Draws = GetIntField(*Stats, TEXT("draws"));
		Profile.Stats.Kills = GetIntField(*Stats, TEXT("kills"));
		Profile.Stats.Deaths = GetIntField(*Stats, TEXT("deaths"));
	}

	const TArray<TSharedPtr<FJsonValue>>* Matches = nullptr;
	if (Response.Json->TryGetArrayField(TEXT("recentMatches"), Matches) && Matches)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Matches)
		{
			const TSharedPtr<FJsonObject> Match = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Match.IsValid())
			{
				continue;
			}
			FBackendMatchRecord& Record = Profile.RecentMatches.AddDefaulted_GetRef();
			Record.MatchId = GetStringField(Match, TEXT("matchId"));
			Record.RecordedAt = GetStringField(Match, TEXT("recordedAt"));
			Record.MapKey = GetStringField(Match, TEXT("mapKey"));
			Record.Result = GetStringField(Match, TEXT("result"));
			Record.Team = GetIntField(Match, TEXT("team"));
			Record.Kills = GetIntField(Match, TEXT("kills"));
			Record.Deaths = GetIntField(Match, TEXT("deaths"));
		}
	}

	UE_LOG(LogBackendClient, Log, TEXT("Profile %s (%s): matches=%d wins=%d losses=%d draws=%d kills=%d deaths=%d recent=%d"),
		*Profile.DisplayName, *Profile.PlayerId, Profile.Stats.Matches, Profile.Stats.Wins, Profile.Stats.Losses,
		Profile.Stats.Draws, Profile.Stats.Kills, Profile.Stats.Deaths, Profile.RecentMatches.Num());
	OnProfileReceived.Broadcast(true, Profile);
}

void UBackendClientSubsystem::ConnectToLocalServer(const FString& Address)
{
	APlayerController* PlayerController = GetLocalPlayerController();
	if (!IsLoggedIn() || !PlayerController || Address.IsEmpty())
	{
		UE_LOG(LogBackendClient, Warning, TEXT("Local server connection needs a backend login and an address."));
		return;
	}

	PlayerController->ClientTravel(FString::Printf(TEXT("%s?PlayerId=%s"), *Address, *PlayerId), TRAVEL_Absolute);
}

// 만료된 토큰은 버리고, 다음 요청에서 다시 로그인하게 한다.
void UBackendClientSubsystem::HandleUnauthorized()
{
	UE_LOG(LogBackendClient, Warning, TEXT("Backend session expired. Log in again."));
	LoginState = EBackendLoginState::LoggedOut;
	SessionToken.Reset();
}

FString UBackendClientSubsystem::ResolveSteamDisplayName() const
{
	const IOnlineSubsystem* SteamSubsystem = Online::GetSubsystem(GetWorld(), STEAM_SUBSYSTEM);
	const IOnlineIdentityPtr Identity = SteamSubsystem ? SteamSubsystem->GetIdentityInterface() : nullptr;
	const FString Nickname = Identity.IsValid() ? Identity->GetPlayerNickname(0) : FString();
	return Nickname.IsEmpty() ? FString(FPlatformProcess::UserName()) : Nickname;
}

APlayerController* UBackendClientSubsystem::GetLocalPlayerController() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
}
