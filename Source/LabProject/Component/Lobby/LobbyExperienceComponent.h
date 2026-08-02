#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "LobbyExperienceComponent.generated.h"

class ALobbyGameMode;
class UExperienceDefinition;

/**
 * Experience bootstrap and deferred player-start policy for the lobby.
 */
UCLASS(ClassGroup = (Lobby))
class LABPROJECT_API ULobbyExperienceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULobbyExperienceComponent();

	void StartExperienceLoad();
	bool IsExperienceLoaded() const;
	bool ShouldDelayPlayerStart() const;
	UClass* ResolveExperiencePawnClass() const;
	FPrimaryAssetId GetConfiguredExperienceId() const;

private:
	ALobbyGameMode* GetLobbyGameMode() const;
	void HandleExperienceLoaded(
		const UExperienceDefinition* Experience);
	void HandleExperienceLoadFailed(
		FPrimaryAssetId ExperienceId,
		const FString& FailureMessage);
	void ResumeWaitingPlayers();

	bool bWaitingForExperience = false;
};
