#include "Online/Backend/BackendHttp.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace BackendHttp
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
