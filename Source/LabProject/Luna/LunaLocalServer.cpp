#include "Luna/LunaLocalServer.h"

#include "HAL/FileManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Luna/LunaChatSettings.h"
#include "Misc/Paths.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLunaServer, Log, All);

namespace
{
	/** 준비 확인 간격. 모델을 올리는 동안 /health는 503을 돌려준다. */
	constexpr double HealthCheckInterval = 0.5;

	/** 같은 PC에서만 접속할 수 있는 빈 포트를 운영체제에 받아 온다. */
	int32 FindFreeLoopbackPort()
	{
		ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		FSocket* Socket = Sockets ? Sockets->CreateSocket(NAME_Stream, TEXT("LunaPortProbe"), false) : nullptr;
		if (!Socket)
		{
			return 0;
		}
		const TSharedRef<FInternetAddr> Address = Sockets->CreateInternetAddr();
		bool bValidIp = false;
		Address->SetIp(TEXT("127.0.0.1"), bValidIp);
		Address->SetPort(0);
		const int32 Port = bValidIp && Socket->Bind(*Address) ? Socket->GetPortNo() : 0;
		Socket->Close();
		Sockets->DestroySocket(Socket);
		return Port;
	}
}

FLunaLocalServer::FLunaLocalServer()
{
	FindBundle();
	State = IsInstalled() ? EState::Stopped : EState::NotInstalled;
}

FLunaLocalServer::~FLunaLocalServer()
{
	Stop();
}

// 패키지 빌드는 설치 폴더(RootDir) 아래, 에디터와 개발 실행은 프로젝트 폴더 아래의 Luna/를 찾는다.
void FLunaLocalServer::FindBundle()
{
	const ULunaChatSettings* Settings = GetDefault<ULunaChatSettings>();
	for (const FString& Base : {FPaths::RootDir(), FPaths::ProjectDir()})
	{
		const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::Combine(Base, Settings->GetLocalBundleFolder()));
		const FString Executable = FPaths::Combine(Folder, Settings->GetLocalServerExecutable());
		const FString Model = FPaths::Combine(Folder, Settings->GetLocalModelFile());
		if (IFileManager::Get().FileExists(*Executable) && IFileManager::Get().FileExists(*Model))
		{
			ExecutablePath = Executable;
			ModelPath = Model;
			UE_LOG(LogLunaServer, Log, TEXT("Bundled Luna model found in %s"), *Folder);
			return;
		}
	}
}

bool FLunaLocalServer::Start()
{
	if (!IsInstalled())
	{
		return false;
	}
	if (State == EState::Starting || State == EState::Ready)
	{
		return true;
	}

	Port = FindFreeLoopbackPort();
	if (Port == 0)
	{
		UE_LOG(LogLunaServer, Warning, TEXT("No free local port for the Luna model server."));
		SetState(EState::Failed);
		return false;
	}

	const ULunaChatSettings* Settings = GetDefault<ULunaChatSettings>();
	const FString LogFile = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectLogDir(), TEXT("LunaServer.log")));
	const FString Arguments = FString::Printf(TEXT("-m \"%s\" --host 127.0.0.1 --port %d -c %d -np 1 --log-file \"%s\" %s"),
		*ModelPath, Port, Settings->GetLocalContextSize(), *LogFile, *Settings->GetLocalServerArguments());
	Process = FPlatformProcess::CreateProc(*ExecutablePath, *Arguments, false, true, true, nullptr, 0,
		*FPaths::GetPath(ExecutablePath), nullptr);
	if (!Process.IsValid())
	{
		UE_LOG(LogLunaServer, Warning, TEXT("Could not start %s"), *ExecutablePath);
		SetState(EState::Failed);
		return false;
	}

#if PLATFORM_WINDOWS
	// 게임이 비정상 종료돼도 모델 서버가 메모리를 쥔 채 남지 않도록, 게임 프로세스가 닫히면 함께 끝나는 작업 개체에 넣는다.
	// 넣지 못해도 서버는 쓸 수 있고 정상 종료 때는 Stop이 끝내므로, 경고만 남긴다.
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION Limits = {};
	Limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	HANDLE Job = CreateJobObjectW(nullptr, nullptr);
	if (Job && SetInformationJobObject(Job, JobObjectExtendedLimitInformation, &Limits, sizeof(Limits))
		&& AssignProcessToJobObject(Job, Process.Get()))
	{
		JobHandle = Job;
	}
	else
	{
		UE_LOG(LogLunaServer, Warning, TEXT("Could not tie the Luna model server to the game process (error %u). It may outlive a crash."),
			static_cast<uint32>(GetLastError()));
		if (Job)
		{
			CloseHandle(Job);
		}
	}
