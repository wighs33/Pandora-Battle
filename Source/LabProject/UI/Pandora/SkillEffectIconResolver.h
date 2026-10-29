#pragma once

#include "CoreMinimal.h"

class UImage;
class USkillDefinition;

namespace SkillEffectIconResolver
{
	void ApplySkillEffectIcon(
		const UObject* WorldContextObject,
		const USkillDefinition* Skill,
		UImage* ImageWidget);
}
