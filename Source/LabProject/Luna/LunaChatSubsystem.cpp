#include "Luna/LunaChatSubsystem.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Localization/MenuLocalizationSubsystem.h"
#include "Common/Ticker.h"
#include "Common/UiLanguage.h"
#include "Data/ContentDataSubsystem.h"
#include "Definition/Mode/GameInstanceDefinition.h"
#include "Definition/UI/GuideDefinition.h"
#include "Luna/LunaChatSettings.h"
#include "Online/Backend/BackendHttp.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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

	/** 답할 때 넣는 가이드 절 수. 절 하나가 200~800자라 두 개면 질문 대부분을 덮으면서 프롬프트가 짧게 유지된다. */
	constexpr int32 MaxGuideSections = 2;

	/**
	 * 절 고르기 응답의 토큰 한도. 스키마로 고정해도 모델이 JSON 사이에 줄바꿈과 공백을 넣어, 번호 한두 개에 61토큰까지 썼다.
	 * 32에서는 한국어 질문 14개 중 4개가 잘려 가이드 없이 답했다. 한도는 끝까지 쓰지 않으므로 넉넉히 둔다.
	 */
	constexpr int32 MaxSectionChoiceTokens = 128;

	/** 절 고르기 응답을 번호 배열로 고정하는 JSON 스키마(Ollama의 format) */
	const TCHAR* GuideSectionSchema = TEXT("{\"type\":\"object\",\"properties\":{\"sections\":{\"type\":\"array\",\"items\":{\"type\":\"integer\"}}},\"required\":[\"sections\"]}");

	TSharedPtr<FJsonValue> MakeMessage(const FString& Role, const FString& Content)
	{
		const TSharedRef<FJsonObject> Message = MakeShared<FJsonObject>();
		Message->SetStringField(TEXT("role"), Role);
		Message->SetStringField(TEXT("content"), Content);
		return MakeShared<FJsonValueObject>(Message);
	}

	FAutoConsoleCommandWithWorldAndArgs AskLunaCommand(TEXT("pd.Luna.Ask"),
		TEXT("pd.Luna.Ask <question>: ask Luna through the bundled model or the local Ollama."),
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
	// 가이드는 첫 질문 전에 막지 않고 불러 둔다. 이미지는 가이드 화면이 따로 불러온다.
	const TSoftObjectPtr<UGuideDefinition>& Guide = UGameInstanceDefinition::GetConfiguredDefinitionReferences().Guide;
	if (!Guide.IsNull())
	{
		GuideLease = UContentDataSubsystem::AcquireContent({Guide.ToSoftObjectPath()},
			FSimpleDelegate::CreateUObject(this, &ThisClass::RebuildGuideSections));
	}
	LocalServer = MakeUnique<FLunaLocalServer>();
	LocalServer->OnStateChanged().AddUObject(this, &ThisClass::HandleServerStateChanged);
}

void ULunaChatSubsystem::Deinitialize()
{
	if (UMenuLocalizationSubsystem* Localization = GetGameInstance()->GetSubsystem<UMenuLocalizationSubsystem>())
	{
		Localization->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleMenuLanguageChanged);
	}
	CancelReply();
	LocalServer.Reset();
	GuideLease.Reset();
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
	const uint32 RequestSerial = ++ActiveRequestSerial;
	UE_LOG(LogLunaChat, Log, TEXT("Asking Luna (%s): %s"),
		UsesBundledServer() ? *Settings->GetLocalModelFile() : *Settings->GetModel(), *TrimmedQuestion);
	if (!UsesBundledServer() || LocalServer->IsReady())
	{
		SendQuestion(RequestSerial);
		return true;
	}

	// 모델 서버가 준비되면 이어서 묻는다. 띄우지 못했으면 다음 틱에 실패를 알려, 타이틀이 "생각 중" 문구를 먼저 보여 준 뒤 덮게 한다.
	// Start는 띄우지 못하면 Failed를 그 자리에서 알리므로, 그 알림이 여기서 답을 끝내지 않게 기다림은 Start가 돌아온 뒤에 표시한다.
	const bool bStarted = LocalServer->Start();
	bWaitingForServer = true;
	if (!bStarted)
	{
		Ticker::RunNextTick(this, [this, RequestSerial]()
		{
			if (RequestSerial == ActiveRequestSerial && bWaitingForServer)
			{
				bWaitingForServer = false;
				FinishReply(false, TEXT("The bundled model server could not start."));
			}
		});
	}
	return true;
}

