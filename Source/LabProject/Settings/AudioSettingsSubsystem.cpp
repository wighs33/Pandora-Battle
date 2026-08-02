#include "Settings/AudioSettingsSubsystem.h"

#include "AudioDevice.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "SavedGameData/AudioSettingsSaveGame.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AudioSettingsSubsystem)

namespace
{
	constexpr int32 MinMasterVolumePercent = 0;
	constexpr int32 MaxMasterVolumePercent = 100;

	const FString& GetAudioSettingsSaveSlotName()
	{
		static const FString AudioSettingsSaveSlotName(TEXT("AudioSettings"));
		return AudioSettingsSaveSlotName;
	}
}

void UAudioSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UGameSettingsSubsystem>();

	LoadMasterVolumeSettings();
	ApplyMasterVolumeToAudioDevice();

	if (!PostLoadMapWithWorldHandle.IsValid())
	{
		PostLoadMapWithWorldHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
			this,
			&ThisClass::HandlePostLoadMapWithWorld);
	}
}

void UAudioSettingsSubsystem::Deinitialize()
{
	bWaitingForDefaultMasterVolume = false;
	++DefaultMasterVolumeRequestGeneration;
	SaveMasterVolumeSettings();

	if (PostLoadMapWithWorldHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapWithWorldHandle);
		PostLoadMapWithWorldHandle.Reset();
	}

	Super::Deinitialize();
}

int32 UAudioSettingsSubsystem::GetMasterVolumePercent() const
{
	return MasterVolumePercent == INDEX_NONE
		? ResolveDefaultMasterVolumePercent()
		: FMath::Clamp(MasterVolumePercent, MinMasterVolumePercent, MaxMasterVolumePercent);
}

bool UAudioSettingsSubsystem::IsMasterMuted() const
{
	return GetMasterVolumePercent() == MinMasterVolumePercent;
}

void UAudioSettingsSubsystem::SetMasterVolumePercent(
	const int32 NewVolumePercent,
	const bool bSaveImmediately)
{
	// A user/runtime choice made while the default setting is loading must win.
	bWaitingForDefaultMasterVolume = false;
	++DefaultMasterVolumeRequestGeneration;

	const int32 SanitizedVolumePercent = FMath::Clamp(
		NewVolumePercent,
		MinMasterVolumePercent,
		MaxMasterVolumePercent);
	if (SanitizedVolumePercent > MinMasterVolumePercent)
	{
		LastAudibleMasterVolumePercent = SanitizedVolumePercent;
	}

	const bool bVolumeChanged = MasterVolumePercent != SanitizedVolumePercent;
	MasterVolumePercent = SanitizedVolumePercent;
	bMasterVolumeSettingsDirty = bMasterVolumeSettingsDirty || bVolumeChanged;

	ApplyMasterVolumeToAudioDevice();
	if (bVolumeChanged)
	{
		OnMasterVolumeChanged.Broadcast(MasterVolumePercent);
	}

	if (bSaveImmediately)
	{
		SaveMasterVolumeSettings();
	}
}

void UAudioSettingsSubsystem::ToggleMasterMute()
{
	if (IsMasterMuted())
	{
		int32 RestoreVolumePercent = LastAudibleMasterVolumePercent;
		if (RestoreVolumePercent <= MinMasterVolumePercent)
		{
			RestoreVolumePercent = ResolveDefaultMasterVolumePercent();
		}
		SetMasterVolumePercent(RestoreVolumePercent, true);
		return;
	}

	SetMasterVolumePercent(MinMasterVolumePercent, true);
}

void UAudioSettingsSubsystem::SaveMasterVolumeSettings()
{
	if (!bMasterVolumeSettingsDirty)
	{
		return;
	}

	if (!IsValid(AudioSettingsSaveGame))
	{
		AudioSettingsSaveGame = Cast<UAudioSettingsSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UAudioSettingsSaveGame::StaticClass()));
	}
	if (!IsValid(AudioSettingsSaveGame))
	{
		return;
	}

	AudioSettingsSaveGame->bHasMasterVolumeSetting = true;
	AudioSettingsSaveGame->MasterVolumePercent = GetMasterVolumePercent();
	AudioSettingsSaveGame->LastAudibleMasterVolumePercent = FMath::Clamp(
		LastAudibleMasterVolumePercent,
		MinMasterVolumePercent,
		MaxMasterVolumePercent);

	if (UGameplayStatics::SaveGameToSlot(AudioSettingsSaveGame, GetAudioSettingsSaveSlotName(), 0))
	{
		bMasterVolumeSettingsDirty = false;
	}
}

void UAudioSettingsSubsystem::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	ApplyMasterVolumeToAudioDevice(LoadedWorld);
}

