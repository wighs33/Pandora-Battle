#include "UI/Widget/AudioVolumeControl.h"

#include "AudioSlider.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Settings/AudioSettingsSubsystem.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Settings/GameSettingsSubsystem.h"
#include "Widgets/SWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AudioVolumeControl)

namespace
{
	constexpr int32 AudioVolumeControlMaxMasterVolumePercent = 100;

	void SetBrushTexture(FSlateBrush& Brush, UTexture2D* Texture)
	{
		Brush.SetResourceObject(Texture);
	}

	void HideAudioSliderTextLabel(const TSharedRef<SWidget>& Widget)
	{
		if (Widget->GetType() == TEXT("SAudioTextBox"))
		{
			Widget->SetVisibility(EVisibility::Collapsed);
			return;
		}

		FChildren* Children = Widget->GetChildren();
		for (int32 ChildIndex = 0; Children && ChildIndex < Children->Num(); ++ChildIndex)
		{
			HideAudioSliderTextLabel(Children->GetChildAt(ChildIndex));
		}
	}
}

void UAudioVolumeControl::Initialize(
	UUserWidget* InOwnerWidget,
	UAudioVolumeSlider* InVolumeSlider,
	UButton* InSoundButton)
{
	Shutdown();

	OwnerWidget = InOwnerWidget;
	VolumeSlider = InVolumeSlider;
	SoundButton = InSoundButton;

	if (VolumeSlider)
	{
		VolumeSlider->SetShowLabelOnlyOnHover(true);
		VolumeSlider->SetShowUnitsText(false);
		HideAudioSliderTextLabel(VolumeSlider->TakeWidget());
		VolumeSlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleSliderValueChanged);
	}

	if (SoundButton)
	{
		SoundButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSoundButtonClicked);
	}

	if (UAudioSettingsSubsystem* AudioSettingsSubsystem = GetAudioSettingsSubsystem())
	{
		MasterVolumeChangedHandle = AudioSettingsSubsystem->OnMasterVolumeChanged.AddUObject(
			this,
			&ThisClass::HandleMasterVolumeChanged);
	}

	BeginSoundButtonTexturePreload();
	SynchronizeFromSubsystem();
}

void UAudioVolumeControl::Shutdown()
{
	ReleaseSoundButtonTexturePreload();

	if (VolumeSlider)
	{
		VolumeSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleSliderValueChanged);
	}

	if (SoundButton)
	{
		SoundButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleSoundButtonClicked);
	}

	if (UAudioSettingsSubsystem* AudioSettingsSubsystem = GetAudioSettingsSubsystem())
	{
		if (MasterVolumeChangedHandle.IsValid())
		{
			AudioSettingsSubsystem->OnMasterVolumeChanged.Remove(MasterVolumeChangedHandle);
		}
		AudioSettingsSubsystem->SaveMasterVolumeSettings();
	}

	MasterVolumeChangedHandle.Reset();
	VolumeSlider = nullptr;
	SoundButton = nullptr;
	OwnerWidget.Reset();
	bSynchronizing = false;
}

void UAudioVolumeControl::BeginSoundButtonTexturePreload()
{
	ReleaseSoundButtonTexturePreload();
	const int32 PreloadGeneration = ++TexturePreloadGeneration;

	UUserWidget* Widget = OwnerWidget.Get();
	UGameInstance* GameInstance = Widget ? Widget->GetGameInstance() : nullptr;
	UGameSettingsSubsystem* SettingsSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UGameSettingsSubsystem>() : nullptr;
	if (!SettingsSubsystem)
	{
		return;
	}

	SettingsSubsystem->PreloadRuntimeContentAsync(
		FSimpleDelegate::CreateWeakLambda(
			this,
			[this, PreloadGeneration]()
			{
				if (PreloadGeneration == TexturePreloadGeneration)
				{
					SynchronizeFromSubsystem();
				}
			}));
}

void UAudioVolumeControl::ReleaseSoundButtonTexturePreload()
{
	++TexturePreloadGeneration;
}

void UAudioVolumeControl::HandleSliderValueChanged(const float NormalizedValue)
{
	if (bSynchronizing)
	{
		return;
	}

	if (UAudioSettingsSubsystem* AudioSettingsSubsystem = GetAudioSettingsSubsystem())
	{
		const int32 VolumePercent = FMath::Clamp(
			FMath::RoundToInt(NormalizedValue * static_cast<float>(AudioVolumeControlMaxMasterVolumePercent)),
			0,
			AudioVolumeControlMaxMasterVolumePercent);
		AudioSettingsSubsystem->SetMasterVolumePercent(VolumePercent);
	}
}

void UAudioVolumeControl::HandleSoundButtonClicked()
{
	if (UAudioSettingsSubsystem* AudioSettingsSubsystem = GetAudioSettingsSubsystem())
	{
		AudioSettingsSubsystem->ToggleMasterMute();
	}
}

void UAudioVolumeControl::HandleMasterVolumeChanged(const int32 VolumePercent)
{
	SetSliderValueFromPercent(VolumePercent);
	RefreshSoundButtonStyle(VolumePercent <= 0);
}

void UAudioVolumeControl::SynchronizeFromSubsystem()
{
	const UAudioSettingsSubsystem* AudioSettingsSubsystem = GetAudioSettingsSubsystem();
	if (!AudioSettingsSubsystem)
	{
		return;
	}

	const int32 VolumePercent = AudioSettingsSubsystem->GetMasterVolumePercent();
	SetSliderValueFromPercent(VolumePercent);
	RefreshSoundButtonStyle(AudioSettingsSubsystem->IsMasterMuted());
}

void UAudioVolumeControl::SetSliderValueFromPercent(const int32 VolumePercent)
{
	if (!VolumeSlider)
	{
		return;
	}

	TGuardValue<bool> SynchronizingGuard(bSynchronizing, true);
	VolumeSlider->Value = static_cast<float>(
		FMath::Clamp(VolumePercent, 0, AudioVolumeControlMaxMasterVolumePercent))
		/ static_cast<float>(AudioVolumeControlMaxMasterVolumePercent);
	static_cast<UAudioSliderBase*>(VolumeSlider.Get())->SynchronizeProperties();
}

void UAudioVolumeControl::RefreshSoundButtonStyle(const bool bMuted) const
{
	if (!SoundButton || !OwnerWidget.IsValid())
	{
		return;
	}

	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveLoadedGameSettingDefinition(OwnerWidget.Get());
	if (!SettingDefinition)
	{
		return;
	}

	UTexture2D* ButtonTexture = bMuted
		? SettingDefinition->SoundMutedButtonTexture.Get()
		: SettingDefinition->SoundEnabledButtonTexture.Get();
	if (!ButtonTexture)
	{
		return;
	}

	FButtonStyle ButtonStyle = SoundButton->GetStyle();
	SetBrushTexture(ButtonStyle.Normal, ButtonTexture);
	SetBrushTexture(ButtonStyle.Hovered, ButtonTexture);
	SetBrushTexture(ButtonStyle.Pressed, ButtonTexture);
	SetBrushTexture(ButtonStyle.Disabled, ButtonTexture);
	SoundButton->SetStyle(ButtonStyle);
}

UAudioSettingsSubsystem* UAudioVolumeControl::GetAudioSettingsSubsystem() const
{
	const UUserWidget* Widget = OwnerWidget.Get();
	UGameInstance* GameInstance = Widget ? Widget->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UAudioSettingsSubsystem>() : nullptr;
}
