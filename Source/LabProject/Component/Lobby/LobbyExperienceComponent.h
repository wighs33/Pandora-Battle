#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "LobbyExperienceComponent.generated.h"

class ALobbyGameMode;
class UExperienceDefinition;

/**
 * 로비 Experience의 준비가 끝날 때까지 플레이어 시작을 보류하고, 실패 시 시작을 차단한다.
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
};
