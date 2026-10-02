#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Luna/LunaChatProtocol.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LunaChatSubsystem.generated.h"

class FJsonObject;
struct FLunaChatStreamBuffer;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLunaReplyUpdated, const FString& /*ReplySoFar*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLunaReplyFinished, bool /*bSucceeded*/, const FString& /*Reply*/);

/**
 * 타이틀의 Luna와 나누는 대화를 로컬 Ollama 모델에 묻고, 답변을 토큰이 도착하는 대로 알린다.
 *
 * 시스템 프롬프트는 현재 메뉴 언어와 DT_MenuText의 Luna 안내 문구로 만든다. 게임이 가진 설명만 근거로 답하게
 * 하고, 답하는 언어도 메뉴 언어에 맞춘다. 메뉴 언어가 바뀌면 이전 대화를 비운다.
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
	bool IsReplying() const { return ActiveRequest.IsValid(); }

	// Event Handlers --------------------------------------------------------------------------------------------------
	FOnLunaReplyUpdated& OnReplyUpdated() { return ReplyUpdated; }
	FOnLunaReplyFinished& OnReplyFinished() { return ReplyFinished; }

private:
	UFUNCTION()
	void HandleMenuLanguageChanged();

	void DrainStream(uint32 RequestSerial);
	void HandleRequestComplete(FHttpResponsePtr Response, bool bConnectedSuccessfully, uint32 RequestSerial);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	FString BuildSystemPrompt() const;
	TSharedRef<FJsonObject> BuildRequestBody(const FString& Question) const;
	void FinishReply(bool bSucceeded, const FString& Error);

	struct FChatMessage
	{
		FString Role;
		FString Content;
	};

	TArray<FChatMessage> History;
	FString PendingQuestion;
	FString ReplyText;
	LunaChat::FStreamParser Parser;
	TSharedPtr<FLunaChatStreamBuffer, ESPMode::ThreadSafe> StreamBuffer;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;
	uint32 ActiveRequestSerial = 0;
	bool bStreamingResponse = true;

	FOnLunaReplyUpdated ReplyUpdated;
	FOnLunaReplyFinished ReplyFinished;
};