#endif

	UE_LOG(LogLunaServer, Log, TEXT("Starting the Luna model server on port %d"), Port);
	StartTime = FPlatformTime::Seconds();
	NextHealthCheckTime = StartTime + HealthCheckInterval;
	SetState(EState::Starting);
	if (!TickerHandle.IsValid())
	{
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FLunaLocalServer::Poll));
	}
	return true;
}

void FLunaLocalServer::Stop()
{
	FTSTicker::RemoveTicker(TickerHandle);
	TickerHandle.Reset();
	if (HealthRequest.IsValid())
	{
		HealthRequest->OnProcessRequestComplete().Unbind();
		HealthRequest->CancelRequest();
		HealthRequest.Reset();
	}
	if (Process.IsValid())
	{
		if (FPlatformProcess::IsProcRunning(Process))
		{
			FPlatformProcess::TerminateProc(Process, true);
		}
		FPlatformProcess::CloseProc(Process);
		UE_LOG(LogLunaServer, Log, TEXT("Stopped the Luna model server"));
	}
#if PLATFORM_WINDOWS
	if (JobHandle)
	{
		CloseHandle(static_cast<HANDLE>(JobHandle));
		JobHandle = nullptr;
	}
#endif
	if (IsInstalled())
	{
		SetState(EState::Stopped);
	}
}

FString FLunaLocalServer::GetBaseUrl() const
{
	return FString::Printf(TEXT("http://127.0.0.1:%d"), Port);
}

// 프로세스가 살아 있는지 보고, 준비 중이면 /health가 200을 줄 때까지 묻는다.
bool FLunaLocalServer::Poll(float)
{
	if (!FPlatformProcess::IsProcRunning(Process))
	{
		int32 ReturnCode = 0;
		FPlatformProcess::GetProcReturnCode(Process, &ReturnCode);
		UE_LOG(LogLunaServer, Warning, TEXT("The Luna model server exited (code %d). See Saved/Logs/LunaServer.log."), ReturnCode);
		TickerHandle.Reset();
		Stop();
		SetState(EState::Failed);
		return false;
	}
	if (State != EState::Starting)
	{
		return true;
	}

	const double Now = FPlatformTime::Seconds();
	if (Now - StartTime > GetDefault<ULunaChatSettings>()->GetLocalServerStartTimeoutSeconds())
	{
		UE_LOG(LogLunaServer, Warning, TEXT("The Luna model server did not become ready in time."));
		TickerHandle.Reset();
		Stop();
		SetState(EState::Failed);
		return false;
	}
	if (HealthRequest.IsValid() || Now < NextHealthCheckTime)
	{
		return true;
	}

	HealthRequest = FHttpModule::Get().CreateRequest();
	HealthRequest->SetVerb(TEXT("GET"));
	HealthRequest->SetURL(GetBaseUrl() + TEXT("/health"));
	HealthRequest->SetTimeout(2.f);
	HealthRequest->OnProcessRequestComplete().BindLambda(
		[this](FHttpRequestPtr, const FHttpResponsePtr Response, const bool bConnectedSuccessfully)
		{
			HealthRequest.Reset();
			NextHealthCheckTime = FPlatformTime::Seconds() + HealthCheckInterval;
			if (State == EState::Starting && bConnectedSuccessfully && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
			{
				UE_LOG(LogLunaServer, Log, TEXT("The Luna model server is ready (%.1fs)"), FPlatformTime::Seconds() - StartTime);
				SetState(EState::Ready);
			}
		});
	HealthRequest->ProcessRequest();
	return true;
}

void FLunaLocalServer::SetState(const EState NewState)
{
	if (State != NewState)
	{
		State = NewState;
		StateChanged.Broadcast(NewState);
	}
}
