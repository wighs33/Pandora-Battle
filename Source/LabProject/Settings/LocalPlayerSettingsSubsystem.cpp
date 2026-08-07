#include "Settings/LocalPlayerSettingsSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "SavedGameData/InputSettingsSaveGame.h"
#include "Settings/GameSettingsSubsystem.h"
#include "UI/Cursor/MouseCursorWidget.h"
#include "Widgets/SWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LocalPlayerSettingsSubsystem)

namespace
{
	constexpr float DefaultPlayerViewPitchMin = -60.0f;
	constexpr float DefaultPlayerViewPitchMax = 60.0f;
	constexpr float MinSupportedViewPitch = -89.9f;
	constexpr float MaxSupportedViewPitch = 89.9f;
	constexpr int32 MinMouseSensitivityPercent = 10;
	constexpr int32 MaxMouseSensitivityPercent = 200;
	constexpr int32 DefaultMouseSensitivityPercent = 100;

	const FString& GetInputSettingsSaveSlotName()
	{
		static const FString InputSettingsSaveSlotName(TEXT("InputSettings"));
		return InputSettingsSaveSlotName;
	}

	constexpr EMouseCursor::Type CustomCursorMappedTypes[] =
	{
		EMouseCursor::Default,
		EMouseCursor::TextEditBeam,
		EMouseCursor::ResizeLeftRight,
		EMouseCursor::ResizeUpDown,
		EMouseCursor::ResizeSouthEast,
		EMouseCursor::ResizeSouthWest,
		EMouseCursor::CardinalCross,
		EMouseCursor::Crosshairs,
		EMouseCursor::Hand,
		EMouseCursor::GrabHand,
		EMouseCursor::GrabHandClosed,
		EMouseCursor::SlashedCircle,
		EMouseCursor::EyeDropper,
		EMouseCursor::Custom,
	};
}

ULocalPlayerSettingsSubsystem* ULocalPlayerSettingsSubsystem::Get(const APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return nullptr;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<ULocalPlayerSettingsSubsystem>() : nullptr;
}

void ULocalPlayerSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadInputSettings();
}

void ULocalPlayerSettingsSubsystem::Deinitialize()
{
	ReleaseRuntimeSettingsPreload();
	SaveInputSettings();
	Super::Deinitialize();
}

void ULocalPlayerSettingsSubsystem::ApplyLocalPlayerSettings(APlayerController* PlayerController)
{
	ApplyConfiguredMouseCursor(PlayerController);
	ApplyCameraViewPitchClamp(PlayerController);
	ApplyMouseSensitivity(PlayerController);
}

bool ULocalPlayerSettingsSubsystem::ApplyConfiguredMouseCursor(APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return false;
	}

	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveLoadedGameSettingDefinition(PlayerController);
	if (!SettingDefinition)
	{
		QueueRuntimeSettingsApplication(PlayerController);
		return false;
	}

	return ApplyLoadedConfiguredMouseCursor(PlayerController, SettingDefinition);
}

