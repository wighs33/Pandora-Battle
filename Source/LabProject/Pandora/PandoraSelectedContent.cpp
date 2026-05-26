#include "Pandora/PandoraSelectedContent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraSelectedContent)

void FPandoraSelectedContent::Reset()
{
	AbilityHandles.Reset();
	EffectHandles.Reset();
	RuntimeContexts.Reset();
}

void FPandoraSelectedContent::Capture(FPandoraSkillBindingResult&& BindingResult)
{
	AbilityHandles = MoveTemp(BindingResult.AbilityHandles);
	EffectHandles = MoveTemp(BindingResult.EffectHandles);
	RuntimeContexts = MoveTemp(BindingResult.RuntimeContexts);
}
