#include "Mode/PdGameInstance.h"

#include "Audio/BgmSubsystem.h"
#include "Engine/Engine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdGameInstance)

DEFINE_LOG_CATEGORY_STATIC(LogPdGameInstance, Log, All);

namespace
{
	const FName IpNetDriverClassName(TEXT("/Script/OnlineSubsystemUtils.IpNetDriver"));
}

// 전용 서버 프로세스는 첫 맵의 Listen 전에 게임 넷 드라이버를 IP 드라이버로 바꾼다.
void UPdGameInstance::Init()
{
	Super::Init();

	if (IsRunningDedicatedServer())
	{
		UseIpNetDriverForDedicatedServer();
	}
}

// 게임 시작 시 현재 월드에 맞는 배경음을 요청한다. 이후 전환과 종료 정리는 BGM 서브시스템이 맡는다.
void UPdGameInstance::OnStart()
{
	Super::OnStart();

	if (UBgmSubsystem* BgmSubsystem = GetSubsystem<UBgmSubsystem>())
	{
		BgmSubsystem->RestoreWorldBgm();
	}
}

// Listen Server와 방 목록은 Steam 소켓을 그대로 쓰고, 전용 서버는 IP:포트로 접속받는다(GameLift 등 매치메이킹 경로).
// 클라이언트의 SteamNetDriver는 steam. 주소가 아니면 IP 연결로 동작하므로 설정은 서버 프로세스에서만 바꾼다.
void UPdGameInstance::UseIpNetDriverForDedicatedServer() const
{
	if (!GEngine)
	{
		return;
	}

	for (FNetDriverDefinition& NetDriverDefinition : GEngine->NetDriverDefinitions)
	{
		if (NetDriverDefinition.DefName == NAME_GameNetDriver
			&& NetDriverDefinition.DriverClassName != IpNetDriverClassName)
		{
			UE_LOG(LogPdGameInstance, Log, TEXT("Dedicated server uses IpNetDriver instead of %s."),
				*NetDriverDefinition.DriverClassName.ToString());
			NetDriverDefinition.DriverClassName = IpNetDriverClassName;
			NetDriverDefinition.DriverClassNameFallback = IpNetDriverClassName;
		}
	}
}
