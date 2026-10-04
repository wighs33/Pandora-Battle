#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "LocalPlayerSettingsSubsystem.generated.h"

class APlayerController;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
class UInputSettingsSaveGame;
class UGameSettingDefinition;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCustomMouseCursorSettingsReady,
	APlayerController* /*PlayerController*/,
	const UGameSettingDefinition& /*SettingDefinition*/);

UCLASS()
class LABPROJECT_API ULocalPlayerSettingsSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Public API ------------------------------------------------------------------------------------------------------
	static ULocalPlayerSettingsSubsystem* Get(const APlayerController* PlayerController);

	void ApplyLocalPlayerSettings(APlayerController* PlayerController);

	void ApplyCameraViewPitchClamp(APlayerController* PlayerController);

	float GetMouseSensitivitySliderValue() const;

	void SetMouseSensitivitySliderValue(float NormalizedValue);

	void SaveInputSettings();

	bool AddInputMappingContext(UInputMappingContext* InputMappingContext, int32 Priority) const;
	bool RemoveInputMappingContext(UInputMappingContext* InputMappingContext) const;
	TArray<FKey> QueryKeysMappedToAction(const UInputAction* InputAction) const;

public:
	/** 사용자 지정 커서 설정이 준비되면 알린다. 커서 위젯을 만들어 뷰포트에 거는 일은 UI가 맡는다. */
	FOnCustomMouseCursorSettingsReady OnCustomMouseCursorSettingsReady;

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	bool ApplyConfiguredMouseCursor(APlayerController* PlayerController);
	int32 GetMouseSensitivityPercent() const;
	float GetMouseSensitivityMultiplier() const;
	void LoadInputSettings();
	void ApplyMouseSensitivity(APlayerController* PlayerController) const;
	bool ApplyLoadedConfiguredMouseCursor(APlayerController* PlayerController,
		const UGameSettingDefinition* SettingDefinition);
	void QueueRuntimeSettingsApplication(APlayerController* PlayerController);
	void ReleaseRuntimeSettingsPreload();

private:
	UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;

private:
	TWeakObjectPtr<APlayerController> PendingSettingsPlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UInputSettingsSaveGame> InputSettingsSaveGame;

	uint64 RuntimeSettingsPreloadGeneration = 0;
	int32 MouseSensitivityPercent = 100;
	bool bRuntimeSettingsLoadPending = false;
	bool bInputSettingsDirty = false;
};
