#include "Story/Steps/StoryStageSteps.h"

#include "Story/StoryDirector.h"
#include "Story/StoryPortal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StoryStageSteps)

TUniquePtr<FStoryTask> UStoryShotStep::Start(AStoryDirector& Director) const
{
	Director.PlayShot(Shot);
	return bWaitForBlend && Shot.BlendSeconds > 0.f ? MakeUnique<FStoryWaitTask>(Shot.BlendSeconds) : nullptr;
}

TUniquePtr<FStoryTask> UStoryWaitStep::Start(AStoryDirector& Director) const
{
	return MakeUnique<FStoryWaitTask>(Seconds);
}

TUniquePtr<FStoryTask> UStoryShakeStep::Start(AStoryDirector& Director) const
{
	Director.Shake(Strength, Seconds);
	return nullptr;
}

TUniquePtr<FStoryTask> UStoryPortalStep::Start(AStoryDirector& Director) const
{
	if (AStoryPortal* Portal = Director.GetPortal())
	{
		if (bOpen)
		{
			Portal->Open();
		}
		else
		{
			Portal->Close();
		}
	}
	return nullptr;
}

TUniquePtr<FStoryTask> UStorySoundStep::Start(AStoryDirector& Director) const
{
	Director.PlaySound(Sound);
	return nullptr;
}