void UAudioSettingsSubsystem::LoadMasterVolumeSettings()
{
	if (UGameplayStatics::DoesSaveGameExist(GetAudioSettingsSaveSlotName(), 0))
	{
		AudioSettingsSaveGame = Cast<UAudioSettingsSaveGame>(
			UGameplayStatics::LoadGameFromSlot(GetAudioSettingsSaveSlotName(), 0));
	}

	if (!IsValid(AudioSettingsSaveGame))
	{
		AudioSettingsSaveGame = Cast<UAudioSettingsSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UAudioSettingsSaveGame::StaticClass()));
	}

	if (IsValid(AudioSettingsSaveGame) && AudioSettingsSaveGame->bHasMasterVolumeSetting)
	{
		MasterVolumePercent = FMath::Clamp(
			AudioSettingsSaveGame->MasterVolumePercent,
			MinMasterVolumePercent,
			MaxMasterVolumePercent);
		LastAudibleMasterVolumePercent = FMath::Clamp(
			AudioSettingsSaveGame->LastAudibleMasterVolumePercent,
			MinMasterVolumePercent,
			MaxMasterVolumePercent);
	}
	else if (IsValid(AudioSettingsSaveGame) && AudioSettingsSaveGame->bHasBgmVolumeSetting)
	{
		MasterVolumePercent = FMath::Clamp(
			AudioSettingsSaveGame->BgmVolumePercent,
			MinMasterVolumePercent,
			MaxMasterVolumePercent);
		LastAudibleMasterVolumePercent = FMath::Clamp(
			AudioSettingsSaveGame->LastAudibleBgmVolumePercent,
			MinMasterVolumePercent,
			MaxMasterVolumePercent);
		bMasterVolumeSettingsDirty = true;
	}
	else
	{
		const int32 DefaultVolumePercent = ResolveDefaultMasterVolumePercent();
		MasterVolumePercent = DefaultVolumePercent;
		LastAudibleMasterVolumePercent = DefaultVolumePercent;

		UGameSettingsSubsystem* SettingsSubsystem = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UGameSettingsSubsystem>()
			: nullptr;
		if (SettingsSubsystem && !SettingsSubsystem->IsRuntimeContentReady())
		{
			bWaitingForDefaultMasterVolume = true;
			const uint64 RequestGeneration = ++DefaultMasterVolumeRequestGeneration;
			SettingsSubsystem->PreloadRuntimeContentAsync(
				FSimpleDelegate::CreateWeakLambda(
					this,
					[this, RequestGeneration]()
					{
						HandleDefaultMasterVolumePreloadComplete(RequestGeneration);
					}));
		}
		else
		{
			bMasterVolumeSettingsDirty = true;
		}
	}

	if (LastAudibleMasterVolumePercent <= MinMasterVolumePercent
		&& MasterVolumePercent > MinMasterVolumePercent)
	{
		LastAudibleMasterVolumePercent = MasterVolumePercent;
		bMasterVolumeSettingsDirty = true;
	}

	if (!bWaitingForDefaultMasterVolume)
	{
		SaveMasterVolumeSettings();
	}
}

void UAudioSettingsSubsystem::HandleDefaultMasterVolumePreloadComplete(
	const uint64 RequestGeneration)
{
	if (!bWaitingForDefaultMasterVolume
		|| RequestGeneration != DefaultMasterVolumeRequestGeneration)
	{
		return;
	}

	bWaitingForDefaultMasterVolume = false;
	const int32 PreviousVolumePercent = MasterVolumePercent;
	const int32 DefaultVolumePercent = ResolveDefaultMasterVolumePercent();
	MasterVolumePercent = DefaultVolumePercent;
	LastAudibleMasterVolumePercent = DefaultVolumePercent;
	bMasterVolumeSettingsDirty = true;

	ApplyMasterVolumeToAudioDevice();
	if (PreviousVolumePercent != MasterVolumePercent)
	{
		OnMasterVolumeChanged.Broadcast(MasterVolumePercent);
	}

	SaveMasterVolumeSettings();
}

int32 UAudioSettingsSubsystem::ResolveDefaultMasterVolumePercent() const
{
	UGameSettingsSubsystem* SettingsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UGameSettingsSubsystem>()
		: nullptr;
	const UGameSettingDefinition* SettingDefinition = SettingsSubsystem
		? SettingsSubsystem->GetLoadedGameSettingDefinition()
		: nullptr;
	return SettingDefinition
		? FMath::Clamp(
			SettingDefinition->DefaultMasterVolumePercent,
			MinMasterVolumePercent,
			MaxMasterVolumePercent)
		: MaxMasterVolumePercent;
}

float UAudioSettingsSubsystem::GetMasterVolumeMultiplier() const
{
	return static_cast<float>(GetMasterVolumePercent())
		/ static_cast<float>(MaxMasterVolumePercent);
}

void UAudioSettingsSubsystem::ApplyMasterVolumeToAudioDevice(UWorld* TargetWorld) const
{
	UWorld* World = TargetWorld;
	if (!World)
	{
		const UGameInstance* GameInstance = GetGameInstance();
		World = GameInstance ? GameInstance->GetWorld() : nullptr;
	}

	if (!World
		|| !World->IsGameWorld()
		|| World->GetGameInstance() != GetGameInstance()
		|| World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (FAudioDevice* AudioDevice = World->GetAudioDeviceRaw())
	{
		AudioDevice->SetTransientPrimaryVolume(GetMasterVolumeMultiplier());
	}
}
