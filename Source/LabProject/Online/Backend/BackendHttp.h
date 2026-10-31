#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IHttpRequest.h"

/**
 * 백엔드 JSON API 호출을 묶는다. 응답 콜백은 HTTP 모듈이 게임 스레드에서 실행한다.
 */
namespace BackendHttp
{
	struct FResponse
	{
		/** 연결에 성공하고 2xx 응답을 받았는지 여부 */
		bool bSucceeded = false;
		int32 StatusCode = 0;
		/** 응답 본문이 JSON 객체면 채워진다. */
		TSharedPtr<FJsonObject> Json;
		/** 실패 시 백엔드의 error 코드, 없으면 연결 오류 설명 */
		FString ErrorCode;
		FString ErrorMessage;
	};

	/** 클라이언트가 따로 처리하는 백엔드 error 코드. 값은 Backend/server의 응답과 같다. */
	namespace ErrorCodes
	{
		inline constexpr const TCHAR* SessionTimeout = TEXT("session_timeout");
		inline constexpr const TCHAR* BossRaidClosed = TEXT("bossraid_closed");
		inline constexpr const TCHAR* NoAttemptsLeft = TEXT("no_attempts_left");
	}

	/** 전용 서버 보고 경로(/server/*)가 확인하는 서버 키 헤더. Backend/server의 require_server가 읽는다. */
	inline constexpr const TCHAR* ServerKeyHeader = TEXT("X-Server-Key");

	DECLARE_DELEGATE_OneParam(FOnResponse, const FResponse&);

	LABPROJECT_API FString SerializeJson(const TSharedRef<FJsonObject>& Object);

	/** JSON 본문과 공통 헤더를 설정한 요청을 만든다. Body가 없으면 본문 없이 보낸다. */
	LABPROJECT_API TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateJsonRequest(
		const FString& Verb, const FString& Url, const TSharedPtr<FJsonObject>& Body, float TimeoutSeconds);

	LABPROJECT_API void Send(const TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& Request, FOnResponse OnResponse);
}
