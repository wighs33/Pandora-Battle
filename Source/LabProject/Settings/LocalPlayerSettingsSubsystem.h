#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "LocalPlayerSettingsSubsystem.generated.h"

class APlayerController;
class UInputAction;
class UInputMappingContext;
class UGameSettingDefinition;

UCLASS()
class LABPROJECT_API ULocalPlayerSettingsSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	static ULocalPlayerSettingsSubsystem* Get(const APlayerController* PlayerController);
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "!Setting|Local Player")
	void ApplyLocalPlayerSettings(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "!Setting|Mouse Cursor")
	bool ApplyConfiguredMouseCursor(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "!Setting|Camera")
	void ApplyCameraViewPitchClamp(APlayerController* PlayerController);

	bool AddInputMappingContext(UInputMappingContext* InputMappingContext, int32 Priority) const;
	bool RemoveInputMappingContext(UInputMappingContext* InputMappingContext) const;
	TArray<FKey> QueryKeysMappedToAction(const UInputAction* InputAction) const;

private:
	bool ApplyLoadedConfiguredMouseCursor(
		APlayerController* PlayerController,
		const UGameSettingDefinition* SettingDefinition);
	void QueueRuntimeSettingsApplication(APlayerController* PlayerController);
	void ReleaseRuntimeSettingsPreload();

	TWeakObjectPtr<APlayerController> PendingSettingsPlayerController;
	uint64 RuntimeSettingsPreloadGeneration = 0;
	bool bRuntimeSettingsLoadPending = false;
};
