#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "LobbyConfigurationComponent.generated.h"

class ALobbyGameMode;
class ULobbyModeDefinition;
class UDefaultProvisionDefinition;
struct FStreamableHandle;

/**
 * Data resolution and selected-map policy for ALobbyGameMode.
 */
UCLASS(ClassGroup = (Lobby))
class LABPROJECT_API ULobbyConfigurationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULobbyConfigurationComponent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void InitializeRuntime(FSimpleDelegate OnReady = FSimpleDelegate());
	void ApplyDefaultLobbyConfigIfNeeded();
	void SyncSelectedLobbyConfigToRuntime();
	void SaveConfig(
		FName MapKey,
		int32 InMaxPlayerCount,
		int32 InMaxBotCount);

	FString GetRoomTravelMapName();
	FString ResolveTravelMapName(FName MapKey);
	FName GetFirstMapKey();
	int32 GetLobbyMapOptionCount();
	bool GetLobbyMapOptionAtIndex(
		int32 Index,
		FLobbyMatchMapOption& OutMapOption);
	bool GetSelectedLobbyMapOption(
		FLobbyMatchMapOption& OutMapOption);
	FName GetSelectedLobbyMapKey();
	int32 GetSelectedLobbyMaxPlayerCount();
	void SelectLobbyMapByOffset(int32 Offset);

	FName ResolveConfiguredMapKey(FName MapKey);
	bool FindConfiguredMapOption(
		FName MapKey,
		FLobbyMatchMapOption& OutMapOption);
	int32 GetConfiguredMaxPlayerCount();
	int32 GetConfiguredMaxBotCount(FName MapKey);

	const ULobbyModeDefinition* GetLobbyModeDefinition();
	const UMatchRuleDefinition* GetMatchRuleDefinition();
	const UDefaultProvisionDefinition* GetDefaultProvisionDefinition();

private:
	ALobbyGameMode* GetLobbyGameMode() const;
	FString ResolveSoftMapPath(
		const TSoftObjectPtr<UWorld>& Map,
		const FString& FallbackTravelMapName) const;
	void HandleLobbyModePreloadComplete(uint32 RequestGeneration);
	void HandleLobbyDependenciesPreloadComplete(uint32 RequestGeneration);
	void FinishRuntimeInitialization(uint32 RequestGeneration);
	void ReleaseRuntimePreloads();

	UPROPERTY(EditDefaultsOnly, Category = "!Lobby|Definition",
		meta = (AllowedTypes = "LobbyModeDefinition"))
	TSoftObjectPtr<ULobbyModeDefinition> LobbyModeDefinition;

	UPROPERTY(Transient)
	TObjectPtr<ULobbyModeDefinition>
		LoadedLobbyModeDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UMatchRuleDefinition>
		LoadedMatchRuleDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UDefaultProvisionDefinition>
		LoadedDefaultProvisionDefinition;

	bool bLoggedMissingLobbyModeDefinition = false;
	bool bLoggedMissingMatchRuleDefinition = false;
	bool bLoggedMissingDefaultProvisionDefinition = false;
	TSharedPtr<FStreamableHandle> LobbyModePreloadHandle;
	TSharedPtr<FStreamableHandle> LobbyDependenciesPreloadHandle;
	FSimpleDelegate RuntimeReadyDelegate;
	uint32 RuntimePreloadRequestGeneration = 0;
};