void ULunaChatSubsystem::SendQuestion(const uint32 RequestSerial)
{
	if (GuideSections.IsEmpty())
	{
		RequestReply({}, RequestSerial);
	}
	else
	{
		RequestGuideSections(RequestSerial);
	}
}

void ULunaChatSubsystem::PrepareModel()
{
	if (UsesBundledServer())
	{
		LocalServer->Start();
	}
}

void ULunaChatSubsystem::ReleaseModel()
{
	CancelReply();
	if (LocalServer.IsValid())
	{
		LocalServer->Stop();
	}
}

void ULunaChatSubsystem::HandleServerStateChanged(const FLunaLocalServer::EState NewState)
{
	if (!bWaitingForServer)
	{
		return;
	}
	if (NewState == FLunaLocalServer::EState::Ready)
	{
		bWaitingForServer = false;
		SendQuestion(ActiveRequestSerial);
	}
	else if (NewState == FLunaLocalServer::EState::Failed)
	{
		bWaitingForServer = false;
		FinishReply(false, TEXT("The bundled model server stopped before it was ready."));
	}
}

void ULunaChatSubsystem::RequestGuideSections(const uint32 RequestSerial)
{
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateChatRequest(BuildSectionRequestBody(PendingQuestion));
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[this, RequestSerial](FHttpRequestPtr, const FHttpResponsePtr Response, const bool bConnectedSuccessfully)
		{
			HandleGuideSectionsChosen(Response, bConnectedSuccessfully, RequestSerial);
		});
	ActiveRequest = Request;
	Request->ProcessRequest();
}

void ULunaChatSubsystem::HandleGuideSectionsChosen(
	const FHttpResponsePtr Response, const bool bConnectedSuccessfully, const uint32 RequestSerial)
{
	if (RequestSerial != ActiveRequestSerial || !ActiveRequest.IsValid())
	{
		return;
	}
	const FString Failure = DescribeRequestFailure(Response, bConnectedSuccessfully);
	if (!Failure.IsEmpty())
	{
		FinishReply(false, Failure);
		return;
	}

	// 맞는 절이 없거나 응답을 해석하지 못하면 안내 문구만으로 답하게 한다.
	const TArray<int32> Chosen = LunaChat::ParseChosenSections(Response->GetContentAsString(), GuideSections.Num(), MaxGuideSections);
	for (const int32 Index : Chosen)
	{
		UE_LOG(LogLunaChat, Log, TEXT("Guide section for the answer: %s"), *GuideSections[Index].Title);
	}
	RequestReply(Chosen, RequestSerial);
}

