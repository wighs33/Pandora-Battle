#include "Luna/LunaChatSubsystem.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Localization/MenuLocalizationSubsystem.h"
#include "Common/UiLanguage.h"
#include "Luna/LunaChatSettings.h"
#include "Online/Backend/BackendHttp.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LunaChatSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogLunaChat, Log, All);

/** HTTP 스레드가 받은 응답 바이트를 게임 스레드로 넘기는 버퍼 */
struct FLunaChatStreamBuffer
{
	void Append(const void* Data, const int64 Num)
	{
		FScopeLock Lock(&Mutex);
		Bytes.Append(static_cast<const uint8*>(Data), static_cast<int32>(Num));
	}

	TArray<uint8> Take()
	{
		FScopeLock Lock(&Mutex);
		return MoveTemp(Bytes);
	}

private:
	FCriticalSection Mutex;
	TArray<uint8> Bytes;
};

namespace
{
	/** 말풍선 한두 줄에 들어가는 길이 */
	constexpr int32 MaxReplyCharacters = 120;

	FAutoConsoleCommandWithWorldAndArgs AskLunaCommand(
		TEXT("pd.Luna.Ask"),
		TEXT("pd.Luna.Ask <question>: ask Luna through the local Ollama model."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			if (ULunaChatSubsystem* Luna = GameInstance ? GameInstance->GetSubsystem<ULunaChatSubsystem>() : nullptr)
			{
				Luna->Ask(FString::Join(Args, TEXT(" ")));
			}
		}));
}

bool ULunaChatSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void ULunaChatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UMenuLocalizationSubsystem* Localization = Collection.InitializeDependency<UMenuLocalizationSubsystem>())
	{
		Localization->OnLanguageChanged.AddDynamic(this, &ThisClass::HandleMenuLanguageChanged);
	}
}

void ULunaChatSubsystem::Deinitialize()
{
	if (UMenuLocalizationSubsystem* Localization = GetGameInstance()->GetSubsystem<UMenuLocalizationSubsystem>())
	{
		Localization->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleMenuLanguageChanged);
	}
	CancelReply();
	Super::Deinitialize();
}

bool ULunaChatSubsystem::Ask(const FString& Question)
{
	const ULunaChatSettings* Settings = GetDefault<ULunaChatSettings>();
	const FString TrimmedQuestion = Question.TrimStartAndEnd().Left(Settings->GetMaxQuestionLength());
	if (TrimmedQuestion.IsEmpty())
	{
		return false;
	}

	CancelReply();
	PendingQuestion = TrimmedQuestion;
	ReplyText.Reset();
	Parser = LunaChat::FStreamParser();
	StreamBuffer = MakeShared<FLunaChatStreamBuffer, ESPMode::ThreadSafe>();
	const uint32 RequestSerial = ++ActiveRequestSerial;

	FString Url = Settings->GetOllamaUrl();
	while (Url.EndsWith(TEXT("/")))
	{
		Url.LeftChopInline(1);
	}

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(TEXT("POST"));
	Request->SetURL(Url + TEXT("/api/chat"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(PdBackendHttp::SerializeJson(BuildRequestBody(TrimmedQuestion)));
	Request->SetTimeout(Settings->GetRequestTimeoutSeconds());

	// 스트림 콜백은 HTTP 스레드에서 오므로 바이트만 쌓고, 해석과 알림은 게임 스레드에서 한다.
	const TWeakObjectPtr<ThisClass> WeakThis(this);
	const TSharedPtr<FLunaChatStreamBuffer, ESPMode::ThreadSafe> Buffer = StreamBuffer;
	bStreamingResponse = Request->SetResponseBodyReceiveStreamDelegateV2(FHttpRequestStreamDelegateV2::CreateLambda(
		[Buffer, WeakThis, RequestSerial](void* Data, int64& Length)
		{
			Buffer->Append(Data, Length);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, RequestSerial]()
			{
				if (ULunaChatSubsystem* This = WeakThis.Get())
				{
					This->DrainStream(RequestSerial);
				}
			});
		}));
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[this, RequestSerial](FHttpRequestPtr, const FHttpResponsePtr Response, const bool bConnectedSuccessfully)
		{
			HandleRequestComplete(Response, bConnectedSuccessfully, RequestSerial);
		});

	ActiveRequest = Request;
	UE_LOG(LogLunaChat, Log, TEXT("Asking Luna (%s): %s"), *Settings->GetModel(), *TrimmedQuestion);
	Request->ProcessRequest();
	return true;
}

