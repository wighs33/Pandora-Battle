#include "Audio/BgmSubsystem.h"

#include "Components/AudioComponent.h"
#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
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

	bool ResolveBgmSettings(const UGameSettingDefinition* SettingDefinition, const EBgmContext BgmContext,
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
		PostLoadMapWithWorldHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this,
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
	if (IsValid(ActiveBgmAudioComponent))
	{
		ActiveBgmAudioComponent->OnAudioFinished.RemoveDynamic(this, &ThisClass::HandleBgmAudioFinished);
		ActiveBgmAudioComponent->Stop();
		ActiveBgmAudioComponent = nullptr;
	}
	ActiveBgmSoundPath.Reset();
}

void UBgmSubsystem::PlayBgmForContext(const EBgmContext BgmContext, UWorld* World)
{
	if (!World || !World->IsGameWorld() || World->GetGameInstance() != GetGameInstance()
		|| World->GetNetMode() == NM_DedicatedServer)
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
	SettingsSubsystem->PreloadRuntimeContentAsync(
		FSimpleDelegate::CreateWeakLambda(this, [this, LoadGeneration, BgmContext, WeakWorld]()
			{
				if (LoadGeneration != BgmLoadGeneration)
				{
					return;
				}

				BeginBgmSoundPreload(LoadGeneration, BgmContext, WeakWorld);
			}));
}

void UBgmSubsystem::BeginBgmSoundPreload(const uint64 LoadGeneration, const EBgmContext BgmContext,
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
	if (IsValid(ActiveBgmAudioComponent) && ActiveBgmAudioComponent->IsPlaying() && ActiveBgmContext == BgmContext
		&& ActiveBgmSoundPath == RequestedSoundPath)
	{
		const float BgmBaseVolume = FMath::Max(BgmSettings.Volume, 0.0f);
		ActiveBgmAudioComponent->SetVolumeMultiplier(BgmBaseVolume);
		return;
	}

	if (BgmSettings.Sound.Get())
	{
		CompleteBgmSoundPreload(BgmContext, World);
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

	SoundLease = ContentSubsystem->AcquireContent({RequestedSoundPath},
		FSimpleDelegate::CreateWeakLambda(this, [this, BgmContext, World]()
			{
				CompleteBgmSoundPreload(BgmContext, World);
			}));
}

void UBgmSubsystem::CompleteBgmSoundPreload(const EBgmContext BgmContext, const TWeakObjectPtr<UWorld> World)
{
	// 새 오디오 컴포넌트가 사운드를 참조한 뒤에 로드를 놓도록 함수가 끝날 때 해제한다.
	const TSharedPtr<FContentLease> CompletedLease = MoveTemp(SoundLease);

	UWorld* ResolvedWorld = World.Get();
	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveLoadedGameSettingDefinition(ResolvedWorld);
	FResolvedBgmSettings BgmSettings;
	if (!ResolvedWorld || !ResolveBgmSettings(SettingDefinition, BgmContext, BgmSettings))
	{
		return;
	}

	USoundBase* LoadedBgm = BgmSettings.Sound.Get();
	if (!LoadedBgm)
	{
		return;
	}

	StopActiveBgmAudio();
	const float BgmBaseVolume = FMath::Max(BgmSettings.Volume, 0.0f);
	ActiveBgmAudioComponent = UGameplayStatics::SpawnSound2D(ResolvedWorld, LoadedBgm, BgmBaseVolume, BgmSettings.Pitch,
		0.0f, nullptr, BgmSettings.bPersistAcrossLevelTransition, true);
	if (IsValid(ActiveBgmAudioComponent))
	{
		ActiveBgmAudioComponent->OnAudioFinished.AddUniqueDynamic(this, &ThisClass::HandleBgmAudioFinished);
	}
	ActiveBgmContext = BgmContext;
	ActiveBgmSoundPath = BgmSettings.Sound.ToSoftObjectPath();
}

void UBgmSubsystem::CancelPendingBgmLoads()
{
	++BgmLoadGeneration;
	SoundLease.Reset();
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

	if (SelectedMatchMapKey.ToString().Contains(TEXT("Training"), ESearchCase::IgnoreCase))
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

	ActiveBgmAudioComponent = nullptr;
	PlayBgmForContext(ActiveBgmContext, GetWorld());
}
