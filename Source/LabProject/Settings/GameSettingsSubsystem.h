#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameSettingsSubsystem.generated.h"

class UGameSettingDefinition;
struct FStreamableHandle;

UCLASS(Config=Game, DefaultConfig)
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
	static FSoftObjectPath GetDefaultGameSettingDefinitionPath();

	UFUNCTION(BlueprintCallable, Category = "!Setting")
	UGameSettingDefinition* GetGameSettingDefinition();

	UGameSettingDefinition* GetLoadedGameSettingDefinition() const;
	void PreloadRuntimeContentAsync(
		FSimpleDelegate OnComplete = FSimpleDelegate());
	bool IsGameSettingDefinitionReady() const
	{
		return bGameSettingDefinitionReady && CachedGameSettingDefinition != nullptr;
	}
	bool IsRuntimeContentReady() const { return bRuntimeContentReady; }

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleDefinitionPreloadComplete();
	void HandleRuntimeContentPreloadComplete(
		TArray<FSoftObjectPath> ExpectedAssetPaths);

	// Internal Helpers ------------------------------------------------------------------------------------------------
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
	bool bGameSettingDefinitionReady = false;
	bool bRuntimeContentReady = false;
};