void ULunaChatSubsystem::CancelReply()
{
	if (!ActiveRequest.IsValid())
	{
		return;
	}

	// 취소도 완료 콜백을 부르므로, 번호를 먼저 바꿔 늦게 오는 콜백과 스트림 조각을 무시한다.
	const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = ActiveRequest;
	ActiveRequest.Reset();
	StreamBuffer.Reset();
	++ActiveRequestSerial;
	Request->CancelRequest();
}

void ULunaChatSubsystem::ResetConversation()
{
	History.Reset();
}

// 대화 기록은 이전 언어로 쌓였으므로, 새 언어의 시스템 프롬프트와 섞이지 않게 비운다.
void ULunaChatSubsystem::HandleMenuLanguageChanged()
{
	ResetConversation();
}

void ULunaChatSubsystem::DrainStream(const uint32 RequestSerial)
{
	if (RequestSerial != ActiveRequestSerial || !ActiveRequest.IsValid() || !StreamBuffer.IsValid())
	{
		return;
	}

	const TArray<uint8> Bytes = StreamBuffer->Take();
	if (Bytes.IsEmpty())
	{
		return;
	}

	Parser.Append(Bytes);
	FString Content;
	bool bDone = false;
	FString Error;
	Parser.Consume(Content, bDone, Error);
	if (!Error.IsEmpty())
	{
		FinishReply(false, Error);
		return;
	}

	if (!Content.IsEmpty())
	{
		ReplyText += Content;
		const FString Visible = LunaChat::ToBubbleText(LunaChat::StripThinking(ReplyText));
		if (!Visible.IsEmpty())
		{
			ReplyUpdated.Broadcast(Visible);
		}
	}
	if (bDone)
	{
		FinishReply(true, FString());
	}
}

void ULunaChatSubsystem::HandleRequestComplete(
	const FHttpResponsePtr Response, const bool bConnectedSuccessfully, const uint32 RequestSerial)
{
	if (RequestSerial != ActiveRequestSerial || !ActiveRequest.IsValid())
	{
		return;
	}

	// 완료 콜백이 마지막 스트림 조각보다 먼저 올 수 있으므로 남은 바이트를 먼저 처리한다.
	DrainStream(RequestSerial);
	if (!ActiveRequest.IsValid())
	{
		return;
	}

	// 스트림 콜백을 지원하지 않는 HTTP 구현이면 응답 본문 전체를 한 번에 해석한다.
	if (!bStreamingResponse && Response.IsValid())
	{
		Parser.Append(Response->GetContent());
	}
	FString Content;
	bool bDone = false;
	FString Error;
	Parser.Flush(Content, bDone, Error);
	ReplyText += Content;

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		FinishReply(false, FString::Printf(TEXT("Could not reach Ollama at %s. Is it running?"),
			*GetDefault<ULunaChatSettings>()->GetOllamaUrl()));
		return;
	}
	const int32 StatusCode = Response->GetResponseCode();
	if (!Error.IsEmpty() || !EHttpResponseCodes::IsOk(StatusCode))
	{
		FinishReply(false, Error.IsEmpty() ? FString::Printf(TEXT("Ollama returned HTTP %d."), StatusCode) : Error);
		return;
	}
	FinishReply(true, FString());
}

