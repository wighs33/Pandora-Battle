#include "Mode/PdGameInstance.h"

#include "Mode/GameInstanceRuntimeSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdGameInstance)

void UPdGameInstance::OnStart()
{
	Super::OnStart();

	if (UGameInstanceRuntimeSubsystem* RuntimeSubsystem =
		GetSubsystem<UGameInstanceRuntimeSubsystem>())
	{
		RuntimeSubsystem->HandleGameInstanceStarted();
	}
}

void UPdGameInstance::Shutdown()
{
	if (UGameInstanceRuntimeSubsystem* RuntimeSubsystem =
		GetSubsystem<UGameInstanceRuntimeSubsystem>())
	{
		RuntimeSubsystem->HandleGameInstanceShutdown();
	}

	Super::Shutdown();
}
