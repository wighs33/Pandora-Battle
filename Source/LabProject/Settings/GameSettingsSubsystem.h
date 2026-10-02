#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameSettingsSubsystem.generated.h"

class UGameSettingDefinition;
struct FStreamableHandle;

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
	static UGameSettingDefinition* ResolveLoadedGameSettingDefinition(
		const UObject* WorldContextObject);

	UGameSettingDefinition* GetGameSettingDefinition();

	UGameSettingDefinition* GetLoadedGameSettingDefinition() const;
	void PreloadRuntimeContentAsync(
		FSimpleDelegate OnComplete = FSimpleDelegate());
	bool IsRuntimeContentReady() const { return bRuntimeContentReady; }
	bool IsRuntimeContentLoading() const { return bRuntimeContentPreloadPending; }

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleDefinitionPreloadComplete();
	void HandleRuntimeContentPreloadComplete(
		TArray<FSoftObjectPath> ExpectedAssetPaths);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	static FSoftObjectPath GetDefaultGameSettingDefinitionPath();
	void FinishRuntimeContentPreload(bool bSucceeded);
	void ReleaseRuntimeContentPreloadHandles();

private:
	UPROPERTY(Transient)
	TSoftObjectPtr<UGameSettingDefinition> GameSettingDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UGameSettingDefinition> CachedGameSettingDefinition;

	TArray<FSimpleDelegate> PendingRuntimeContentCallbacks;
	TSharedPtr<FStreamableHandle> DefinitionPreloadHandle;
	TSharedPtr<FStreamableHandle> RuntimeContentPreloadHandle;
	bool bRuntimeContentPreloadPending = false;
	bool bRuntimeContentReady = false;
	bool bReportedMissingGameSettingDefinition = false;
};
