#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Definition/Level/LevelDefinition.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "LobbyConfigurationComponent.generated.h"

class ALobbyGameMode;
class UDefaultProvisionDefinition;
struct FStreamableHandle;

/**
 * ALobbyGameMode의 데이터 조회와 선택된 맵에 대한 정책을 담당한다.
 */
UCLASS(ClassGroup = (Lobby))
class LABPROJECT_API ULobbyConfigurationComponent : public UActorComponent
{
	GENERATED_BODY()

private:
	enum class ERuntimeState : uint8 { NotStarted, Loading, Ready, Failed };

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Public API ------------------------------------------------------------------------------------------------------
	ULobbyConfigurationComponent();

	void InitializeRuntime(FSimpleDelegate OnReady = FSimpleDelegate());
	bool IsRuntimeReady() const { return RuntimeState == ERuntimeState::Ready; }
	bool HasRuntimeInitializationFailed() const { return RuntimeState == ERuntimeState::Failed; }
	void ApplyDefaultLobbyConfigIfNeeded();
	void SaveConfig(
		FName MapKey,
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

	FName ResolveConfiguredMapKey(FName MapKey);
	bool FindConfiguredMapOption(
		FName MapKey,
		FLobbyMatchMapOption& OutMapOption);
	int32 GetConfiguredMaxPlayerCount();
	int32 GetConfiguredMaxBotCount();

	const ULevelDefinition* GetLevelDefinition();
	const UMatchRuleDefinition* GetMatchRuleDefinition();
	const UDefaultProvisionDefinition* GetDefaultProvisionDefinition() const;

private:
	// Event Handlers --------------------------------------------------------------------------------------------------

	// Internal Helpers ------------------------------------------------------------------------------------------------
	ALobbyGameMode* GetLobbyGameMode() const;
	void FinishRuntimeInitialization(uint32 RequestGeneration);
	void ReleaseRuntimePreloads();

private:
	UPROPERTY(Transient)
	TObjectPtr<ULevelDefinition>
		LoadedLevelDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UMatchRuleDefinition>
		LoadedMatchRuleDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UDefaultProvisionDefinition>
		LoadedDefaultProvisionDefinition;

	ERuntimeState RuntimeState = ERuntimeState::NotStarted;
	TSharedPtr<FStreamableHandle> LobbyDependenciesPreloadHandle;
	FSimpleDelegate RuntimeReadyDelegate;
	uint32 RuntimePreloadRequestGeneration = 0;
};
