#pragma once

#include "CoreMinimal.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BgmSubsystem.generated.h"

class UAudioComponent;
class UGameSettingDefinition;
class UWorld;
struct FStreamableHandle;

UCLASS()
class LABPROJECT_API UBgmSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!Audio")
	void PlayBgmForContext(EBgmContext BgmContext);

	UFUNCTION(BlueprintCallable, Category = "!Audio")
	void RestoreWorldBgm();

	UFUNCTION(BlueprintCallable, Category = "!Audio")
	void StopBgm();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);

	UFUNCTION()
	void HandleBgmAudioFinished();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void PlayBgmForContext(EBgmContext BgmContext, UWorld* World);
	void BeginBgmSoundPreload(
		uint64 LoadGeneration,
		EBgmContext BgmContext,
		TWeakObjectPtr<UWorld> World);
	void CompleteBgmSoundPreload(
		uint64 LoadGeneration,
		EBgmContext BgmContext,
		TWeakObjectPtr<UWorld> World);
	void CancelPendingBgmLoads();
	void StopActiveBgmAudio();
	EBgmContext ResolveWorldBgmContext(UWorld* World) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> StartupBgmAudioComponent;

	EBgmContext ActiveBgmContext = EBgmContext::Startup;
	FSoftObjectPath ActiveBgmSoundPath;
	FDelegateHandle PostLoadMapWithWorldHandle;
	float ActiveBgmBaseVolume = 1.0f;
	uint64 BgmLoadGeneration = 0;
	bool bSettingsLoadPending = false;
	bool bSoundLoadPending = false;
	TSharedPtr<FStreamableHandle> PendingSoundLoadHandle;
};
