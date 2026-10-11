#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IHttpRequest.h"

/**
 * 게임과 함께 배포하는 로컬 모델 서버(llama.cpp의 llama-server)를 자식 프로세스로 띄우고 준비 상태를 확인한다.
 *
 * 실행 파일과 모델은 설치 폴더의 Luna/ 아래에 있다(Steam에서는 무료 DLC 데포로 받는다). 파일이 없으면 설치되지 않은 것으로 보고
 * Luna 채팅은 예전처럼 Ollama 설정을 쓴다. 서버는 같은 PC의 빈 포트에만 열고, 게임이 비정상 종료돼도 함께 꺼지도록 묶는다.
 */
class FLunaLocalServer
{
public:
	enum class EState : uint8
	{
		NotInstalled,
		Stopped,
		Starting,
		Ready,
		Failed
	};

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnStateChanged, EState /*NewState*/);

	FLunaLocalServer();
	~FLunaLocalServer();

	/** Luna/ 폴더에서 실행 파일과 모델을 찾는다. 없으면 false. */
	bool IsInstalled() const { return !ExecutablePath.IsEmpty(); }

	/** 서버를 띄운다. 이미 띄웠거나 준비 중이면 아무것도 하지 않는다. 설치되지 않았으면 false. */
	bool Start();

	/** 서버 프로세스를 끝내 모델이 차지한 메모리를 돌려준다. */
	void Stop();

	EState GetState() const { return State; }
	bool IsReady() const { return State == EState::Ready; }
	FString GetBaseUrl() const;
	FOnStateChanged& OnStateChanged() { return StateChanged; }

private:
	void FindBundle();
	bool Poll(float DeltaTime);
	void SetState(EState NewState);

	FString ExecutablePath;
	FString ModelPath;
	FProcHandle Process;
	void* JobHandle = nullptr;
	int32 Port = 0;
	EState State = EState::NotInstalled;
	double StartTime = 0.0;
	double NextHealthCheckTime = 0.0;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HealthRequest;
	FTSTicker::FDelegateHandle TickerHandle;
	FOnStateChanged StateChanged;
};
