#include "Mode/PdGameInstance.h"

#include "Mode/PdGameInstanceRuntimeSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdGameInstance)

void UPdGameInstance::OnStart()
{
	Super::OnStart();

	if (UPdGameInstanceRuntimeSubsystem* RuntimeSubsystem =
		GetSubsystem<UPdGameInstanceRuntimeSubsystem>())
	{
		RuntimeSubsystem->HandleGameInstanceStarted();
	}
}

void UPdGameInstance::Shutdown()
{
	if (UPdGameInstanceRuntimeSubsystem* RuntimeSubsystem =
		GetSubsystem<UPdGameInstanceRuntimeSubsystem>())
	{
		RuntimeSubsystem->HandleGameInstanceShutdown();
	}

	Super::Shutdown();
}
