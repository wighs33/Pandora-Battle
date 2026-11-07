#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IHttpRequest.h"

/** Luna 모델 서버의 상태. 질문은 Ready에서만 받는다. */
enum class ELunaServerState : uint8
{
	/** Luna/ 폴더에 실행 파일이나 모델이 없다. DLC를 설치하지 않은 상태다. */
	NotInstalled,
	Stopped,
	/** 프로세스를 띄웠고 모델을 메모리에 올리는 중이다. */
	Starting,
	/** /health가 200을 돌려줬다. */
	Ready,
	/** 띄우지 못했거나, 시간 안에 준비되지 않았거나, 프로세스가 끝났다. */
	Failed
};

/**
 * Luna DLC에 든 로컬 모델 서버(llama.cpp의 llama-server)를 게임의 자식 프로세스로 띄우고 준비 상태를 확인한다.
 *
 * 실행 파일과 모델은 설치 폴더의 Luna/ 아래에 있고, Steam에서는 무료 DLC 데포로 받는다. 파일이 없으면 Luna 채팅을 쓸 수 없다.
 * 서버는 같은 PC의 빈 포트에만 열고, 게임이 비정상 종료돼도 함께 꺼지도록 묶는다.
 */
class FLunaLocalServer
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnStateChanged, ELunaServerState /*NewState*/);

	~FLunaLocalServer();

	/**
	 * Luna/ 폴더를 다시 찾아 서버를 띄운다. 이미 띄웠거나 준비 중이면 아무것도 하지 않는다.
	 * 파일이 없거나 프로세스를 띄우지 못하면 상태를 바꾸고 false를 돌려준다.
	 */
	bool Start();

	/** 서버 프로세스를 끝내 모델이 차지한 메모리를 돌려준다. */
	void Stop();

	ELunaServerState GetState() const { return State; }
	bool IsReady() const { return State == ELunaServerState::Ready; }
	FString GetBaseUrl() const;
	FOnStateChanged& OnStateChanged() { return StateChanged; }

private:
	/** Luna/ 폴더에서 실행 파일과 모델을 찾는다. 하나라도 없으면 false. */
	bool FindBundle();
	bool Poll(float DeltaTime);
	/** 띄우지 못했거나 준비 중에 멈춘 서버를 정리하고 실패를 알린다. */
	void Fail(const FString& Reason);
	void SetState(ELunaServerState NewState);

	FString ExecutablePath;
	FString ModelPath;
	FProcHandle Process;
	void* JobHandle = nullptr;
	int32 Port = 0;
	ELunaServerState State = ELunaServerState::NotInstalled;
	double StartTime = 0.0;
	double NextHealthCheckTime = 0.0;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HealthRequest;
	FTSTicker::FDelegateHandle TickerHandle;
	FOnStateChanged StateChanged;
};
