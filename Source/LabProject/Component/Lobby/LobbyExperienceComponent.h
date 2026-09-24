#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "LobbyExperienceComponent.generated.h"

class ALobbyGameMode;
class UExperienceDefinition;

/**
 * 로비 Experience를 로딩하고 준비 상태와 사용할 Pawn 클래스를 제공한다.
 */
UCLASS(ClassGroup = (Lobby))
class LABPROJECT_API ULobbyExperienceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	ULobbyExperienceComponent();

	void StartExperienceLoad();
	bool IsExperienceLoaded() const;
	UClass* ResolveExperiencePawnClass() const;
	FPrimaryAssetId GetConfiguredExperienceId() const;

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleExperienceLoaded(
		const UExperienceDefinition* Experience);
	void HandleExperienceLoadFailed(
		FPrimaryAssetId ExperienceId,
		const FString& FailureMessage);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	ALobbyGameMode* GetLobbyGameMode() const;

public:
	FSimpleMulticastDelegate OnExperienceReady;
};
