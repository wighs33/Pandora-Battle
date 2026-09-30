#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BackendSettings.generated.h"

/**
 * 로그인·매치 참가·전적 API의 주소와 AWS 서명 정보를 설정한다.
 *
 * 배포 스크립트(Backend/deploy.ps1)가 출력하는 ApiUrl을 ApiBaseUrl에 넣는다.
 * 실행 인자 -BackendUrl=, -FleetRoleArn=이 있으면 설정 파일 값보다 우선한다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Backend"))
class LABPROJECT_API UBackendSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// Public API ------------------------------------------------------------------------------------------------------
	/** 끝의 '/'를 뺀 API 주소. 설정이 없으면 빈 문자열이다. */
	FString GetApiBaseUrl() const;
	FString BuildApiUrl(const TCHAR* Path) const;
	const FString& GetAwsRegion() const { return AwsRegion; }
	const FString& GetSteamWebApiIdentity() const { return SteamWebApiIdentity; }
	float GetClientRequestTimeoutSeconds() const { return ClientRequestTimeoutSeconds; }
	float GetServerReportTimeoutSeconds() const { return ServerReportTimeoutSeconds; }
	/** 관리형 EC2 플릿에서 GetFleetRoleCredentials로 받을 인스턴스 역할 ARN. */
	FString GetGameServerRoleArn() const;

private:
	UPROPERTY(Config, EditAnywhere, Category = "Backend")
	FString ApiBaseUrl;

	/** SigV4 서명에 쓰는 API Gateway 리전. */
	UPROPERTY(Config, EditAnywhere, Category = "Backend")
	FString AwsRegion = TEXT("ap-northeast-2");

	/** 클라이언트의 Steam Web API 티켓 identity. 백엔드의 SteamTicketIdentity와 같아야 한다. */
	UPROPERTY(Config, EditAnywhere, Category = "Backend|Steam")
	FString SteamWebApiIdentity = TEXT("labproject-backend");

	/** 매치 참가 요청은 백엔드가 새 게임 세션이 ACTIVE가 될 때까지 기다리므로 API Gateway 한도(30초)에 맞춘다. */
	UPROPERTY(Config, EditAnywhere, Category = "Backend", meta = (ClampMin = "1.0"))
	float ClientRequestTimeoutSeconds = 30.0f;

	/** 서버는 결과 보고가 끝나야 GameLift 프로세스를 종료하므로 짧게 둔다. */
	UPROPERTY(Config, EditAnywhere, Category = "Backend|Server", meta = (ClampMin = "1.0"))
	float ServerReportTimeoutSeconds = 10.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Backend|Server")
	FString GameServerRoleArn;
};