bool ULocalPlayerSettingsSubsystem::ApplyLoadedConfiguredMouseCursor(
	APlayerController* PlayerController,
	const UGameSettingDefinition* SettingDefinition)
{
	if (!SettingDefinition || !SettingDefinition->bUseCustomMouseCursor)
	{
		return false;
	}

	UTexture2D* CursorTexture = SettingDefinition->MouseCursorTexture.Get();
	if (!CursorTexture)
	{
		return false;
	}

	UMouseCursorWidget* CursorWidget = CreateWidget<UMouseCursorWidget>(PlayerController, UMouseCursorWidget::StaticClass());
	if (!CursorWidget)
	{
		return false;
	}

	const EMouseCursor::Type CursorType = SettingDefinition->MouseCursorType.GetValue();
	CursorWidget->ConfigureCursor(
		CursorTexture,
		SettingDefinition->MouseCursorSize,
		SettingDefinition->MouseCursorHotSpot);

	UGameViewportClient* ViewportClient = nullptr;
	if (const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
	{
		ViewportClient = LocalPlayer->ViewportClient;
		if (ViewportClient)
		{
			ViewportClient->SetUseSoftwareCursorWidgets(true);
		}
	}

	if (ViewportClient)
	{
		const TSharedRef<SWidget> CursorSlateWidget = CursorWidget->TakeWidget();
		for (const EMouseCursor::Type MappedCursorType : CustomCursorMappedTypes)
		{
			ViewportClient->SetSoftwareCursorWidget(MappedCursorType, CursorSlateWidget);
		}
	}
	else
	{
		PlayerController->SetMouseCursorWidget(CursorType, CursorWidget);
	}

	PlayerController->DefaultMouseCursor = CursorType;
	PlayerController->CurrentMouseCursor = CursorType;
	return true;
}

void ULocalPlayerSettingsSubsystem::ApplyCameraViewPitchClamp(APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	float ConfiguredPitchMin = DefaultPlayerViewPitchMin;
	float ConfiguredPitchMax = DefaultPlayerViewPitchMax;

	if (const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveLoadedGameSettingDefinition(PlayerController))
	{
		ConfiguredPitchMin = SettingDefinition->PlayerViewPitchMin;
		ConfiguredPitchMax = SettingDefinition->PlayerViewPitchMax;
	}
	else
	{
		QueueRuntimeSettingsApplication(PlayerController);
	}

	const float SanitizedPitchMin = FMath::Clamp(FMath::Min(ConfiguredPitchMin, ConfiguredPitchMax), MinSupportedViewPitch, MaxSupportedViewPitch);
	const float SanitizedPitchMax = FMath::Clamp(FMath::Max(ConfiguredPitchMin, ConfiguredPitchMax), MinSupportedViewPitch, MaxSupportedViewPitch);

	if (PlayerController->PlayerCameraManager)
	{
		PlayerController->PlayerCameraManager->ViewPitchMin = SanitizedPitchMin;
		PlayerController->PlayerCameraManager->ViewPitchMax = SanitizedPitchMax;
	}

	FRotator CurrentControlRotation = PlayerController->GetControlRotation();
	const float ClampedPitch = FMath::Clamp(FRotator::NormalizeAxis(CurrentControlRotation.Pitch), SanitizedPitchMin, SanitizedPitchMax);
	if (!FMath::IsNearlyEqual(CurrentControlRotation.Pitch, ClampedPitch))
	{
		CurrentControlRotation.Pitch = ClampedPitch;
		PlayerController->SetControlRotation(CurrentControlRotation);
	}
}

int32 ULocalPlayerSettingsSubsystem::GetMouseSensitivityPercent() const
{
	return FMath::Clamp(
		MouseSensitivityPercent,
		MinMouseSensitivityPercent,
		MaxMouseSensitivityPercent);
}

float ULocalPlayerSettingsSubsystem::GetMouseSensitivityMultiplier() const
{
	return static_cast<float>(GetMouseSensitivityPercent())
		/ static_cast<float>(DefaultMouseSensitivityPercent);
}

float ULocalPlayerSettingsSubsystem::GetMouseSensitivitySliderValue() const
{
	return FMath::GetMappedRangeValueClamped(
		FVector2D(MinMouseSensitivityPercent, MaxMouseSensitivityPercent),
		FVector2D(0.0, 1.0),
		static_cast<double>(GetMouseSensitivityPercent()));
}

void ULocalPlayerSettingsSubsystem::SetMouseSensitivitySliderValue(const float NormalizedValue)
{
	const int32 NewSensitivityPercent = FMath::RoundToInt(FMath::GetMappedRangeValueClamped(
		FVector2D(0.0, 1.0),
		FVector2D(MinMouseSensitivityPercent, MaxMouseSensitivityPercent),
		static_cast<double>(NormalizedValue)));
	if (MouseSensitivityPercent != NewSensitivityPercent)
	{
		MouseSensitivityPercent = NewSensitivityPercent;
		bInputSettingsDirty = true;
	}

	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		ApplyMouseSensitivity(LocalPlayer->GetPlayerController(GetWorld()));
	}
}

void ULocalPlayerSettingsSubsystem::ApplyMouseSensitivity(
	APlayerController* PlayerController) const
{
	if (!PlayerController
		|| !PlayerController->IsLocalController()
		|| !PlayerController->PlayerInput)
	{
		return;
	}

	const UInputSettings* DefaultInputSettings = GetDefault<UInputSettings>();
	if (!DefaultInputSettings)
	{
		return;
	}

	const float SensitivityMultiplier = GetMouseSensitivityMultiplier();
	bool bAppliedAxisSensitivity = false;
	const auto ApplyAxisSensitivity = [
		PlayerController,
		DefaultInputSettings,
		SensitivityMultiplier,
		&bAppliedAxisSensitivity](
		const FKey& AxisKey)
	{
		FInputAxisProperties RuntimeAxisProperties;
		if (!PlayerController->PlayerInput->GetAxisProperties(AxisKey, RuntimeAxisProperties))
		{
			return;
		}

		float DefaultSensitivity = 1.0f;
		for (const FInputAxisConfigEntry& DefaultAxisConfig : DefaultInputSettings->AxisConfig)
		{
			if (DefaultAxisConfig.AxisKeyName == AxisKey.GetFName())
			{
				DefaultSensitivity = DefaultAxisConfig.AxisProperties.Sensitivity;
				break;
			}
		}

		RuntimeAxisProperties.Sensitivity = DefaultSensitivity * SensitivityMultiplier;
		PlayerController->PlayerInput->SetAxisProperties(AxisKey, RuntimeAxisProperties);
		bAppliedAxisSensitivity = true;
	};

	ApplyAxisSensitivity(EKeys::MouseX);
	ApplyAxisSensitivity(EKeys::MouseY);
	ApplyAxisSensitivity(EKeys::Mouse2D);

	if (bAppliedAxisSensitivity)
	{
		if (const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
				LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				FModifyContextOptions RebuildOptions;
				RebuildOptions.bForceImmediately = true;
				InputSubsystem->RequestRebuildControlMappings(RebuildOptions);
			}
		}
	}
}

