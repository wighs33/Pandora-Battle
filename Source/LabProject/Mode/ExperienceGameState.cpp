#include "Mode/ExperienceGameState.h"

#include "Experience/ExperienceManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExperienceGameState)

AExperienceGameState::AExperienceGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ExperienceManagerComponent = CreateDefaultSubobject<UExperienceManagerComponent>(TEXT("ExperienceManagerComponent"));
}
