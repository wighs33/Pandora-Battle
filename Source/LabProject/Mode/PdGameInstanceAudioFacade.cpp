#include "Mode/PdGameInstance.h"

#include "Settings/BgmSubsystem.h"

void UPdGameInstance::PlayBgmForContext(const EBgmContext BgmContext)
{
	if (UBgmSubsystem* BgmSubsystem = GetSubsystem<UBgmSubsystem>())
	{
		BgmSubsystem->PlayBgmForContext(BgmContext);
	}
}

void UPdGameInstance::RestoreWorldBgm()
{
	if (UBgmSubsystem* BgmSubsystem = GetSubsystem<UBgmSubsystem>())
	{
		BgmSubsystem->RestoreWorldBgm();
	}
}

void UPdGameInstance::StopBgm()
{
	if (UBgmSubsystem* BgmSubsystem = GetSubsystem<UBgmSubsystem>())
	{
		BgmSubsystem->StopBgm();
	}
}
