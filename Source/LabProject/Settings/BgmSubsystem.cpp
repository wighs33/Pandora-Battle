#include "Settings/BgmSubsystem.h"

#include "Components/AudioComponent.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Settings/GameSettingsSubsystem.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BgmSubsystem)

namespace
{
	struct FResolvedBgmSettings
	{
		bool bEnabled = false;
		TSoftObjectPtr<USoundBase> Sound;
		float Volume = 1.0f;
		float Pitch = 1.0f;
		bool bPersistAcrossLevelTransition = true;
	};

	bool ResolveBgmSettings(
		const UGameSettingDefinition* SettingDefinition,
		const EBgmContext BgmContext,
		FResolvedBgmSettings& OutSettings)
	{
		if (!SettingDefinition)
		{
			return false;
		}

		switch (BgmContext)
		{
		case EBgmContext::Startup:
			OutSettings.bEnabled = SettingDefinition->bPlayStartupBgm;
			OutSettings.Sound = SettingDefinition->StartupBgm;
			OutSettings.Volume = SettingDefinition->StartupBgmVolume;
			OutSettings.Pitch = SettingDefinition->StartupBgmPitch;
			OutSettings.bPersistAcrossLevelTransition = SettingDefinition->bPersistStartupBgmAcrossLevelTransition;
			return true;
		case EBgmContext::Lobby:
			OutSettings.bEnabled = SettingDefinition->bPlayLobbyBgm;
			OutSettings.Sound = SettingDefinition->LobbyBgm;
			OutSettings.Volume = SettingDefinition->LobbyBgmVolume;
			OutSettings.Pitch = SettingDefinition->LobbyBgmPitch;
			OutSettings.bPersistAcrossLevelTransition = SettingDefinition->bPersistLobbyBgmAcrossLevelTransition;
			return true;
		case EBgmContext::RoomList:
			OutSettings.bEnabled = SettingDefinition->bPlayRoomListBgm;
			OutSettings.Sound = SettingDefinition->RoomListBgm;
			OutSettings.Volume = SettingDefinition->RoomListBgmVolume;
			OutSettings.Pitch = SettingDefinition->RoomListBgmPitch;
			OutSettings.bPersistAcrossLevelTransition = SettingDefinition->bPersistRoomListBgmAcrossLevelTransition;
			return true;
		case EBgmContext::Shop:
			OutSettings.bEnabled = SettingDefinition->bPlayShopBgm;
			OutSettings.Sound = SettingDefinition->ShopBgm;
			OutSettings.Volume = SettingDefinition->ShopBgmVolume;
			OutSettings.Pitch = SettingDefinition->ShopBgmPitch;
			OutSettings.bPersistAcrossLevelTransition = SettingDefinition->bPersistShopBgmAcrossLevelTransition;
			return true;
		case EBgmContext::TrainingRoom:
			OutSettings.bEnabled = SettingDefinition->bPlayTrainingRoomBgm;
			OutSettings.Sound = SettingDefinition->TrainingRoomBgm;
			OutSettings.Volume = SettingDefinition->TrainingRoomBgmVolume;
			OutSettings.Pitch = SettingDefinition->TrainingRoomBgmPitch;
			OutSettings.bPersistAcrossLevelTransition = SettingDefinition->bPersistTrainingRoomBgmAcrossLevelTransition;
			return true;
		case EBgmContext::Gameplay:
			OutSettings.bEnabled = SettingDefinition->bPlayGameplayBgm;
			OutSettings.Sound = SettingDefinition->GameplayBgm;
			OutSettings.Volume = SettingDefinition->GameplayBgmVolume;
			OutSettings.Pitch = SettingDefinition->GameplayBgmPitch;
			OutSettings.bPersistAcrossLevelTransition = SettingDefinition->bPersistGameplayBgmAcrossLevelTransition;
			return true;
		case EBgmContext::Guide:
			OutSettings.bEnabled = SettingDefinition->bPlayGuideBgm;
			OutSettings.Sound = SettingDefinition->GuideBgm;
			OutSettings.Volume = SettingDefinition->GuideBgmVolume;
			OutSettings.Pitch = SettingDefinition->GuideBgmPitch;
			OutSettings.bPersistAcrossLevelTransition = SettingDefinition->bPersistGuideBgmAcrossLevelTransition;
			return true;
		default:
			return false;
		}
	}
}

void UBgmSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UGameSettingsSubsystem>();

	if (!PostLoadMapWithWorldHandle.IsValid())
	{
		PostLoadMapWithWorldHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
			this,
			&ThisClass::HandlePostLoadMapWithWorld);
	}
}