void ULunaChatSubsystem::FinishReply(const bool bSucceeded, const FString& Error)
{
	const FString Reply = LunaChat::ToBubbleText(LunaChat::StripThinking(ReplyText));
	ActiveRequest.Reset();
	StreamBuffer.Reset();

	if (bSucceeded && !Reply.IsEmpty())
	{
		History.Add({TEXT("user"), PendingQuestion});
		History.Add({TEXT("assistant"), Reply});
		const int32 MaxMessages = GetDefault<ULunaChatSettings>()->GetMaxHistoryTurns() * 2;
		if (History.Num() > MaxMessages)
		{
			History.RemoveAt(0, History.Num() - MaxMessages);
		}
		UE_LOG(LogLunaChat, Log, TEXT("Luna: %s"), *Reply);
		PendingQuestion.Reset();
		ReplyFinished.Broadcast(true, Reply);
		return;
	}

	UE_LOG(LogLunaChat, Warning, TEXT("Luna could not answer: %s"), Error.IsEmpty() ? TEXT("the model returned no text") : *Error);
	PendingQuestion.Reset();
	ReplyFinished.Broadcast(false, FString());
}

// 게임에 대한 답은 DT_MenuText의 Luna 안내 문구에서만 가져오게 하고, 답하는 언어는 현재 메뉴 언어로 고정한다.
FString ULunaChatSubsystem::BuildSystemPrompt() const
{
	const UMenuLocalizationSubsystem* Localization = GetGameInstance()->GetSubsystem<UMenuLocalizationSubsystem>();
	const EGuideLanguage Language = Localization ? Localization->GetLanguage() : EGuideLanguage::Korean;
	const FString LanguageName = StaticEnum<EGuideLanguage>()->GetNameStringByValue(static_cast<int64>(Language));

	FString Prompt = FString::Printf(TEXT(
		"You are Luna, the guide of the multiplayer action game \"Pandora Battle\". "
		"You appear on the title screen and talk with the player through a small speech bubble.\n"
		"Rules:\n"
		"- Always reply in %s (%s), whatever language the player writes in.\n"
		"- Keep every reply to one or two short sentences, at most about %d characters. Do not use lists, markdown or emoji.\n"
		"- Speak warmly and politely, in the first person, as Luna.\n"
		"- Answer questions about the game only from the facts below. If they do not cover the question, say you are not sure "
		"and suggest the Guide menu on the title screen. Never invent features.\n"
		"- For questions unrelated to the game, answer briefly and gently bring the conversation back to Pandora Battle.\n"
		"Game facts:\n"),
		*LanguageName, *UiLanguage::NativeName(Language), MaxReplyCharacters);

	if (Localization)
	{
		for (int32 TipNumber = 1; TipNumber <= LunaChat::TipCount; ++TipNumber)
		{
			const FString Fact = Localization->GetText(LunaChat::TipKey(TipNumber)).ToString();
			if (!Fact.IsEmpty())
			{
				Prompt += TEXT("- ") + Fact + TEXT("\n");
			}
		}
	}
	return Prompt;
}

TSharedRef<FJsonObject> ULunaChatSubsystem::BuildRequestBody(const FString& Question) const
{
	const ULunaChatSettings* Settings = GetDefault<ULunaChatSettings>();
	TArray<TSharedPtr<FJsonValue>> Messages;
	const auto AddMessage = [&Messages](const FString& Role, const FString& Content)
	{
		const TSharedRef<FJsonObject> Message = MakeShared<FJsonObject>();
		Message->SetStringField(TEXT("role"), Role);
		Message->SetStringField(TEXT("content"), Content);
		Messages.Add(MakeShared<FJsonValueObject>(Message));
	};

	AddMessage(TEXT("system"), BuildSystemPrompt());
	for (const FChatMessage& Message : History)
	{
		AddMessage(Message.Role, Message.Content);
	}
	AddMessage(TEXT("user"), Question);

	const TSharedRef<FJsonObject> Options = MakeShared<FJsonObject>();
	Options->SetNumberField(TEXT("temperature"), Settings->GetTemperature());
	Options->SetNumberField(TEXT("num_predict"), Settings->GetMaxReplyTokens());

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("model"), Settings->GetModel());
	Body->SetBoolField(TEXT("stream"), true);
	Body->SetStringField(TEXT("keep_alive"), Settings->GetKeepAlive());
	Body->SetArrayField(TEXT("messages"), Messages);
	Body->SetObjectField(TEXT("options"), Options);
	return Body;
}
