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
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "!Audio|Volume")
	int32 GetMasterVolumePercent() const;

	UFUNCTION(BlueprintPure, Category = "!Audio|Volume")
	bool IsMasterMuted() const;

	UFUNCTION(BlueprintCallable, Category = "!Audio|Volume")
	void SetMasterVolumePercent(int32 NewVolumePercent, bool bSaveImmediately = false);

	UFUNCTION(BlueprintCallable, Category = "!Audio|Volume")
	void ToggleMasterMute();

	UFUNCTION(BlueprintCallable, Category = "!Audio|Volume")
	void SaveMasterVolumeSettings();

	FMasterVolumeChangedSignature OnMasterVolumeChanged;

private:
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);
	void HandleDefaultMasterVolumePreloadComplete(uint64 RequestGeneration);
	void LoadMasterVolumeSettings();
	int32 ResolveDefaultMasterVolumePercent() const;
	float GetMasterVolumeMultiplier() const;
	void ApplyMasterVolumeToAudioDevice(UWorld* TargetWorld = nullptr) const;

	UPROPERTY(Transient)
	TObjectPtr<UAudioSettingsSaveGame> AudioSettingsSaveGame;

	FDelegateHandle PostLoadMapWithWorldHandle;
	int32 MasterVolumePercent = INDEX_NONE;
	int32 LastAudibleMasterVolumePercent = INDEX_NONE;
	uint64 DefaultMasterVolumeRequestGeneration = 0;
	bool bMasterVolumeSettingsDirty = false;
	bool bWaitingForDefaultMasterVolume = false;
};
