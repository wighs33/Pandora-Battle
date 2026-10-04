#pragma once

#include "CoreMinimal.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BgmSubsystem.generated.h"

class UAudioComponent;
class UWorld;
class FContentLease;

UCLASS()
class LABPROJECT_API UBgmSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Public API ------------------------------------------------------------------------------------------------------
	void PlayBgmForContext(EBgmContext BgmContext);

	void RestoreWorldBgm();

	void StopBgm();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);

	UFUNCTION()
	void HandleBgmAudioFinished();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void PlayBgmForContext(EBgmContext BgmContext, UWorld* World);
	void BeginBgmSoundPreload(uint64 LoadGeneration, EBgmContext BgmContext, TWeakObjectPtr<UWorld> World);
	void CompleteBgmSoundPreload(EBgmContext BgmContext, TWeakObjectPtr<UWorld> World);
	void CancelPendingBgmLoads();
	void StopActiveBgmAudio();
	EBgmContext ResolveWorldBgmContext(UWorld* World) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveBgmAudioComponent;

	EBgmContext ActiveBgmContext = EBgmContext::Startup;
	FSoftObjectPath ActiveBgmSoundPath;
	FDelegateHandle PostLoadMapWithWorldHandle;
	uint64 BgmLoadGeneration = 0;
	TSharedPtr<FContentLease> SoundLease;
};
