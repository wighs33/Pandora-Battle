#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PdGameInstanceRuntimeSubsystem.generated.h"

class UPdGameInstanceDefinition;
struct FStreamableHandle;

/**
 * Composition root and start/stop coordinator for GameInstance-lifetime services.
 */
UCLASS(Config = Game, DefaultConfig)
class LABPROJECT_API UPdGameInstanceRuntimeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPdGameInstanceRuntimeSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void HandleGameInstanceStarted();
	void HandleGameInstanceShutdown();
	UPdGameInstanceDefinition* GetGameInstanceDefinition();

private:
	void BeginGameInstanceDefinitionPreload();
	void HandleGameInstanceDefinitionPreloaded(uint32 RequestGeneration);
	void ReleaseGameInstanceDefinitionPreload();
	void ApplyRuntimeSettings();
	void ApplyConfiguredWindowMode() const;
	void DisablePackagedCollisionVisualization() const;

	UPROPERTY(Config, EditAnywhere, Category = "Game Instance",
		meta = (AllowedTypes = "GameInstanceDefinition"))
	TSoftObjectPtr<UPdGameInstanceDefinition> GameInstanceDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UPdGameInstanceDefinition> LoadedGameInstanceDefinition;

	TSharedPtr<FStreamableHandle> GameInstanceDefinitionLoadHandle;
	uint32 GameInstanceDefinitionLoadGeneration = 0;
	bool bGameInstanceStarted = false;
	bool bShutdownHandled = false;
};