void ULunaChatSubsystem::RequestReply(const TArray<int32>& GuideSectionIndices, const uint32 RequestSerial)
{
	Parser = LunaChat::FStreamParser();
	StreamBuffer = MakeShared<FLunaChatStreamBuffer, ESPMode::ThreadSafe>();
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		CreateChatRequest(BuildRequestBody(PendingQuestion, GuideSectionIndices));

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
	Request->ProcessRequest();
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> ULunaChatSubsystem::CreateChatRequest(const TSharedRef<FJsonObject>& Body) const
{
	const ULunaChatSettings* Settings = GetDefault<ULunaChatSettings>();
	FString Url = UsesBundledServer() ? LocalServer->GetBaseUrl() : Settings->GetOllamaUrl();
	while (Url.EndsWith(TEXT("/")))
	{
		Url.LeftChopInline(1);
	}

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(TEXT("POST"));
	Request->SetURL(Url + (UsesBundledServer() ? TEXT("/v1/chat/completions") : TEXT("/api/chat")));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(BackendHttp::SerializeJson(Body));
	Request->SetTimeout(Settings->GetRequestTimeoutSeconds());
	return Request;
}

void ULunaChatSubsystem::CancelReply()
{
	if (bWaitingForServer)
	{
		bWaitingForServer = false;
		++ActiveRequestSerial;
	}
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
	RebuildGuideSections();
}

// 절 이름에 영어 이름을 함께 적어, 플레이어가 쓰는 외래어(크리티컬)와 가이드의 UI 용어(치명타)가 달라도 절을 고를 수 있게 한다.
// 모든 언어의 가이드는 절 구성이 같으므로 같은 순서의 영어 절(원문)을 짝지을 수 있다.
void ULunaChatSubsystem::RebuildGuideSections()
{
	GuideSections.Reset();
	GuideSectionNames.Reset();
	const UGuideDefinition* Guide = UGameInstanceDefinition::GetConfiguredDefinitionReferences().Guide.Get();
	const UMenuLocalizationSubsystem* Localization = GetGameInstance()->GetSubsystem<UMenuLocalizationSubsystem>();
	if (!Guide)
	{
		return;
	}

	const bool bEnglish = !Localization || Localization->GetLanguage() == TEXT("en");
	for (const FGuidePageEntry& Page : Guide->Pages)
	{
		const TArray<LunaChat::FGuideSection> Sections = LunaChat::SplitGuideSections(Page.Content.ToString());
		const FString* EnglishContent = FTextInspector::GetSourceString(Page.Content);
		const TArray<LunaChat::FGuideSection> EnglishSections =
			LunaChat::SplitGuideSections(EnglishContent ? *EnglishContent : FString());
		const bool bAddEnglishNames = !bEnglish && EnglishSections.Num() == Sections.Num();
		for (int32 Index = 0; Index < Sections.Num(); ++Index)
		{
			FString Name = LunaChat::DescribeGuideSection(Sections[Index]);
			if (bAddEnglishNames)
			{
				Name += TEXT(" (") + LunaChat::DescribeGuideSection(EnglishSections[Index]) + TEXT(")");
			}
			GuideSectionNames.Add(MoveTemp(Name));
			GuideSections.Add(Sections[Index]);
		}
	}
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

	const FString Failure = DescribeRequestFailure(Response, bConnectedSuccessfully);
	if (!Failure.IsEmpty() || !Error.IsEmpty())
	{
		FinishReply(false, Failure.IsEmpty() ? Error : Failure);
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

// 절 목록과 질문만 보내 답할 때 넣을 절 번호를 받는다. 형식을 JSON 스키마로 고정해 번호만 돌아오게 한다.
TSharedRef<FJsonObject> ULunaChatSubsystem::BuildSectionRequestBody(const FString& Question) const
{
	FString Prompt = FString::Printf(TEXT("Pick the guide sections that answer the player's question. "
		"Choose at most %d, or none if no section fits.\nSections:\n"), MaxGuideSections);
	for (int32 Index = 0; Index < GuideSectionNames.Num(); ++Index)
	{
		Prompt += FString::Printf(TEXT("%d. %s\n"), Index + 1, *GuideSectionNames[Index]);
	}

	TSharedPtr<FJsonObject> Schema;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(GuideSectionSchema), Schema);
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetArrayField(TEXT("messages"), {MakeMessage(TEXT("system"), Prompt), MakeMessage(TEXT("user"), Question)});
	// 같은 질문에는 늘 같은 절을 고르도록 온도를 0으로 둔다.
	ApplyGenerationOptions(*Body, {.bStream = false, .Temperature = 0.0, .MaxTokens = MaxSectionChoiceTokens, .Schema = Schema});
	return Body;
}

// 게임에 대한 답은 고른 가이드 절과 Luna 안내 문구에서만 가져오게 하고, 답하는 언어는 현재 메뉴 언어로 고정한다.
// 가이드 절을 앞에 두고 가장 정확하다고 알려야, 작은 모델이 친절하게 "네"라고 답하며 가이드가 아니라고 한 것을 된다고 하지 않는다.
FString ULunaChatSubsystem::BuildSystemPrompt(const TArray<int32>& GuideSectionIndices) const
{
	const UMenuLocalizationSubsystem* Localization = GetGameInstance()->GetSubsystem<UMenuLocalizationSubsystem>();
	const FString Culture = Localization ? Localization->GetLanguage() : TEXT("ko");
	const FCulturePtr CultureInfo = FInternationalization::Get().GetCulture(Culture);
	const FString LanguageName = CultureInfo ? CultureInfo->GetEnglishName() : Culture;

	FString Prompt = FString::Printf(TEXT("You are Luna, the guide of the multiplayer action game \"Pandora Battle\". "
		"You appear on the title screen and talk with the player through a small speech bubble.\n"
		"Rules:\n"
		"- Always reply in %s (%s), whatever language the player writes in.\n"
		"- Keep every reply to one or two short sentences, at most about %d characters. Do not use lists, markdown or emoji.\n"
		"- Speak warmly and politely, in the first person, as Luna.\n"
		"- Answer questions about the game only from the guide sections and facts below. The guide sections are the most accurate: "
		"answer from them first and repeat their facts and formulas exactly. If they say something is not possible, say no. "
		"If nothing below covers the question, say you are not sure and suggest the Guide menu on the title screen. "
		"Never invent features.\n"
		"- For questions unrelated to the game, answer briefly and gently bring the conversation back to Pandora Battle.\n"),
		*LanguageName, *UiLanguage::NativeName(Culture), MaxReplyCharacters);

	if (!GuideSectionIndices.IsEmpty())
	{
		Prompt += TEXT("Guide sections:\n");
		for (const int32 Index : GuideSectionIndices)
		{
			Prompt += GuideSections[Index].Text + TEXT("\n");
		}
	}
	Prompt += TEXT("Other facts:\n");
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

TSharedRef<FJsonObject> ULunaChatSubsystem::BuildRequestBody(const FString& Question, const TArray<int32>& GuideSectionIndices) const
{
	const ULunaChatSettings* Settings = GetDefault<ULunaChatSettings>();
	TArray<TSharedPtr<FJsonValue>> Messages = {MakeMessage(TEXT("system"), BuildSystemPrompt(GuideSectionIndices))};
	for (const FChatMessage& Message : History)
	{
		Messages.Add(MakeMessage(Message.Role, Message.Content));
	}
	Messages.Add(MakeMessage(TEXT("user"), Question));

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetArrayField(TEXT("messages"), Messages);
	ApplyGenerationOptions(*Body, {.bStream = true, .Temperature = Settings->GetTemperature(), .MaxTokens = Settings->GetMaxReplyTokens()});
	return Body;
}

void ULunaChatSubsystem::ApplyGenerationOptions(FJsonObject& Body, const FGenerationOptions& Options) const
{
	Body.SetBoolField(TEXT("stream"), Options.bStream);
	if (UsesBundledServer())
	{
		// llama-server는 모델 하나만 띄우므로 모델 이름이 필요 없다. 스키마는 OpenAI의 response_format으로 넘긴다.
		Body.SetNumberField(TEXT("temperature"), Options.Temperature);
		Body.SetNumberField(TEXT("max_tokens"), Options.MaxTokens);
		if (Options.Schema.IsValid())
		{
			const TSharedRef<FJsonObject> JsonSchema = MakeShared<FJsonObject>();
			JsonSchema->SetStringField(TEXT("name"), TEXT("answer"));
			JsonSchema->SetObjectField(TEXT("schema"), Options.Schema);
			const TSharedRef<FJsonObject> Format = MakeShared<FJsonObject>();
			Format->SetStringField(TEXT("type"), TEXT("json_schema"));
			Format->SetObjectField(TEXT("json_schema"), JsonSchema);
			Body.SetObjectField(TEXT("response_format"), Format);
		}
		return;
	}

	const ULunaChatSettings* Settings = GetDefault<ULunaChatSettings>();
	const TSharedRef<FJsonObject> OllamaOptions = MakeShared<FJsonObject>();
	OllamaOptions->SetNumberField(TEXT("temperature"), Options.Temperature);
	OllamaOptions->SetNumberField(TEXT("num_predict"), Options.MaxTokens);
	Body.SetStringField(TEXT("model"), Settings->GetModel());
	Body.SetStringField(TEXT("keep_alive"), Settings->GetKeepAlive());
	Body.SetObjectField(TEXT("options"), OllamaOptions);
	if (Options.Schema.IsValid())
	{
		Body.SetObjectField(TEXT("format"), Options.Schema);
	}
}

// 요청이 모델 서버에 닿지 못했거나 오류 코드를 받았으면 그 이유를, 아니면 빈 문자열을 돌려준다.
FString ULunaChatSubsystem::DescribeRequestFailure(const FHttpResponsePtr& Response, const bool bConnectedSuccessfully) const
{
	const FString Server = UsesBundledServer()
		? FString(TEXT("the bundled model server"))
		: FString::Printf(TEXT("Ollama at %s"), *GetDefault<ULunaChatSettings>()->GetOllamaUrl());
	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		return FString::Printf(TEXT("Could not reach %s."), *Server);
	}
	const int32 StatusCode = Response->GetResponseCode();
	return EHttpResponseCodes::IsOk(StatusCode) ? FString() : FString::Printf(TEXT("%s returned HTTP %d."), *Server, StatusCode);
}
