#include "Mode/PdGameInstanceRuntimeSubsystem.h"

#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "SavedGameData/PlayerProfileSubsystem.h"
#include "Settings/BgmSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdGameInstanceRuntimeSubsystem)

namespace
{
	EWindowMode::Type ResolveWindowMode(const EPdStartupWindowMode WindowMode)
	{
		switch (WindowMode)
		{
		case EPdStartupWindowMode::Windowed:
			return EWindowMode::Windowed;
		case EPdStartupWindowMode::Fullscreen:
			return EWindowMode::Fullscreen;
		case EPdStartupWindowMode::WindowedFullscreen:
		default:
			return EWindowMode::WindowedFullscreen;
		}
	}
}

UPdGameInstanceRuntimeSubsystem::UPdGameInstanceRuntimeSubsystem()
	: GameInstanceDefinition(UPdGameInstanceDefinition::GetDefaultDefinitionPath())
{
}

void UPdGameInstanceRuntimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UPlayerProfileSubsystem>();
	Collection.InitializeDependency<UBgmSubsystem>();
	bShutdownHandled = false;
	ApplyRuntimeSettings();
	BeginGameInstanceDefinitionPreload();
}

void UPdGameInstanceRuntimeSubsystem::Deinitialize()
{
	HandleGameInstanceShutdown();
	ReleaseGameInstanceDefinitionPreload();
	bGameInstanceStarted = false;
	LoadedGameInstanceDefinition = nullptr;
	Super::Deinitialize();
}

void UPdGameInstanceRuntimeSubsystem::HandleGameInstanceStarted()
{
	bGameInstanceStarted = true;
	bShutdownHandled = false;

	const UPdGameInstanceDefinition* Definition = GetGameInstanceDefinition();
	if (!Definition)
	{
		return;
	}

	const FPdGameInstanceLifecycleSettings& Settings =
		Definition->GetLifecycleSettings();
	if (Settings.bDisableCollisionVisualizationOutsideEditor)
	{
		DisablePackagedCollisionVisualization();
	}
	if (Settings.bApplyWindowModeOnStart)
	{
		ApplyConfiguredWindowMode();
	}
	if (Settings.bRestoreWorldBgmOnStart)
	{
		if (UBgmSubsystem* BgmSubsystem =
			GetGameInstance()->GetSubsystem<UBgmSubsystem>())
		{
			BgmSubsystem->RestoreWorldBgm();
		}
	}
}

void UPdGameInstanceRuntimeSubsystem::HandleGameInstanceShutdown()
{
	if (bShutdownHandled)
	{
		return;
	}
	bShutdownHandled = true;

	const UPdGameInstanceDefinition* Definition = GetGameInstanceDefinition();
	if (Definition && Definition->GetLifecycleSettings().bStopBgmOnShutdown)
	{
		if (UBgmSubsystem* BgmSubsystem =
			GetGameInstance()->GetSubsystem<UBgmSubsystem>())
		{
			BgmSubsystem->StopBgm();
		}
	}
}

UPdGameInstanceDefinition* UPdGameInstanceRuntimeSubsystem::GetGameInstanceDefinition()
{
	if (LoadedGameInstanceDefinition)
	{
		return LoadedGameInstanceDefinition;
	}

	if (!GameInstanceDefinition.IsNull())
	{
		LoadedGameInstanceDefinition = GameInstanceDefinition.Get();
	}

	return LoadedGameInstanceDefinition
		? LoadedGameInstanceDefinition.Get()
		: GetMutableDefault<UPdGameInstanceDefinition>();
}

void UPdGameInstanceRuntimeSubsystem::BeginGameInstanceDefinitionPreload()
{
	ReleaseGameInstanceDefinitionPreload();

	if (GameInstanceDefinition.IsNull())
	{
		return;
	}
	if (UPdGameInstanceDefinition* LoadedDefinition = GameInstanceDefinition.Get())
	{
		LoadedGameInstanceDefinition = LoadedDefinition;
		ApplyRuntimeSettings();
		return;
	}

	const uint32 RequestGeneration = GameInstanceDefinitionLoadGeneration;
	TSharedPtr<FStreamableHandle> NewLoadHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			GameInstanceDefinition.ToSoftObjectPath(),
			FStreamableDelegate::CreateWeakLambda(
				this,
				[this, RequestGeneration]()
				{
					HandleGameInstanceDefinitionPreloaded(RequestGeneration);
				}));

	if (NewLoadHandle.IsValid()
		&& RequestGeneration == GameInstanceDefinitionLoadGeneration
		&& !LoadedGameInstanceDefinition)
	{
		GameInstanceDefinitionLoadHandle = MoveTemp(NewLoadHandle);
	}
	else if (NewLoadHandle.IsValid())
	{
		NewLoadHandle->ReleaseHandle();
	}
}

void UPdGameInstanceRuntimeSubsystem::HandleGameInstanceDefinitionPreloaded(
	const uint32 RequestGeneration)
{
	if (RequestGeneration != GameInstanceDefinitionLoadGeneration)
	{
		return;
	}

	LoadedGameInstanceDefinition = GameInstanceDefinition.Get();
	if (!LoadedGameInstanceDefinition)
	{
		return;
	}

	ApplyRuntimeSettings();
	if (bGameInstanceStarted)
	{
		HandleGameInstanceStarted();
	}
}

void UPdGameInstanceRuntimeSubsystem::ReleaseGameInstanceDefinitionPreload()
{
	++GameInstanceDefinitionLoadGeneration;
	if (GameInstanceDefinitionLoadHandle.IsValid())
	{
		GameInstanceDefinitionLoadHandle->CancelHandle();
		GameInstanceDefinitionLoadHandle->ReleaseHandle();
		GameInstanceDefinitionLoadHandle.Reset();
	}
}

void UPdGameInstanceRuntimeSubsystem::ApplyRuntimeSettings()
{
	const UPdGameInstanceDefinition* Definition = GetGameInstanceDefinition();
	if (!Definition)
	{
		return;
	}

	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>())
	{
		ProfileSubsystem->ApplySettings(
			Definition->GetProfilePersistenceSettings());
	}
}

void UPdGameInstanceRuntimeSubsystem::ApplyConfiguredWindowMode() const
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

	const FPdGameInstanceLifecycleSettings& Settings =
		Definition->GetLifecycleSettings();
	GameUserSettings->SetFullscreenMode(
		ResolveWindowMode(Settings.StartupWindowMode));
	GameUserSettings->ApplySettings(false);
	if (Settings.bSaveAppliedWindowMode)
	{
		GameUserSettings->SaveSettings();
	}
}

void UPdGameInstanceRuntimeSubsystem::DisablePackagedCollisionVisualization() const
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

#if WITH_EDITOR
	if (const UWorld* World = GetWorld(); World && World->WorldType == EWorldType::PIE)
	{
		return;
	}
#endif

	UGameViewportClient* GameViewport = GEngine ? GEngine->GameViewport : nullptr;
	if (GameViewport && GameViewport->EngineShowFlags.Collision)
	{
		GameViewport->EngineShowFlags.SetCollision(false);
	}
}
