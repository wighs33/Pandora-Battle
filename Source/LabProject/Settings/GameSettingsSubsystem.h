#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameSettingsSubsystem.generated.h"

class UGameSettingDefinition;
class FContentLease;

UCLASS()
class LABPROJECT_API UGameSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Public API ------------------------------------------------------------------------------------------------------
	static UGameSettingDefinition* ResolveGameSettingDefinition(const UObject* WorldContextObject);
	static UGameSettingDefinition* ResolveLoadedGameSettingDefinition(const UObject* WorldContextObject);

	UGameSettingDefinition* GetGameSettingDefinition();

	UGameSettingDefinition* GetLoadedGameSettingDefinition() const;
	void PreloadRuntimeContentAsync(FSimpleDelegate OnComplete = FSimpleDelegate());
	bool IsRuntimeContentReady() const { return bRuntimeContentReady; }
	bool IsRuntimeContentLoading() const { return bRuntimeContentPreloadPending; }

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleDefinitionPreloadComplete();
	void HandleRuntimeContentPreloadComplete();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	static FSoftObjectPath GetDefaultGameSettingDefinitionPath();
	void FinishRuntimeContentPreload(bool bSucceeded);

private:
	UPROPERTY(Transient)
	TSoftObjectPtr<UGameSettingDefinition> GameSettingDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UGameSettingDefinition> CachedGameSettingDefinition;

	TArray<FSimpleDelegate> PendingRuntimeContentCallbacks;
	TSharedPtr<FContentLease> DefinitionLease;
	TSharedPtr<FContentLease> RuntimeContentLease;
	bool bRuntimeContentPreloadPending = false;
	bool bRuntimeContentReady = false;
	bool bReportedMissingGameSettingDefinition = false;
};