void UBgmSubsystem::Deinitialize()
{
	if (PostLoadMapWithWorldHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapWithWorldHandle);
		PostLoadMapWithWorldHandle.Reset();
	}

	StopBgm();
	Super::Deinitialize();
}

void UBgmSubsystem::PlayBgmForContext(const EBgmContext BgmContext)
{
	PlayBgmForContext(BgmContext, GetWorld());
}

void UBgmSubsystem::RestoreWorldBgm()
{
	PlayBgmForContext(ResolveWorldBgmContext(GetWorld()), GetWorld());
}

void UBgmSubsystem::StopBgm()
{
	CancelPendingBgmLoads();
	StopActiveBgmAudio();
}

void UBgmSubsystem::StopActiveBgmAudio()
{
	if (IsValid(StartupBgmAudioComponent))
	{
		StartupBgmAudioComponent->OnAudioFinished.RemoveDynamic(this, &ThisClass::HandleBgmAudioFinished);
		StartupBgmAudioComponent->Stop();
		StartupBgmAudioComponent = nullptr;
	}
	ActiveBgmSoundPath.Reset();
	ActiveBgmBaseVolume = 1.0f;
}

void UBgmSubsystem::PlayBgmForContext(const EBgmContext BgmContext, UWorld* World)
{
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	CancelPendingBgmLoads();
	const uint64 LoadGeneration = BgmLoadGeneration;

	UGameSettingsSubsystem* SettingsSubsystem =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UGameSettingsSubsystem>()
			: nullptr;
	if (!SettingsSubsystem)
	{
		return;
	}

	const TWeakObjectPtr<UWorld> WeakWorld(World);
	bSettingsLoadPending = true;
	SettingsSubsystem->PreloadRuntimeContentAsync(
		FSimpleDelegate::CreateWeakLambda(
			this,
			[this, LoadGeneration, BgmContext, WeakWorld]()
			{
				if (LoadGeneration != BgmLoadGeneration)
				{
					return;
				}

				bSettingsLoadPending = false;
				BeginBgmSoundPreload(
					LoadGeneration,
					BgmContext,
					WeakWorld);
			}));
}

void UBgmSubsystem::BeginBgmSoundPreload(
	const uint64 LoadGeneration,
	const EBgmContext BgmContext,
	const TWeakObjectPtr<UWorld> World)
{
	if (LoadGeneration != BgmLoadGeneration)
	{
		return;
	}

	UWorld* ResolvedWorld = World.Get();
	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveLoadedGameSettingDefinition(ResolvedWorld);
	if (!ResolvedWorld || !SettingDefinition)
	{
		return;
	}

	FResolvedBgmSettings BgmSettings;
	if (!ResolveBgmSettings(SettingDefinition, BgmContext, BgmSettings))
	{
		return;
	}

	if (!BgmSettings.bEnabled)
	{
		StopActiveBgmAudio();
		ActiveBgmContext = BgmContext;
		ActiveBgmSoundPath.Reset();
		return;
	}

	if (BgmSettings.Sound.IsNull())
	{
		StopActiveBgmAudio();
		return;
	}

	const FSoftObjectPath RequestedSoundPath = BgmSettings.Sound.ToSoftObjectPath();
	if (IsValid(StartupBgmAudioComponent)
		&& StartupBgmAudioComponent->IsPlaying()
		&& ActiveBgmContext == BgmContext
		&& ActiveBgmSoundPath == RequestedSoundPath)
	{
		ActiveBgmBaseVolume = FMath::Max(BgmSettings.Volume, 0.0f);
		StartupBgmAudioComponent->SetVolumeMultiplier(ActiveBgmBaseVolume);
		return;
	}

	if (BgmSettings.Sound.Get())
	{
		CompleteBgmSoundPreload(LoadGeneration, BgmContext, World);
		return;
	}

	UContentDataSubsystem* ContentSubsystem =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UContentDataSubsystem>()
			: nullptr;
	if (!ContentSubsystem)
	{
		return;
	}

	bSoundLoadPending = true;
	TSharedPtr<FStreamableHandle> SoundLoadHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
			{RequestedSoundPath},
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this, LoadGeneration, BgmContext, World]()
				{
					CompleteBgmSoundPreload(
						LoadGeneration,
						BgmContext,
						World);
				}));

	if (LoadGeneration == BgmLoadGeneration && bSoundLoadPending)
	{
		PendingSoundLoadHandle = MoveTemp(SoundLoadHandle);
	}
	else if (SoundLoadHandle.IsValid())
	{
		SoundLoadHandle->ReleaseHandle();
	}
}

