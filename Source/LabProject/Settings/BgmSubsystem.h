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
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "!Audio")
	void PlayBgmForContext(EPdBgmContext BgmContext);

	UFUNCTION(BlueprintCallable, Category = "!Audio")
	void RestoreWorldBgm();

	UFUNCTION(BlueprintCallable, Category = "!Audio")
	void StopBgm();

private:
	void PlayBgmForContext(EPdBgmContext BgmContext, UWorld* World);
	void BeginBgmSoundPreload(
		uint64 LoadGeneration,
		EPdBgmContext BgmContext,
		TWeakObjectPtr<UWorld> World);
	void CompleteBgmSoundPreload(
		uint64 LoadGeneration,
		EPdBgmContext BgmContext,
		TWeakObjectPtr<UWorld> World);
	void CancelPendingBgmLoads();
	void StopActiveBgmAudio();
	EPdBgmContext ResolveWorldBgmContext(UWorld* World) const;
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);

	UFUNCTION()
	void HandleBgmAudioFinished();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> StartupBgmAudioComponent;

	EPdBgmContext ActiveBgmContext = EPdBgmContext::Startup;
	FSoftObjectPath ActiveBgmSoundPath;
	FDelegateHandle PostLoadMapWithWorldHandle;
	float ActiveBgmBaseVolume = 1.0f;
	uint64 BgmLoadGeneration = 0;
	bool bSettingsLoadPending = false;
	bool bSoundLoadPending = false;
	TSharedPtr<FStreamableHandle> PendingSoundLoadHandle;
};
