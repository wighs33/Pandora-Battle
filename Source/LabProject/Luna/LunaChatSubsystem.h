#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Luna/LunaChatProtocol.h"
#include "Luna/LunaLocalServer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LunaChatSubsystem.generated.h"

class FContentLease;
class FJsonObject;
struct FLunaChatStreamBuffer;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLunaReplyUpdated, const FString& /*ReplySoFar*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLunaReplyFinished, bool /*bSucceeded*/, const FString& /*Reply*/);

/**
 * 타이틀의 Luna와 나누는 대화를 로컬 모델에 묻고, 답변을 토큰이 도착하는 대로 알린다.
 * 게임과 함께 배포한 모델(Luna/ 폴더의 llama-server)이 있으면 그 서버를 띄워 쓰고, 없으면 같은 PC의 Ollama에 묻는다.
 *
 * 시스템 프롬프트는 현재 메뉴 언어, 문자열 테이블 MenuText의 Luna 안내 문구, 가이드에서 질문에 맞는 절로 만든다.
 * 가이드 전체를 넣으면 질문마다 수천 토큰을 다시 처리하고 작은 모델이 내용을 섞으므로, 먼저 짧은 요청으로 절을 고른다.
 * 게임이 가진 설명만 근거로 답하게 하고, 답하는 언어도 메뉴 언어에 맞춘다. 메뉴 언어가 바뀌면 이전 대화를 비운다.
 */
UCLASS()
class LABPROJECT_API ULunaChatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Public API ------------------------------------------------------------------------------------------------------
	/** 질문을 보낸다. 앞선 답변이 진행 중이면 취소하고 새로 시작한다. 빈 질문이면 false를 돌려준다. */
	bool Ask(const FString& Question);
	void CancelReply();
	void ResetConversation();
	bool IsReplying() const { return ActiveRequest.IsValid() || bWaitingForServer; }

	/** 함께 배포한 모델이 있으면 서버를 미리 띄워 첫 답을 기다리는 시간을 줄인다. 타이틀 화면이 열릴 때 부른다. */
	void PrepareModel();

	/** 모델 서버를 끝내 메모리를 돌려준다. 타이틀 화면을 떠날 때 부른다. */
	void ReleaseModel();

	// Event Handlers --------------------------------------------------------------------------------------------------
	FOnLunaReplyUpdated& OnReplyUpdated() { return ReplyUpdated; }
	FOnLunaReplyFinished& OnReplyFinished() { return ReplyFinished; }

private:
	UFUNCTION()
	void HandleMenuLanguageChanged();

	void HandleServerStateChanged(FLunaLocalServer::EState NewState);

	void HandleGuideSectionsChosen(FHttpResponsePtr Response, bool bConnectedSuccessfully, uint32 RequestSerial);
	void DrainStream(uint32 RequestSerial);
	void HandleRequestComplete(FHttpResponsePtr Response, bool bConnectedSuccessfully, uint32 RequestSerial);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	bool UsesBundledServer() const { return LocalServer.IsValid() && LocalServer->IsInstalled(); }
	void SendQuestion(uint32 RequestSerial);

	/** 메뉴 언어의 가이드를 절로 나눠 둔다. 가이드를 불러왔을 때와 메뉴 언어가 바뀌었을 때 부른다. */
	void RebuildGuideSections();
	void RequestGuideSections(uint32 RequestSerial);
	void RequestReply(const TArray<int32>& GuideSectionIndices, uint32 RequestSerial);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateChatRequest(const TSharedRef<FJsonObject>& Body) const;
	TSharedRef<FJsonObject> BuildSectionRequestBody(const FString& Question) const;
	FString BuildSystemPrompt(const TArray<int32>& GuideSectionIndices) const;
	TSharedRef<FJsonObject> BuildRequestBody(const FString& Question, const TArray<int32>& GuideSectionIndices) const;

	struct FGenerationOptions
	{
		bool bStream = false;
		double Temperature = 0.0;
		int32 MaxTokens = 0;
		/** 있으면 답을 이 JSON 스키마 형식으로 고정한다. */
		TSharedPtr<FJsonObject> Schema;
	};
	/** 서버 종류에 맞게 생성 옵션을 붙인다. Ollama는 options·format, llama-server는 OpenAI 호환 필드를 쓴다. */
	void ApplyGenerationOptions(FJsonObject& Body, const FGenerationOptions& Options) const;
	FString DescribeRequestFailure(const FHttpResponsePtr& Response, bool bConnectedSuccessfully) const;
	void FinishReply(bool bSucceeded, const FString& Error);

	struct FChatMessage
	{
		FString Role;
		FString Content;
	};

	TArray<FChatMessage> History;
	TSharedPtr<FContentLease> GuideLease;
	TArray<LunaChat::FGuideSection> GuideSections;
	/** 절을 고르는 목록에 보여 줄 이름. 메뉴 언어 이름 옆에 영어 이름을 함께 적는다. */
	TArray<FString> GuideSectionNames;
	FString PendingQuestion;
	FString ReplyText;
	LunaChat::FStreamParser Parser;
	TSharedPtr<FLunaChatStreamBuffer, ESPMode::ThreadSafe> StreamBuffer;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;
	uint32 ActiveRequestSerial = 0;
	bool bStreamingResponse = true;
	/** 질문을 받았지만 함께 배포한 모델 서버가 아직 준비 중이다. */
	bool bWaitingForServer = false;
	TUniquePtr<FLunaLocalServer> LocalServer;

	FOnLunaReplyUpdated ReplyUpdated;
	FOnLunaReplyFinished ReplyFinished;
};
