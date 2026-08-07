#include "Mode/GameInstanceRuntimeSubsystem.h"

#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "SavedGameData/PlayerProfileSubsystem.h"
#include "Settings/BgmSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameInstanceRuntimeSubsystem)

namespace
{
	EWindowMode::Type ResolveWindowMode(const EStartupWindowMode WindowMode)
	{
		switch (WindowMode)
		{
		case EStartupWindowMode::Windowed:
			return EWindowMode::Windowed;
		case EStartupWindowMode::Fullscreen:
			return EWindowMode::Fullscreen;
		case EStartupWindowMode::WindowedFullscreen:
		default:
			return EWindowMode::WindowedFullscreen;
		}
	}
}

void UGameInstanceRuntimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	GameInstanceDefinition = TSoftObjectPtr<UPdGameInstanceDefinition>(
		UPdGameInstanceDefinition::GetDefaultDefinitionPath());
	LoadedGameInstanceDefinition = GameInstanceDefinition.LoadSynchronous();
	Collection.InitializeDependency<UPlayerProfileSubsystem>();
	Collection.InitializeDependency<UBgmSubsystem>();
	bShutdownHandled = false;
}

void UGameInstanceRuntimeSubsystem::Deinitialize()
{
	HandleGameInstanceShutdown();
	LoadedGameInstanceDefinition = nullptr;
	Super::Deinitialize();
}

void UGameInstanceRuntimeSubsystem::HandleGameInstanceStarted()
{
	bShutdownHandled = false;

	const UPdGameInstanceDefinition* Definition = GetGameInstanceDefinition();
	if (!Definition)
	{
		return;
	}

	ApplyConfiguredWindowMode();
	if (UBgmSubsystem* BgmSubsystem =
		GetGameInstance()->GetSubsystem<UBgmSubsystem>())
	{
		BgmSubsystem->RestoreWorldBgm();
	}
}

void UGameInstanceRuntimeSubsystem::HandleGameInstanceShutdown()
{
	if (bShutdownHandled)
	{
		return;
	}
	bShutdownHandled = true;

	if (UBgmSubsystem* BgmSubsystem =
		GetGameInstance()->GetSubsystem<UBgmSubsystem>())
	{
		BgmSubsystem->StopBgm();
	}
}

UPdGameInstanceDefinition* UGameInstanceRuntimeSubsystem::GetGameInstanceDefinition()
{
	if (LoadedGameInstanceDefinition)
	{
		return LoadedGameInstanceDefinition;
	}

	if (!GameInstanceDefinition.IsNull())
	{
		LoadedGameInstanceDefinition = GameInstanceDefinition.LoadSynchronous();
	}

	return LoadedGameInstanceDefinition
		? LoadedGameInstanceDefinition.Get()
		: GetMutableDefault<UPdGameInstanceDefinition>();
}
void UGameInstanceRuntimeSubsystem::ApplyConfiguredWindowMode() const
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	const UWorld* World = GetWorld();
#if WITH_EDITOR
	if (World && World->WorldType == EWorldType::PIE)
	{
		return;
	}
#endif

	const UPdGameInstanceDefinition* Definition = LoadedGameInstanceDefinition;
	UGameUserSettings* GameUserSettings =
		Definition && GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!GameUserSettings)
	{
		return;
	}

	const FGameInstanceLifecycleSettings& Settings =
		Definition->GetLifecycleSettings();
	GameUserSettings->SetFullscreenMode(
		ResolveWindowMode(Settings.StartupWindowMode));
	GameUserSettings->ApplySettings(false);
}
