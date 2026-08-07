#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameInstanceRuntimeSubsystem.generated.h"

class UPdGameInstanceDefinition;

/**
 * Composition root and start/stop coordinator for GameInstance-lifetime services.
 */
UCLASS(Config = Game, DefaultConfig)
class LABPROJECT_API UGameInstanceRuntimeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void HandleGameInstanceStarted();
	void HandleGameInstanceShutdown();
	UPdGameInstanceDefinition* GetGameInstanceDefinition();

private:
	void ApplyConfiguredWindowMode() const;

	UPROPERTY(Transient)
	TSoftObjectPtr<UPdGameInstanceDefinition> GameInstanceDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UPdGameInstanceDefinition> LoadedGameInstanceDefinition;

	bool bShutdownHandled = false;
};
