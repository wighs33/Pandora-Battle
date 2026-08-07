#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "ControllerSessionComponent.generated.h"

class APdPlayerController;
struct FGameResultPresentationData;

/**
 * Client/server match-exit and title-travel coordinator.
 *
 * Authoritative match decisions stay in AExperienceGameMode; this component
 * only bridges the controller RPC boundary and local online-session cleanup.
 */
UCLASS(ClassGroup = (PlayerController))
class LABPROJECT_API UControllerSessionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UControllerSessionComponent();

	bool RequestExitMatchToTitle();
	void TravelToTitleWithGameResult(
		const FGameResultPresentationData& GameResultData,
		const FString& TitleMapName) const;
	void TravelToTitleWithoutGameResult(
		const FString& TitleMapName) const;

private:
	APdPlayerController* GetPdController() const;
	bool CanRequestExitMatchToTitle() const;
	void DestroySessionAndTravelToTitle(
		const FString& TitleMapName) const;
};
