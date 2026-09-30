#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IHttpRequest.h"

struct FAwsCredentials;

/**
 * 백엔드 JSON API 호출을 묶는다. 응답 콜백은 HTTP 모듈이 게임 스레드에서 실행한다.
 */
namespace PdBackendHttp
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

	DECLARE_DELEGATE_OneParam(FOnResponse, const FResponse&);

	LABPROJECT_API FString SerializeJson(const TSharedRef<FJsonObject>& Object);

	/** JSON 본문과 공통 헤더를 설정한 요청을 만든다. Body가 없으면 본문 없이 보낸다. */
	LABPROJECT_API TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateJsonRequest(
		const FString& Verb, const FString& Url, const TSharedPtr<FJsonObject>& Body, float TimeoutSeconds);

	/** 현재 URL·메서드·본문으로 SigV4 서명 헤더를 붙인다. URL을 해석할 수 없으면 false. */
	LABPROJECT_API bool SignWithSigV4(
		IHttpRequest& Request, const FAwsCredentials& Credentials, const FString& Region, const FString& Service);

	LABPROJECT_API void Send(const TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& Request, FOnResponse OnResponse);
}
