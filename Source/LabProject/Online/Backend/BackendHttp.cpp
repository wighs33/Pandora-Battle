#include "Online/Backend/BackendHttp.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Online/Backend/AwsSigV4.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	// "https://host[:port]/path?query"를 서명에 필요한 조각으로 나눈다. 기본 포트는 Host 헤더에 붙지 않으므로 뺀다.
	bool SplitUrl(const FString& Url, FString& OutHost, FString& OutPath, FString& OutQuery)
	{
		const int32 SchemeEnd = Url.Find(TEXT("://"));
		if (SchemeEnd == INDEX_NONE)
		{
			return false;
		}
		const FString Scheme = Url.Left(SchemeEnd).ToLower();
		const FString Rest = Url.Mid(SchemeEnd + 3);

		int32 QueryStart = INDEX_NONE;
		Rest.FindChar(TEXT('?'), QueryStart);
		const FString BeforeQuery = QueryStart == INDEX_NONE ? Rest : Rest.Left(QueryStart);
		OutQuery = QueryStart == INDEX_NONE ? FString() : Rest.Mid(QueryStart + 1);

		int32 PathStart = INDEX_NONE;
		BeforeQuery.FindChar(TEXT('/'), PathStart);
		OutHost = (PathStart == INDEX_NONE ? BeforeQuery : BeforeQuery.Left(PathStart)).ToLower();
		OutPath = PathStart == INDEX_NONE ? FString(TEXT("/")) : BeforeQuery.Mid(PathStart);

		if ((Scheme == TEXT("https") && OutHost.EndsWith(TEXT(":443"))) || (Scheme == TEXT("http") && OutHost.EndsWith(TEXT(":80"))))
		{
			OutHost.LeftInline(OutHost.Find(TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromEnd));
		}
		return !OutHost.IsEmpty();
	}
}

namespace PdBackendHttp
{
	FString SerializeJson(const TSharedRef<FJsonObject>& Object)
	{
		FString Output;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
		FJsonSerializer::Serialize(Object, Writer);
		return Output;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateJsonRequest(
		const FString& Verb, const FString& Url, const TSharedPtr<FJsonObject>& Body, const float TimeoutSeconds)
	{
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetVerb(Verb);
		Request->SetURL(Url);
		Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
		Request->SetTimeout(TimeoutSeconds);
		if (Body.IsValid())
		{
			Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
			Request->SetContentAsString(SerializeJson(Body.ToSharedRef()));
		}
		return Request;
	}

	bool SignWithSigV4(
		IHttpRequest& Request, const FAwsCredentials& Credentials, const FString& Region, const FString& Service)
	{
		PdAwsSigV4::FRequest SignRequest;
		if (!Credentials.IsValid()
			|| !SplitUrl(Request.GetURL(), SignRequest.Host, SignRequest.Path, SignRequest.Query))
		{
			return false;
		}

		SignRequest.Method = Request.GetVerb();
		SignRequest.Payload = Request.GetContent();
		SignRequest.Region = Region;
		SignRequest.Service = Service;
		SignRequest.TimestampUtc = FDateTime::UtcNow();
		const PdAwsSigV4::FSignature Signature = PdAwsSigV4::Sign(SignRequest, Credentials);

		Request.SetHeader(TEXT("X-Amz-Date"), Signature.AmzDate);
		if (!Credentials.SessionToken.IsEmpty())
		{
			Request.SetHeader(TEXT("X-Amz-Security-Token"), Credentials.SessionToken);
		}
		Request.SetHeader(TEXT("Authorization"), Signature.Authorization);
		return true;
	}

	void Send(const TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& Request, FOnResponse OnResponse)
	{
		Request->OnProcessRequestComplete().BindLambda(
			[OnResponse](FHttpRequestPtr, const FHttpResponsePtr HttpResponse, const bool bConnected)
			{
				FResponse Result;
				if (!bConnected || !HttpResponse.IsValid())
				{
					Result.ErrorCode = TEXT("connection_failed");
					Result.ErrorMessage = TEXT("Could not reach the backend.");
					OnResponse.ExecuteIfBound(Result);
					return;
				}

				Result.StatusCode = HttpResponse->GetResponseCode();
				Result.bSucceeded = EHttpResponseCodes::IsOk(Result.StatusCode);
				TSharedPtr<FJsonObject> Json;
				const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(HttpResponse->GetContentAsString());
				if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
				{
					Result.Json = Json;
				}

				if (!Result.bSucceeded)
				{
					if (Result.Json.IsValid())
					{
						Result.Json->TryGetStringField(TEXT("error"), Result.ErrorCode);
						Result.Json->TryGetStringField(TEXT("message"), Result.ErrorMessage);
					}
					if (Result.ErrorCode.IsEmpty())
					{
						Result.ErrorCode = FString::Printf(TEXT("http_%d"), Result.StatusCode);
					}
					if (Result.ErrorMessage.IsEmpty())
					{
						Result.ErrorMessage = HttpResponse->GetContentAsString().Left(256);
					}
				}
				OnResponse.ExecuteIfBound(Result);
			});
		Request->ProcessRequest();
	}
}
