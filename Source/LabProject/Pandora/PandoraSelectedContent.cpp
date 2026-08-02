#include "Pandora/PandoraSelectedContent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraSelectedContent)

void FPandoraSelectedContent::Reset()
{
	AbilityHandles.Reset();
	RuntimeContexts.Reset();
}

void FPandoraSelectedContent::Capture(FPandoraSkillBindingResult&& BindingResult)
{
	AbilityHandles = MoveTemp(BindingResult.AbilityHandles);
	RuntimeContexts = MoveTemp(BindingResult.RuntimeContexts);
}
