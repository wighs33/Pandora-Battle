#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AudioSettingsSubsystem.generated.h"

class UAudioSettingsSaveGame;
class UWorld;

DECLARE_MULTICAST_DELEGATE_OneParam(FMasterVolumeChangedSignature, int32);

UCLASS()
class LABPROJECT_API UAudioSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Public API ------------------------------------------------------------------------------------------------------
	int32 GetMasterVolumePercent() const;

	bool IsMasterMuted() const;

	void SetMasterVolumePercent(int32 NewVolumePercent, bool bSaveImmediately = false);

	void ToggleMasterMute();

	void SaveMasterVolumeSettings();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);
	void HandleDefaultMasterVolumePreloadComplete(uint64 RequestGeneration);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void LoadMasterVolumeSettings();
	int32 ResolveDefaultMasterVolumePercent() const;
	float GetMasterVolumeMultiplier() const;
	void ApplyMasterVolumeToAudioDevice(UWorld* TargetWorld = nullptr) const;

public:
	FMasterVolumeChangedSignature OnMasterVolumeChanged;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAudioSettingsSaveGame> AudioSettingsSaveGame;

	FDelegateHandle PostLoadMapWithWorldHandle;
	int32 MasterVolumePercent = INDEX_NONE;
	int32 LastAudibleMasterVolumePercent = INDEX_NONE;
	uint64 DefaultMasterVolumeRequestGeneration = 0;
	bool bMasterVolumeSettingsDirty = false;
	bool bWaitingForDefaultMasterVolume = false;
};
