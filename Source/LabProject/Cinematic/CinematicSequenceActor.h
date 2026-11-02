#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceActor.h"
#include "CinematicSequenceActor.generated.h"

/**
 * 서버가 재생하면 모든 클라이언트가 같은 시점으로 따라 재생하는 연출 시퀀스.
 *
 * 시퀀스는 이 액터의 위치와 방향을 원점으로 재생하므로, 연출을 보여 줄 자리에 액터를 만들면 된다.
 * 재생하는 동안 각 화면의 로컬 플레이어는 연출 상태가 되어(UControllerPresentationComponent) HUD가 게임 입력과 HUD를 막는다.
 * 재생 중에 들어온 클라이언트는 복제된 재생 위치부터 이어 본다.
 */
UCLASS(NotPlaceable)
class LABPROJECT_API ACinematicSequenceActor : public ALevelSequenceActor
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Public API ------------------------------------------------------------------------------------------------------
	ACinematicSequenceActor(const FObjectInitializer& ObjectInitializer);

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandlePlaybackChanged();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void SetLocalPlayersInCinematic(bool bInCinematic) const;
};
