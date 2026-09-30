#include "Online/Backend/MatchReportSubsystem.h"

#include "Algo/Count.h"
#include "Engine/GameInstance.h"
#include "Online/Backend/AwsSigV4.h"
#include "Online/Backend/BackendHttp.h"
#include "Online/Backend/BackendSettings.h"
#include "Online/GameLift/GameLiftServerSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MatchReportSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogMatchReport, Log, All);

namespace
{
	TSharedRef<FJsonObject> BuildReportJson(const FMatchReport& Report)
	{
		const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("matchId"), Report.MatchId);
		Body->SetStringField(TEXT("mapKey"), Report.MapKey);
		Body->SetStringField(TEXT("endReason"), Report.EndReason);
		Body->SetNumberField(TEXT("winnerTeam"), Report.WinnerTeam);

		TArray<TSharedPtr<FJsonValue>> Players;
		for (const FMatchReportPlayer& Player : Report.Players)
		{
			const TSharedRef<FJsonObject> PlayerJson = MakeShared<FJsonObject>();
			PlayerJson->SetStringField(TEXT("playerId"), Player.PlayerId);
			PlayerJson->SetStringField(TEXT("displayName"), Player.DisplayName);
			PlayerJson->SetNumberField(TEXT("team"), Player.Team);
			PlayerJson->SetNumberField(TEXT("kills"), Player.Kills);
			PlayerJson->SetNumberField(TEXT("deaths"), Player.Deaths);
			PlayerJson->SetStringField(TEXT("result"), Player.Result);
			Players.Add(MakeShared<FJsonValueObject>(PlayerJson));
		}
		Body->SetArrayField(TEXT("players"), Players);
		return Body;
	}
}

bool UMatchReportSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

FString UMatchReportSubsystem::CreateMatchId() const
{
	const UGameLiftServerSubsystem* GameLift = GetGameInstance()->GetSubsystem<UGameLiftServerSubsystem>();
	if (GameLift && GameLift->IsGameLiftActive() && !GameLift->GetGameSessionId().IsEmpty())
	{
		return GameLift->GetGameSessionId();
	}
	return FString::Printf(TEXT("local-%s"), *FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
}

void UMatchReportSubsystem::ReportMatch(const FMatchReport& Report)
{
	const UBackendSettings* Settings = GetDefault<UBackendSettings>();
	const FString Url = Settings->BuildApiUrl(TEXT("/server/match-result"));
	if (Url.IsEmpty())
	{
		UE_LOG(LogMatchReport, Log, TEXT("Backend ApiBaseUrl is not configured. Match %s is not reported."), *Report.MatchId);
		return;
	}

	FAwsCredentials Credentials;
	UGameLiftServerSubsystem* GameLift = GetGameInstance()->GetSubsystem<UGameLiftServerSubsystem>();
	if (!GameLift || !GameLift->TryGetServiceCredentials(Credentials))
	{
		UE_LOG(LogMatchReport, Warning, TEXT("No AWS credentials for the game server. Match %s is not reported."), *Report.MatchId);
		return;
	}

	const int32 PlayersWithoutId = Algo::CountIf(Report.Players,
		[](const FMatchReportPlayer& Player) { return Player.PlayerId.IsEmpty(); });
	if (PlayersWithoutId > 0)
	{
		UE_LOG(LogMatchReport, Warning, TEXT("Match %s: %d of %d players have no backend player ID. Only the match record keeps them."),
			*Report.MatchId, PlayersWithoutId, Report.Players.Num());
	}

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = PdBackendHttp::CreateJsonRequest(
		TEXT("POST"), Url, BuildReportJson(Report), Settings->GetServerReportTimeoutSeconds());
	if (!PdBackendHttp::SignWithSigV4(*Request, Credentials, Settings->GetAwsRegion(), TEXT("execute-api")))
	{
		UE_LOG(LogMatchReport, Warning, TEXT("Could not sign the match report for %s."), *Url);
		return;
	}

	++PendingReportCount;
	const FString MatchId = Report.MatchId;
	PdBackendHttp::Send(Request, PdBackendHttp::FOnResponse::CreateWeakLambda(this,
		[this, MatchId](const PdBackendHttp::FResponse& Response)
		{
			--PendingReportCount;
			if (Response.bSucceeded)
			{
				bool bDuplicate = false;
				if (Response.Json.IsValid())
				{
					Response.Json->TryGetBoolField(TEXT("duplicate"), bDuplicate);
				}
				UE_LOG(LogMatchReport, Log, TEXT("Match %s reported%s."), *MatchId,
					bDuplicate ? TEXT(" (already recorded)") : TEXT(""));
			}
			else
			{
				UE_LOG(LogMatchReport, Error, TEXT("Match %s report failed. HTTP %d %s: %s"), *MatchId,
					Response.StatusCode, *Response.ErrorCode, *Response.ErrorMessage);
			}
		}));
}