void UBgmSubsystem::CompleteBgmSoundPreload(
	const uint64 LoadGeneration,
	const EBgmContext BgmContext,
	const TWeakObjectPtr<UWorld> World)
{
	if (LoadGeneration != BgmLoadGeneration)
	{
		return;
	}

	bSoundLoadPending = false;
	TSharedPtr<FStreamableHandle> CompletedHandle =
		MoveTemp(PendingSoundLoadHandle);
	PendingSoundLoadHandle.Reset();

	UWorld* ResolvedWorld = World.Get();
	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveLoadedGameSettingDefinition(ResolvedWorld);
	FResolvedBgmSettings BgmSettings;
	if (!ResolvedWorld
		|| !ResolveBgmSettings(SettingDefinition, BgmContext, BgmSettings))
	{
		if (CompletedHandle.IsValid())
		{
			CompletedHandle->ReleaseHandle();
		}
		return;
	}

	USoundBase* LoadedBgm = BgmSettings.Sound.Get();
	if (!LoadedBgm)
	{
		if (CompletedHandle.IsValid())
		{
			CompletedHandle->ReleaseHandle();
		}
		return;
	}

	StopActiveBgmAudio();
	ActiveBgmBaseVolume = FMath::Max(BgmSettings.Volume, 0.0f);
	StartupBgmAudioComponent = UGameplayStatics::SpawnSound2D(
		ResolvedWorld,
		LoadedBgm,
		ActiveBgmBaseVolume,
		BgmSettings.Pitch,
		0.0f,
		nullptr,
		BgmSettings.bPersistAcrossLevelTransition,
		true);
	if (IsValid(StartupBgmAudioComponent))
	{
		StartupBgmAudioComponent->OnAudioFinished.AddUniqueDynamic(this, &ThisClass::HandleBgmAudioFinished);
	}
	ActiveBgmContext = BgmContext;
	ActiveBgmSoundPath = BgmSettings.Sound.ToSoftObjectPath();

	if (CompletedHandle.IsValid())
	{
		CompletedHandle->ReleaseHandle();
	}
}

void UBgmSubsystem::CancelPendingBgmLoads()
{
	++BgmLoadGeneration;
	bSettingsLoadPending = false;
	bSoundLoadPending = false;

	auto CancelHandle = [](TSharedPtr<FStreamableHandle>& Handle)
	{
		if (Handle.IsValid())
		{
			Handle->CancelHandle();
			Handle->ReleaseHandle();
			Handle.Reset();
		}
	};

	CancelHandle(PendingSoundLoadHandle);
}

EBgmContext UBgmSubsystem::ResolveWorldBgmContext(UWorld* World) const
{
	if (!World || !World->IsGameWorld())
	{
		return EBgmContext::Startup;
	}

	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(World, true);
	if (CurrentLevelName.Contains(TEXT("Title"), ESearchCase::IgnoreCase))
	{
		return EBgmContext::Startup;
	}

	if (CurrentLevelName.Contains(TEXT("Training"), ESearchCase::IgnoreCase))
	{
		return EBgmContext::TrainingRoom;
	}

	if (CurrentLevelName.Contains(TEXT("Lobby"), ESearchCase::IgnoreCase))
	{
		return EBgmContext::Lobby;
	}

	if (CurrentLevelName.Contains(TEXT("Room"), ESearchCase::IgnoreCase))
	{
		return EBgmContext::RoomList;
	}

	const ULobbyRuntimeSubsystem* LobbyRuntimeSubsystem =
		GetGameInstance()->GetSubsystem<ULobbyRuntimeSubsystem>();
	const FName SelectedMapKey = LobbyRuntimeSubsystem
		? LobbyRuntimeSubsystem->GetLobbySelectedMapKey()
		: NAME_None;
	if (SelectedMapKey.ToString().Contains(TEXT("Training"), ESearchCase::IgnoreCase))
	{
		return EBgmContext::TrainingRoom;
	}

	return EBgmContext::Gameplay;
}

void UBgmSubsystem::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	PlayBgmForContext(ResolveWorldBgmContext(LoadedWorld), LoadedWorld);
}

void UBgmSubsystem::HandleBgmAudioFinished()
{
	if (ActiveBgmSoundPath.IsNull())
	{
		return;
	}

	StartupBgmAudioComponent = nullptr;
	PlayBgmForContext(ActiveBgmContext, GetWorld());
}
