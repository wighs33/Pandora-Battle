#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AudioVolumeControl.generated.h"

class UAudioSettingsSubsystem;
class UAudioVolumeSlider;
class UButton;
class UUserWidget;

UCLASS()
class LABPROJECT_API UAudioVolumeControl : public UObject
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	void Initialize(UUserWidget* InOwnerWidget, UAudioVolumeSlider* InVolumeSlider, UButton* InSoundButton);
	void Shutdown();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandleSliderValueChanged(float NormalizedValue);

	UFUNCTION()
	void HandleSoundButtonClicked();

	void HandleMasterVolumeChanged(int32 VolumePercent);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void BeginSoundButtonTexturePreload();
	void ReleaseSoundButtonTexturePreload();
	void SynchronizeFromSubsystem();
	void SetSliderValueFromPercent(int32 VolumePercent);
	void RefreshSoundButtonStyle(bool bMuted) const;
	UAudioSettingsSubsystem* GetAudioSettingsSubsystem() const;

private:
	TWeakObjectPtr<UUserWidget> OwnerWidget;

	UPROPERTY(Transient)
	TObjectPtr<UAudioVolumeSlider> VolumeSlider;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SoundButton;

	FDelegateHandle MasterVolumeChangedHandle;
	int32 TexturePreloadGeneration = 0;
	bool bSynchronizing = false;
};
