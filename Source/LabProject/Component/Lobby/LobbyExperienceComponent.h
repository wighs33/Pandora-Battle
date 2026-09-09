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
	ULobbyExperienceComponent();

	void StartExperienceLoad();
	bool IsExperienceLoaded() const;
	FSimpleMulticastDelegate OnExperienceReady;
	UClass* ResolveExperiencePawnClass() const;
	FPrimaryAssetId GetConfiguredExperienceId() const;

private:
	ALobbyGameMode* GetLobbyGameMode() const;
	void HandleExperienceLoaded(
		const UExperienceDefinition* Experience);
	void HandleExperienceLoadFailed(
		FPrimaryAssetId ExperienceId,
		const FString& FailureMessage);
};
