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
#include "Profile/PlayerProfileSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BackendClientSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogBackendClient, Log, All);

namespace
{
	/**
	 * 백엔드는 한 번에 약 24초만 세션 활성화를 기다린다(API Gateway 30초 한도). 보스 레이드 서버는 맵 준비를 최대 60초까지
	 * 기다리므로, 시간 초과 응답이면 같은 요청을 이만큼 더 보낸다. 백엔드는 활성화 중인 같은 모드 세션을 이어서 기다린다.
	 */
	constexpr int32 MaxMatchJoinTimeoutRetries = 2;
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

	int64 GetInt64Field(const TSharedPtr<FJsonObject>& Json, const TCHAR* FieldName)
	{
		int64 Value = 0;
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
	FAutoConsoleCommandWithWorldAndArgs LoginCommand(TEXT("pd.Backend.Login"),
		TEXT("Log in to the backend with Steam (or -BackendDevLogin=<id>)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UBackendClientSubsystem* Backend = FindBackendClient(World))
			{
				Backend->Login();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs DevLoginCommand(TEXT("pd.Backend.DevLogin"),
		TEXT("pd.Backend.DevLogin <id>: developer login (backend stage must allow it)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UBackendClientSubsystem* Backend = FindBackendClient(World);
			if (Backend && Args.Num() > 0)
			{
				Backend->LoginAsDeveloper(Args[0]);
			}
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

	FAutoConsoleCommandWithWorldAndArgs ProfileCommand(TEXT("pd.Backend.Profile"),
		TEXT("Print the logged-in player's stats and recent matches."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UBackendClientSubsystem* Backend = FindBackendClient(World))
			{
				Backend->RequestMyProfile();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs ConnectLocalCommand(
		TEXT("pd.Backend.ConnectLocal"),
		TEXT("pd.Backend.ConnectLocal <ip:port>: join a local dedicated server with the backend PlayerId (no GameLift)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UBackendClientSubsystem* Backend = FindBackendClient(World);
			if (Backend && Args.Num() > 0)
			{
				Backend->ConnectToLocalServer(Args[0]);
			}
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

	TArray<TUniqueFunction<void(bool, const FString&)>> Requests = MoveTemp(RequestsAfterLogin);
	RequestsAfterLogin.Reset();
	for (TUniqueFunction<void(bool, const FString&)>& Request : Requests)
	{
		Request(bSucceeded, ErrorMessage);
	}
}

void UBackendClientSubsystem::RunAfterLogin(TUniqueFunction<void(bool bLoggedIn, const FString& LoginError)> Request)
{
	if (IsLoggedIn())
	{
		Request(true, FString());
		return;
	}

	RequestsAfterLogin.Add(MoveTemp(Request));
	if (LoginState != EBackendLoginState::LoggingIn)
	{
		Login();
	}
}

TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> UBackendClientSubsystem::CreateAuthorizedRequest(
	const TCHAR* Verb, const TCHAR* Path) const
{
	const UBackendSettings* Settings = GetDefault<UBackendSettings>();
	const FString Url = Settings->BuildApiUrl(Path);
	if (Url.IsEmpty() || !IsLoggedIn())
	{
		return nullptr;
	}
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		PdBackendHttp::CreateJsonRequest(Verb, Url, nullptr, Settings->GetClientRequestTimeoutSeconds());
	Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + SessionToken);
	return Request;
}

void UBackendClientSubsystem::JoinOnlineMatch(const EOnlineMatchMode Mode)
{
	if (bMatchJoinInProgress)
	{
		return;
	}

	bMatchJoinInProgress = true;
	MatchJoinTimeoutRetries = 0;
	PendingMatchMode = Mode;
	LastMatchJoinErrorCode.Reset();
	RunAfterLogin([this](const bool bLoggedIn, const FString& LoginError)
	{
		if (bLoggedIn)
		{
			SendMatchJoinRequest();
		}
		else
		{
			FinishMatchJoin(false, LoginError);
		}
	});
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
	if (!Response.bSucceeded && Response.ErrorCode == PdBackendHttp::ErrorCodes::SessionTimeout
		&& MatchJoinTimeoutRetries < MaxMatchJoinTimeoutRetries)
	{
		++MatchJoinTimeoutRetries;
		UE_LOG(LogBackendClient, Log, TEXT("Game session is still starting; asking the backend again (%d/%d)."),
			MatchJoinTimeoutRetries, MaxMatchJoinTimeoutRetries);
		SendMatchJoinRequest();
		return;
	}
	if (!Response.bSucceeded)
	{
		LastMatchJoinErrorCode = Response.ErrorCode;
		if (Response.ErrorCode == PdBackendHttp::ErrorCodes::BossRaidClosed)
		{
			// 타이틀에서 개최 여부를 받은 뒤에 레이드가 닫혔다.
			SetBossRaidOpen(false);
		}
		else if (Response.ErrorCode == PdBackendHttp::ErrorCodes::NoAttemptsLeft)
		{
			BossRaidStanding.bReceived = true;
			BossRaidStanding.AttemptsLeft = 0;
			OnBossRaidStandingUpdated.Broadcast();
		}
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
	if (PendingMatchMode == EOnlineMatchMode::BossRaid)
	{
		BossRaidStanding.AttemptsLeft = GetIntField(Response.Json, TEXT("attemptsLeft"));
	}
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

void UBackendClientSubsystem::RequestBossRaidStatus()
{
	const UBackendSettings* Settings = GetDefault<UBackendSettings>();
	const FString Url = Settings->BuildApiUrl(TEXT("/bossraid/status"));
	if (Url.IsEmpty())
	{
		UE_LOG(LogBackendClient, Warning, TEXT("Boss raid status request needs ApiBaseUrl."));
		return;
	}

	PdBackendHttp::Send(
		PdBackendHttp::CreateJsonRequest(TEXT("GET"), Url, nullptr, Settings->GetClientRequestTimeoutSeconds()),
		PdBackendHttp::FOnResponse::CreateUObject(this, &ThisClass::HandleBossRaidStatusResponse));
}

// 답을 받지 못하면 마지막 값을 그대로 둔다. 참가 요청은 백엔드가 다시 확인한다.
void UBackendClientSubsystem::HandleBossRaidStatusResponse(const PdBackendHttp::FResponse& Response)
{
	if (!Response.bSucceeded || !Response.Json.IsValid() || !Response.Json->HasTypedField<EJson::Boolean>(TEXT("open")))
	{
		UE_LOG(LogBackendClient, Warning, TEXT("Boss raid status request failed. %s: %s"), *Response.ErrorCode, *Response.ErrorMessage);
		return;
	}
	ReadBossRaidStatus(*Response.Json);
	SetBossRaidOpen(BossRaidStatus.bOpen);
}

// GET /bossraid/status와 /bossraid/me가 같은 모양({"open": bool, "raid": 회차 또는 null})으로 돌려준다.
void UBackendClientSubsystem::ReadBossRaidStatus(const FJsonObject& Json)
{
	BossRaidStatus = FBackendBossRaidStatus();
	Json.TryGetBoolField(TEXT("open"), BossRaidStatus.bOpen);
	const TSharedPtr<FJsonObject>* Raid = nullptr;
	if (Json.TryGetObjectField(TEXT("raid"), Raid) && Raid)
	{
		BossRaidStatus.bHasRaid = true;
		(*Raid)->TryGetBoolField(TEXT("defeated"), BossRaidStatus.bDefeated);
		BossRaidStatus.Health = GetInt64Field(*Raid, TEXT("hp"));
		BossRaidStatus.MaxHealth = GetInt64Field(*Raid, TEXT("maxHp"));
		BossRaidStatus.DailyAttempts = GetIntField(*Raid, TEXT("dailyAttempts"));
		BossRaidStatus.RewardGold = GetIntField(*Raid, TEXT("rewardGold"));
	}
}

void UBackendClientSubsystem::SetBossRaidOpen(const bool bOpen)
{
	BossRaidStatus.bOpen = bOpen;
	OnBossRaidStatusUpdated.Broadcast(bOpen);
}

void UBackendClientSubsystem::RefreshBossRaidStanding()
{
	if (bBossRaidStandingInProgress)
	{
		return;
	}

	bBossRaidStandingInProgress = true;
	RunAfterLogin([this](const bool bLoggedIn, const FString& LoginError)
	{
		const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request =
			bLoggedIn ? CreateAuthorizedRequest(TEXT("POST"), TEXT("/bossraid/claim")) : nullptr;
		if (!Request.IsValid())
		{
			UE_LOG(LogBackendClient, Warning, TEXT("Boss raid standing needs a backend login and ApiBaseUrl. %s"), *LoginError);
			bBossRaidStandingInProgress = false;
			return;
		}
		PdBackendHttp::Send(Request.ToSharedRef(),
			PdBackendHttp::FOnResponse::CreateUObject(this, &ThisClass::HandleBossRaidClaimResponse));
	});
}

// 백엔드는 받았다고 기록한 뒤에 골드를 돌려주므로 바로 로컬 프로필에 더한다. 요청이 실패하면 다음에 다시 받는다.
void UBackendClientSubsystem::HandleBossRaidClaimResponse(const PdBackendHttp::FResponse& Response)
{
	if (Response.StatusCode == 401)
	{
		HandleUnauthorized();
	}
	if (!Response.bSucceeded)
	{
		UE_LOG(LogBackendClient, Warning, TEXT("Boss raid reward claim failed. %s: %s"), *Response.ErrorCode, *Response.ErrorMessage);
	}
	const int32 Gold = Response.bSucceeded ? GetIntField(Response.Json, TEXT("gold")) : 0;
	if (Gold > 0)
	{
		if (UPlayerProfileSubsystem* Profile = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance()))
		{
			Profile->AddGold(Gold);
		}
		UE_LOG(LogBackendClient, Log, TEXT("Boss raid reward: %d gold."), Gold);
		OnBossRaidRewardClaimed.Broadcast(Gold);
	}

	const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = CreateAuthorizedRequest(TEXT("GET"), TEXT("/bossraid/me"));
	if (!Request.IsValid())
	{
		bBossRaidStandingInProgress = false;
		return;
	}
	PdBackendHttp::Send(Request.ToSharedRef(),
		PdBackendHttp::FOnResponse::CreateUObject(this, &ThisClass::HandleBossRaidStandingResponse));
}

void UBackendClientSubsystem::HandleBossRaidStandingResponse(const PdBackendHttp::FResponse& Response)
{
	bBossRaidStandingInProgress = false;
	if (Response.StatusCode == 401)
	{
		HandleUnauthorized();
	}
	if (!Response.bSucceeded || !Response.Json.IsValid())
	{
		UE_LOG(LogBackendClient, Warning, TEXT("Boss raid standing request failed. %s: %s"), *Response.ErrorCode, *Response.ErrorMessage);
		return;
	}

	ReadBossRaidStatus(*Response.Json);
	BossRaidStanding = FBackendBossRaidStanding();
	BossRaidStanding.bReceived = true;
	BossRaidStanding.AttemptsLeft = GetIntField(Response.Json, TEXT("attemptsLeft"));
	BossRaidStanding.Damage = GetInt64Field(Response.Json, TEXT("damage"));
	BossRaidStanding.Rank = GetIntField(Response.Json, TEXT("rank"));
	const TArray<TSharedPtr<FJsonValue>>* Top = nullptr;
	if (Response.Json->TryGetArrayField(TEXT("top"), Top) && Top)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Top)
		{
			const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
			if (Entry.IsValid())
			{
				BossRaidStanding.Top.Add({GetIntField(Entry, TEXT("rank")), GetStringField(Entry, TEXT("name")),
					GetInt64Field(Entry, TEXT("damage"))});
			}
		}
	}
	SetBossRaidOpen(BossRaidStatus.bOpen);
	OnBossRaidStandingUpdated.Broadcast();
}

bool UBackendClientSubsystem::ConsumeBossRaidPlayed()
{
	const bool bPlayed = bBossRaidPlayed;
	bBossRaidPlayed = false;
	return bPlayed;
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
