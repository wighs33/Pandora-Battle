#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LunaChatSettings.generated.h"

/**
 * 타이틀의 Luna가 대답할 때 쓰는 로컬 LLM(Ollama) 설정.
 *
 * ini에서 주소를 바꿀 때는 따옴표로 감싼다(OllamaUrl="http://..."). 따옴표 없는 '//'는 주석으로 잘린다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Luna Chat"))
class LABPROJECT_API ULunaChatSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// Public API ------------------------------------------------------------------------------------------------------
	const FString& GetOllamaUrl() const { return OllamaUrl; }
	const FString& GetModel() const { return Model; }
	float GetTemperature() const { return Temperature; }
	int32 GetMaxReplyTokens() const { return MaxReplyTokens; }
	float GetRequestTimeoutSeconds() const { return RequestTimeoutSeconds; }
	const FString& GetKeepAlive() const { return KeepAlive; }
	int32 GetMaxHistoryTurns() const { return MaxHistoryTurns; }
	int32 GetMaxQuestionLength() const { return MaxQuestionLength; }

private:
	/** Ollama 서버 주소. 기본값은 같은 PC의 기본 포트다. */
	UPROPERTY(Config, EditAnywhere, Category = "Ollama")
	FString OllamaUrl = TEXT("http://127.0.0.1:11434");

	/** `ollama pull`로 받아 둔 모델 이름. 12개 메뉴 언어로 답해야 하므로 다국어 모델을 쓴다. */
	UPROPERTY(Config, EditAnywhere, Category = "Ollama")
	FString Model = TEXT("gemma3:4b");

	UPROPERTY(Config, EditAnywhere, Category = "Ollama", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Temperature = 0.6f;

	/** 말풍선에 들어갈 짧은 답만 받도록 생성 토큰 수를 제한한다. */
	UPROPERTY(Config, EditAnywhere, Category = "Ollama", meta = (ClampMin = "16"))
	int32 MaxReplyTokens = 160;

	/** 첫 질문은 모델을 메모리에 올리느라 오래 걸릴 수 있다. */
	UPROPERTY(Config, EditAnywhere, Category = "Ollama", meta = (ClampMin = "5.0", Units = "s"))
	float RequestTimeoutSeconds = 90.f;

	/** 마지막 요청 뒤 모델을 메모리에 유지하는 시간. 이어지는 질문의 대기 시간을 줄인다. */
	UPROPERTY(Config, EditAnywhere, Category = "Ollama")
	FString KeepAlive = TEXT("15m");

	/** 문맥으로 함께 보내는 이전 질문·답변 쌍의 수 */
	UPROPERTY(Config, EditAnywhere, Category = "Conversation", meta = (ClampMin = "0"))
	int32 MaxHistoryTurns = 4;

	UPROPERTY(Config, EditAnywhere, Category = "Conversation", meta = (ClampMin = "1"))
	int32 MaxQuestionLength = 120;
};
