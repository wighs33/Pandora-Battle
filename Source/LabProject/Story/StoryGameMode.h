#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StoryGameMode.generated.h"

/** 싱글 스토리 맵의 게임 모드. 인물은 감독(AStoryDirector)이 세우므로 기본 Pawn이 없다. */
UCLASS()
class LABPROJECT_API AStoryGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	AStoryGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
