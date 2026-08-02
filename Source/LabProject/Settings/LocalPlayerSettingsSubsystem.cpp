#include "Settings/LocalPlayerSettingsSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Definition/Settings/GameSettingDefinition.h"
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

void ULocalPlayerSettingsSubsystem::Deinitialize()
{
	ReleaseRuntimeSettingsPreload();
	Super::Deinitialize();
}

void ULocalPlayerSettingsSubsystem::ApplyLocalPlayerSettings(APlayerController* PlayerController)
{
	ApplyConfiguredMouseCursor(PlayerController);
	ApplyCameraViewPitchClamp(PlayerController);
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
