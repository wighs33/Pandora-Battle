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

	/** 로비가 고른 다음 경기 맵. 이름으로 문맥을 알 수 없는 전환 맵에서도 훈련장 BGM을 이어 틀 때 쓴다. */
	void SetSelectedMatchMapKey(FName MapKey) { SelectedMatchMapKey = MapKey; }

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
	FName SelectedMatchMapKey;
	FSoftObjectPath ActiveBgmSoundPath;
	FDelegateHandle PostLoadMapWithWorldHandle;
	uint64 BgmLoadGeneration = 0;
	TSharedPtr<FContentLease> SoundLease;
};
