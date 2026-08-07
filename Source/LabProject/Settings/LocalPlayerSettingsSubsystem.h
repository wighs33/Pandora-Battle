#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "LocalPlayerSettingsSubsystem.generated.h"

class APlayerController;
class UInputAction;
class UInputMappingContext;
class UInputSettingsSaveGame;
class UGameSettingDefinition;

UCLASS()
class LABPROJECT_API ULocalPlayerSettingsSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	static ULocalPlayerSettingsSubsystem* Get(const APlayerController* PlayerController);
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "!Setting|Local Player")
	void ApplyLocalPlayerSettings(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "!Setting|Mouse Cursor")
	bool ApplyConfiguredMouseCursor(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "!Setting|Camera")
	void ApplyCameraViewPitchClamp(APlayerController* PlayerController);

	UFUNCTION(BlueprintPure, Category = "!Setting|Mouse Sensitivity")
	int32 GetMouseSensitivityPercent() const;

	UFUNCTION(BlueprintPure, Category = "!Setting|Mouse Sensitivity")
	float GetMouseSensitivityMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "!Setting|Mouse Sensitivity")
	float GetMouseSensitivitySliderValue() const;

	UFUNCTION(BlueprintCallable, Category = "!Setting|Mouse Sensitivity")
	void SetMouseSensitivitySliderValue(float NormalizedValue);

	UFUNCTION(BlueprintCallable, Category = "!Setting|Mouse Sensitivity")
	void SaveInputSettings();

	bool AddInputMappingContext(UInputMappingContext* InputMappingContext, int32 Priority) const;
	bool RemoveInputMappingContext(UInputMappingContext* InputMappingContext) const;
	TArray<FKey> QueryKeysMappedToAction(const UInputAction* InputAction) const;

private:
	void LoadInputSettings();
	void ApplyMouseSensitivity(APlayerController* PlayerController) const;
	bool ApplyLoadedConfiguredMouseCursor(
		APlayerController* PlayerController,
		const UGameSettingDefinition* SettingDefinition);
	void QueueRuntimeSettingsApplication(APlayerController* PlayerController);
	void ReleaseRuntimeSettingsPreload();

	TWeakObjectPtr<APlayerController> PendingSettingsPlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UInputSettingsSaveGame> InputSettingsSaveGame;

	uint64 RuntimeSettingsPreloadGeneration = 0;
	int32 MouseSensitivityPercent = 100;
	bool bRuntimeSettingsLoadPending = false;
	bool bInputSettingsDirty = false;
};
