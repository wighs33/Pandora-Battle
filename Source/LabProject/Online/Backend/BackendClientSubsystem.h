#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BackendClientSubsystem.generated.h"

class APlayerController;
class FJsonObject;

namespace PdBackendHttp
{
	struct FResponse;
}

UENUM(BlueprintType)
enum class EBackendLoginState : uint8
{
	LoggedOut,
	LoggingIn,
	LoggedIn
};

USTRUCT(BlueprintType)
struct FBackendPlayerStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Backend")
	int32 Matches = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Backend")
	int32 Wins = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Backend")
	int32 Losses = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Backend")
	int32 Draws = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Backend")
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Backend")
	int32 Deaths = 0;
};

USTRUCT(BlueprintType)
struct FBackendMatchRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Backend")
	FString MatchId;

	UPROPERTY(BlueprintReadOnly, Category = "Backend")
	FString RecordedAt;

	UPROPERTY(BlueprintReadOnly, Category = "Backend")
	FString MapKey;

	/** win, lose, draw */
	UPROPERTY(BlueprintReadOnly, Category = "Backend")
	FString Result;

	UPROPERTY(BlueprintReadOnly, Category = "Backend")
	int32 Team = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Backend")
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Backend")
	int32 Deaths = 0;
};

USTRUCT(BlueprintType)
struct FBackendPlayerProfile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Backend")
	FString PlayerId;

	UPROPERTY(BlueprintReadOnly, Category = "Backend")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Backend")
	FBackendPlayerStats Stats;

	UPROPERTY(BlueprintReadOnly, Category = "Backend")
	TArray<FBackendMatchRecord> RecentMatches;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBackendRequestFinished, bool, bSucceeded, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBackendProfileReceived, bool, bSucceeded, const FBackendPlayerProfile&, Profile);

/**
 * 클라이언트의 백엔드 로그인, GameLift 매치 참가, 전적 조회를 담당한다.
 *
 * Steam Web API 티켓으로 로그인해 받은 세션 토큰은 메모리에만 둔다. 매치 참가는 백엔드가 잡아 준
 * 게임 세션 주소로 이동하며, 접속 URL의 PlayerSessionId를 전용 서버가 GameLift로 검증한다.
 * 한 PC에서 클라이언트 여러 개를 시험할 때는 -BackendDevLogin=<id>로 개발용 로그인을 쓴다.
 */
UCLASS()
class LABPROJECT_API UBackendClientSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// Public API ------------------------------------------------------------------------------------------------------
	/** Steam으로 로그인한다. 실행 인자 -BackendDevLogin=<id>가 있으면 개발용 로그인을 대신 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "Backend")
	void Login();

	/** 백엔드 스테이지가 AllowDevLogin=true일 때만 성공한다. */
	UFUNCTION(BlueprintCallable, Category = "Backend")
	void LoginAsDeveloper(const FString& DevId);

	/** 필요하면 먼저 로그인한 뒤, 백엔드가 잡아 준 GameLift 게임 세션으로 이동한다. */
	UFUNCTION(BlueprintCallable, Category = "Backend")
	void JoinOnlineMatch();

	UFUNCTION(BlueprintCallable, Category = "Backend")
	void RequestMyProfile();

	/** GameLift 없이 로컬 전용 서버로 전적 기록을 시험한다. 서버는 이 경로의 PlayerId를 검증하지 않는다. */
	void ConnectToLocalServer(const FString& Address);

	UFUNCTION(BlueprintPure, Category = "Backend")
	bool IsLoggedIn() const { return LoginState == EBackendLoginState::LoggedIn; }

	UFUNCTION(BlueprintPure, Category = "Backend")
	EBackendLoginState GetLoginState() const { return LoginState; }

	UFUNCTION(BlueprintPure, Category = "Backend")
	const FString& GetPlayerId() const { return PlayerId; }

	UFUNCTION(BlueprintPure, Category = "Backend")
	const FString& GetDisplayName() const { return DisplayName; }

	// Event Handlers --------------------------------------------------------------------------------------------------
	UPROPERTY(BlueprintAssignable, Category = "Backend")
	FOnBackendRequestFinished OnLoginFinished;

	/** 성공은 게임 서버로 이동을 시작했다는 뜻이다. 접속 실패는 기존 네트워크 오류 흐름이 처리한다. */
	UPROPERTY(BlueprintAssignable, Category = "Backend")
	FOnBackendRequestFinished OnMatchJoinFinished;

	UPROPERTY(BlueprintAssignable, Category = "Backend")
	FOnBackendProfileReceived OnProfileReceived;

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void SendLoginRequest(const TCHAR* Path, const TSharedRef<FJsonObject>& Body);
	void HandleLoginResponse(const PdBackendHttp::FResponse& Response);
	void FinishLogin(bool bSucceeded, const FString& ErrorMessage);
	void SendMatchJoinRequest();
	void HandleMatchJoinResponse(const PdBackendHttp::FResponse& Response);
	void FinishMatchJoin(bool bSucceeded, const FString& ErrorMessage);
	void HandleProfileResponse(const PdBackendHttp::FResponse& Response);
	void HandleUnauthorized();
	FString ResolveSteamDisplayName() const;
	APlayerController* GetLocalPlayerController() const;

	EBackendLoginState LoginState = EBackendLoginState::LoggedOut;
	FString SessionToken;
	FString PlayerId;
	FString DisplayName;
	bool bJoinMatchAfterLogin = false;
	bool bMatchJoinInProgress = false;
};
