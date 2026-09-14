#include "Definition/AbilitySystem/SkillTypes.h"

#include "HAL/IConsoleManager.h"

namespace
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	TAutoConsoleVariable<int32> CVarSkillDebugDrawing(
		TEXT("lab.Skill.DebugDraw"),
		1,
		TEXT("Enables skill debug drawing for data assets whose individual debug flag is enabled.\n")
		TEXT("0: disabled, 1: enabled (default)"),
		ECVF_Cheat);
#endif
}

bool LabSkillDebug::IsDrawingEnabled()
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	return false;
#else
	return CVarSkillDebugDrawing.GetValueOnAnyThread() != 0;
#endif
}
