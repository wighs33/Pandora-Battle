#include "Mode/PdGameInstance.h"

#include "Settings/BgmSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdGameInstance)

// 게임 시작 시 현재 월드에 맞는 배경음을 요청한다. 이후 전환과 종료 정리는 BGM 서브시스템이 맡는다.
void UPdGameInstance::OnStart()
{
	Super::OnStart();

	if (UBgmSubsystem* BgmSubsystem = GetSubsystem<UBgmSubsystem>())
	{
		BgmSubsystem->RestoreWorldBgm();
	}
}
