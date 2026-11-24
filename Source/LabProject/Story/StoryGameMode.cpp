#include "Story/StoryGameMode.h"

#include "UI/Story/StoryHUD.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StoryGameMode)

AStoryGameMode::AStoryGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultPawnClass = nullptr;
	HUDClass = AStoryHUD::StaticClass();
}
