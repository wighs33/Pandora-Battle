#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LunaChatSettings.generated.h"

/**
 * 타이틀의 Luna가 대답할 때 쓰는 로컬 LLM 설정.
 * 모델(llama-server + GGUF)은 무료 DLC로 받아 설치 폴더의 Luna/에 놓이고, 게임이 그 서버를 직접 띄워 쓴다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Luna Chat"))
class LABPROJECT_API ULunaChatSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// Public API ------------------------------------------------------------------------------------------------------
	float GetTemperature() const { return Temperature; }
	int32 GetMaxReplyTokens() const { return MaxReplyTokens; }
	float GetRequestTimeoutSeconds() const { return RequestTimeoutSeconds; }
	int32 GetMaxHistoryTurns() const { return MaxHistoryTurns; }
	int32 GetMaxQuestionLength() const { return MaxQuestionLength; }
	const FString& GetLocalBundleFolder() const { return LocalBundleFolder; }
	const FString& GetLocalServerExecutable() const { return LocalServerExecutable; }
	const FString& GetLocalModelFile() const { return LocalModelFile; }
	int32 GetLocalContextSize() const { return LocalContextSize; }
	const FString& GetLocalServerArguments() const { return LocalServerArguments; }
	float GetLocalServerStartTimeoutSeconds() const { return LocalServerStartTimeoutSeconds; }
	int32 GetDlcAppId() const { return DlcAppId; }

private:
	/** 가이드에 적힌 내용을 그대로 전하도록 낮게 둔다. 0.6에서는 가이드가 아니라고 한 것도 된다고 답했다. */
	UPROPERTY(Config, EditAnywhere, Category = "!Reply", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Temperature = 0.2f;

	/** 말풍선에 들어갈 짧은 답만 받도록 생성 토큰 수를 제한한다. */
	UPROPERTY(Config, EditAnywhere, Category = "!Reply", meta = (ClampMin = "16"))
	int32 MaxReplyTokens = 160;

	/** 답 하나를 기다리는 최대 시간. CPU만 있는 PC는 답이 느리다. */
	UPROPERTY(Config, EditAnywhere, Category = "!Reply", meta = (ClampMin = "5.0", Units = "s"))
	float RequestTimeoutSeconds = 90.f;

	/** 문맥으로 함께 보내는 이전 질문·답변 쌍의 수 */
	UPROPERTY(Config, EditAnywhere, Category = "!Conversation", meta = (ClampMin = "0"))
	int32 MaxHistoryTurns = 4;

	UPROPERTY(Config, EditAnywhere, Category = "!Conversation", meta = (ClampMin = "1"))
	int32 MaxQuestionLength = 120;

	/** DLC가 설치되는 모델 폴더. 패키지는 설치 폴더, 개발 실행은 프로젝트 폴더 기준이다. */
	UPROPERTY(Config, EditAnywhere, Category = "!Bundled Model")
	FString LocalBundleFolder = TEXT("Luna");

	/** llama.cpp의 llama-server 실행 파일(폴더 기준 경로) */
	UPROPERTY(Config, EditAnywhere, Category = "!Bundled Model")
	FString LocalServerExecutable = TEXT("llama-server.exe");

	/** GGUF 모델 파일(폴더 기준 경로) */
	UPROPERTY(Config, EditAnywhere, Category = "!Bundled Model")
	FString LocalModelFile = TEXT("models/gemma-3-4b-it-Q4_K_M.gguf");

	/** 문맥 길이. 지정하지 않으면 모델 최대 길이로 잡혀 메모리를 수 GB 더 쓰므로, 프롬프트와 짧은 대화가 들어갈 만큼만 둔다. */
	UPROPERTY(Config, EditAnywhere, Category = "!Bundled Model", meta = (ClampMin = "1024"))
	int32 LocalContextSize = 4096;

	/** llama-server에 덧붙이는 인자(예: GPU 사용 층 수) */
	UPROPERTY(Config, EditAnywhere, Category = "!Bundled Model")
	FString LocalServerArguments;

	/** 모델을 메모리에 올리는 데 기다리는 최대 시간. 느린 디스크와 CPU만 있는 PC를 고려한다. */
	UPROPERTY(Config, EditAnywhere, Category = "!Bundled Model", meta = (ClampMin = "5.0", Units = "s"))
	float LocalServerStartTimeoutSeconds = 120.f;

	/** 모델을 담은 Steam DLC의 앱 ID. 설치하지 않은 플레이어에게 이 상점 페이지를 연다. */
	UPROPERTY(Config, EditAnywhere, Category = "!Bundled Model")
	int32 DlcAppId = 5429280;
};