void ULocalPlayerSettingsSubsystem::SaveInputSettings()
{
	if (!bInputSettingsDirty)
	{
		return;
	}

	if (!IsValid(InputSettingsSaveGame))
	{
		InputSettingsSaveGame = Cast<UInputSettingsSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UInputSettingsSaveGame::StaticClass()));
	}
	if (!IsValid(InputSettingsSaveGame))
	{
		return;
	}

	InputSettingsSaveGame->bHasMouseSensitivitySetting = true;
	InputSettingsSaveGame->MouseSensitivityPercent = GetMouseSensitivityPercent();
	if (UGameplayStatics::SaveGameToSlot(InputSettingsSaveGame, GetInputSettingsSaveSlotName(), 0))
	{
		bInputSettingsDirty = false;
	}
}

void ULocalPlayerSettingsSubsystem::LoadInputSettings()
{
	if (UGameplayStatics::DoesSaveGameExist(GetInputSettingsSaveSlotName(), 0))
	{
		InputSettingsSaveGame = Cast<UInputSettingsSaveGame>(
			UGameplayStatics::LoadGameFromSlot(GetInputSettingsSaveSlotName(), 0));
	}

	if (!IsValid(InputSettingsSaveGame))
	{
		InputSettingsSaveGame = Cast<UInputSettingsSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UInputSettingsSaveGame::StaticClass()));
	}

	if (IsValid(InputSettingsSaveGame)
		&& InputSettingsSaveGame->bHasMouseSensitivitySetting)
	{
		MouseSensitivityPercent = FMath::Clamp(
			InputSettingsSaveGame->MouseSensitivityPercent,
			MinMouseSensitivityPercent,
			MaxMouseSensitivityPercent);
		return;
	}

	MouseSensitivityPercent = DefaultMouseSensitivityPercent;
	bInputSettingsDirty = true;
	SaveInputSettings();
}

void ULocalPlayerSettingsSubsystem::QueueRuntimeSettingsApplication(
	APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	if (bRuntimeSettingsLoadPending
		&& PendingSettingsPlayerController.Get() == PlayerController)
	{
		return;
	}

	ReleaseRuntimeSettingsPreload();
	PendingSettingsPlayerController = PlayerController;
	const uint64 PreloadGeneration = RuntimeSettingsPreloadGeneration;

	UGameInstance* GameInstance = PlayerController->GetGameInstance();
	UGameSettingsSubsystem* SettingsSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UGameSettingsSubsystem>() : nullptr;
	if (!SettingsSubsystem)
	{
		PendingSettingsPlayerController.Reset();
		return;
	}

	bRuntimeSettingsLoadPending = true;
	SettingsSubsystem->PreloadRuntimeContentAsync(
		FSimpleDelegate::CreateWeakLambda(
			this,
			[this, PreloadGeneration]()
			{
				if (PreloadGeneration != RuntimeSettingsPreloadGeneration)
				{
					return;
				}

				bRuntimeSettingsLoadPending = false;
				APlayerController* PlayerController =
					PendingSettingsPlayerController.Get();
				PendingSettingsPlayerController.Reset();
				const UGameSettingDefinition* SettingDefinition =
					UGameSettingsSubsystem::ResolveLoadedGameSettingDefinition(
						PlayerController);
				if (PlayerController && SettingDefinition)
				{
					ApplyLoadedConfiguredMouseCursor(
						PlayerController,
						SettingDefinition);
					ApplyCameraViewPitchClamp(PlayerController);
				}
			}));
}

void ULocalPlayerSettingsSubsystem::ReleaseRuntimeSettingsPreload()
{
	++RuntimeSettingsPreloadGeneration;
	bRuntimeSettingsLoadPending = false;
	PendingSettingsPlayerController.Reset();
}

bool ULocalPlayerSettingsSubsystem::AddInputMappingContext(UInputMappingContext* InputMappingContext, const int32 Priority) const
{
	if (!InputMappingContext)
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
		: nullptr;
	if (!InputSubsystem)
	{
		return false;
	}

	InputSubsystem->AddMappingContext(InputMappingContext, Priority);
	return true;
}

bool ULocalPlayerSettingsSubsystem::RemoveInputMappingContext(UInputMappingContext* InputMappingContext) const
{
	if (!InputMappingContext)
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
		: nullptr;
	if (!InputSubsystem)
	{
		return false;
	}

	InputSubsystem->RemoveMappingContext(InputMappingContext);
	return true;
}

TArray<FKey> ULocalPlayerSettingsSubsystem::QueryKeysMappedToAction(const UInputAction* InputAction) const
{
	if (!InputAction)
	{
		return {};
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
		: nullptr;
	return InputSubsystem ? InputSubsystem->QueryKeysMappedToAction(InputAction) : TArray<FKey>();
}
